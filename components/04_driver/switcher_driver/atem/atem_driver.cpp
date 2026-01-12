/**
 * @file AtemDriver.cpp
 * @brief ATEM UDP Driver 구현
 *
 * 역할: ATEM 프로토콜 로직 구현
 */

#include "atem_driver.h"
#include "t_log.h"

// =============================================================================
// Tally 설정 (하드코딩)
// =============================================================================

#define TALLY_MAX_CHANNELS      20       ///< 최대 Tally 채널 수

// Hello 응답 타임아웃 (AtemProtocol.h에 없는 값만 추가)
#define ATEM_HELLO_RESPONSE_TIMEOUT_MS  10000   ///< Hello 응답 타임아웃 (10초)

#include <sys/socket.h>

// ============================================================================
// 태그
// ============================================================================

static const char* TAG = "04_Atem";
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <stdio.h>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================================
// 생성자/소멸자
// ============================================================================

AtemDriver::AtemDriver(const AtemConfig& config)
    : config_(config)
    , state_()
    , conn_state_(CONNECTION_STATE_DISCONNECTED)
    , sock_fd_(-1)
    , tally_callback_()
    , connection_callback_()
    , connect_attempt_time_(0)
    , last_hello_time_(0)
    , lastNetworkRestart_(0)
    , needsNetworkRestart_(false)
{
    rx_buffer_.fill(0);
    tx_buffer_.fill(0);
    cached_packed_.data = nullptr;
    cached_packed_.data_size = 0;
    cached_packed_.channel_count = 0;

    // 디버깅: 드라이버 생성 로그 (이름, IP, 인스턴스 주소)
    T_LOGD(TAG, "Driver created: %s@%s (this=%p)", config_.name.c_str(), config_.ip.c_str(), this);
}

AtemDriver::~AtemDriver() {
    // 디버깅: 드라이버 소멸 로그
    T_LOGD(TAG, "Driver destroyed: %s@%s (this=%p, session=0x%04X)",
             config_.name.c_str(), config_.ip.c_str(), this, state_.session_id);
    disconnect();
    packed_data_cleanup(&cached_packed_);
}

// ============================================================================
// 연결 관리
// ============================================================================

bool AtemDriver::initialize() {
    if (sock_fd_ >= 0) {
        T_LOGD(TAG, "ok:already");
        return true;
    }

    // UDP 소켓 생성
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd_ < 0) {
        T_LOGE(TAG, "fail:socket:%d", errno);
        return false;
    }

    // SO_REUSEADDR 설정
    int opt = 1;
    setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 소켓 수신 버퍼 크기 증가 (64KB) - 고속 패킷 수신용
    int rcvbuf_size = 64 * 1024;
    setsockopt(sock_fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size));

    // 논블로킹 모드 설정
    int flags = fcntl(sock_fd_, F_GETFL, 0);
    fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

    // 로컬 포트 바인드 (인터페이스 선택 지원)
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = 0;  // 자동 할당

    // 서비스 레이어에서 설정한 로컬 바인딩 IP 사용
    if (!config_.local_bind_ip.empty()) {
        local_addr.sin_addr.s_addr = inet_addr(config_.local_bind_ip.c_str());
        T_LOGD(TAG, "bind:%s", config_.local_bind_ip.c_str());
    } else {
        local_addr.sin_addr.s_addr = INADDR_ANY;
        T_LOGD(TAG, "bind:auto");
    }

    if (bind(sock_fd_, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        T_LOGE(TAG, "fail:bind:%d", errno);
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
    }

    T_LOGD(TAG, "init:ok:fd=%d", sock_fd_);
    return true;
}

void AtemDriver::connect() {
    if (conn_state_ != CONNECTION_STATE_DISCONNECTED) {
        T_LOGW(TAG, "connect:busy");
        return;
    }

    // 소켓이 유효하지 않으면 다시 초기화
    if (sock_fd_ < 0) {
        if (!initialize()) {
            T_LOGE(TAG, "fail:reinit");
            return;
        }
    }

    T_LOGD(TAG, "connect:%s:%d", config_.ip.c_str(), config_.port);

    // 상태 초기화
    state_ = AtemState();
    setConnectionState(CONNECTION_STATE_CONNECTING);

    // 연결 시도 시작 시간 기록
    connect_attempt_time_ = getMillis();
    last_hello_time_ = 0;  // 즉시 Hello 전송하도록

    // Hello 패킷 생성 (정확히 20바이트)
    uint8_t hello[ATEM_HELLO_PACKET_SIZE];
    createHelloPacket(hello);

    T_LOGD(TAG, "hello:tx");

    // 전송
    if (sendPacket(hello, ATEM_HELLO_PACKET_SIZE) < 0) {
        T_LOGE(TAG, "fail:hello_tx");
        disconnect();  // 소켓 닫기 (메모리 누수 방지)
        return;
    }
    last_hello_time_ = getMillis();
}

void AtemDriver::disconnect() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }

    bool was_connected = state_.connected;
    state_.connected = false;
    state_.initialized = false;
    setConnectionState(CONNECTION_STATE_DISCONNECTED);

    if (was_connected) {
        T_LOGD(TAG, "disconnect");
    }
}

// ============================================================================
// 루프 처리
// ============================================================================

int AtemDriver::loop() {
    if (!state_.connected && conn_state_ != CONNECTION_STATE_CONNECTING) {
        return -1;
    }

    int processed = 0;
    uint32_t now = getMillis();

    // 수신 간격 추적
    static uint32_t last_packet_recv_ms = 0;
    static uint32_t packet_count = 0;

    // CONNECTING 상태 처리
    if (conn_state_ == CONNECTION_STATE_CONNECTING) {
        // Hello 응답 타임아웃 체크
        if (now - connect_attempt_time_ > ATEM_HELLO_RESPONSE_TIMEOUT_MS) {
            T_LOGE(TAG, "fail:hello_timeout");
            disconnect();
            return -1;
        }

        // Hello 패킷 재전송 (1초마다)
        if (now - last_hello_time_ > 1000) {
            uint8_t hello[ATEM_HELLO_PACKET_SIZE];
            createHelloPacket(hello);

            T_LOGD(TAG, "hello:retry:%dms", (int)(now - connect_attempt_time_));

            int send_ret = sendPacket(hello, ATEM_HELLO_PACKET_SIZE);
            if (send_ret < 0) {
                // 전송 실패 시 소켓 닫고 연결 해제 (메모리 누수 방지)
                T_LOGE(TAG, "fail:hello_retry");
                disconnect();
                return -1;
            }
            last_hello_time_ = now;
        }
    }

    // 패킷 수신 - UDP 버퍼가 빌 때까지 모두 처리
    while (true) {
        struct sockaddr_in remote_addr;
        socklen_t addr_len = sizeof(remote_addr);

        ssize_t received = recvfrom(sock_fd_, rx_buffer_.data(), rx_buffer_.size(),
                                     0, (struct sockaddr*)&remote_addr, &addr_len);

        if (received > 0) {
            // 수신 간격 로깅
            uint32_t recv_time = getMillis();
            if (last_packet_recv_ms > 0) {
                uint32_t interval = recv_time - last_packet_recv_ms;
                packet_count++;

                // 10회마다 또는 긴 간격일 때 로그 (DEBUG 레벨)
                if (packet_count % 50 == 0 || interval > 500) {
                    T_LOGD(TAG, "rx:#%u,%dms,%db",
                             packet_count, interval, static_cast<int>(received));
                }
            }
            last_packet_recv_ms = recv_time;

            // 패킷 처리
            int result = processPacket(rx_buffer_.data(), static_cast<uint16_t>(received));
            if (result == 0) {
                // 유효한 패킷 처리 성공 - last_contact 업데이트
                state_.last_contact_ms = getMillis();
                processed++;
            }
        } else {
            // 더 이상 패킷 없음
            break;
        }
    }

    // 타임아웃 체크 (5초)
    now = getMillis();
    if (state_.connected &&
        now - state_.last_contact_ms > ATEM_MAX_SILENCE_TIME_MS) {
        T_LOGW(TAG, "timeout:%dms", (int)(now - state_.last_contact_ms));

        // 연결된 상태에서 타임아웃 = 네트워크 스택 오류 가능성
        uint32_t now_ms = getMillis();
        if (now_ms - lastNetworkRestart_ > RESTART_COOLDOWN_MS) {
            T_LOGE(TAG, "err:net_stack");
            lastNetworkRestart_ = now_ms;
            needsNetworkRestart_ = true;
        }

        disconnect();
        return -1;
    }

    // Keepalive 전송 (1초마다)
    if (state_.initialized &&
        now - state_.last_keepalive_ms > ATEM_KEEPALIVE_INTERVAL_MS) {
        uint8_t keepalive[ATEM_ACK_PACKET_SIZE];
        createKeepalivePacket(keepalive);
        T_LOGD(TAG, "[%s] ka:%dms", config_.name.c_str(), (int)(now - state_.last_keepalive_ms));
        sendPacket(keepalive, ATEM_ACK_PACKET_SIZE);
        state_.last_keepalive_ms = now;
    }

    return processed;
}

// ============================================================================
// 상태 조회
// ============================================================================

connection_state_t AtemDriver::getConnectionState() const {
    return conn_state_;
}

bool AtemDriver::isConnected() const {
    return state_.connected;
}

bool AtemDriver::isInitialized() const {
    return state_.initialized;
}

packed_data_t AtemDriver::getPackedTally() const {
    // 필터링된 채널 수 결정
    uint8_t channel_count = state_.num_cameras;

    // num_cameras가 0이거나 camera_limit가 0이면 기본값 20 사용
    if (channel_count == 0) {
        channel_count = (config_.camera_limit > 0) ? config_.camera_limit : TALLY_MAX_CHANNELS;
    }

    // camera_limit 적용
    if (config_.camera_limit > 0 && channel_count > config_.camera_limit) {
        channel_count = config_.camera_limit;
    }

    // 최대 20채널 제한
    if (channel_count > TALLY_MAX_CHANNELS) {
        channel_count = TALLY_MAX_CHANNELS;
    }

    // 캐시된 데이터와 채널 수가 다르면 재생성
    if (cached_packed_.channel_count != channel_count) {
        packed_data_cleanup(&cached_packed_);
        packed_data_init(&cached_packed_, channel_count);
    }

    // 64비트 값에서 비트 추출하여 PackedData로 변환
    for (uint8_t i = 0; i < channel_count; i++) {
        uint8_t flags = (state_.tally_packed >> (i * 2)) & 0x03;
        packed_data_set_channel(&cached_packed_, i + 1, flags);
    }

    return cached_packed_;
}

uint8_t AtemDriver::getCameraCount() const {
    return state_.num_cameras;
}

uint32_t AtemDriver::getLastUpdateTime() const {
    return state_.last_contact_ms;
}

tally_status_t AtemDriver::getChannelTally(uint8_t channel) const {
    if (channel < 1 || channel > state_.num_cameras) {
        return TALLY_STATUS_OFF;
    }

    uint8_t flags = (state_.tally_packed >> ((channel - 1) * 2)) & 0x03;
    return static_cast<tally_status_t>(flags);
}

// ============================================================================
// 제어 명령
// ============================================================================

void AtemDriver::cut() {
    if (!state_.initialized) {
        T_LOGW(TAG, "not_init:cut");
        return;
    }

    uint8_t data[4] = { 0, 0, 0, 0 };  // ME 0
    createCommandPacket(ATEM_CMD_CUT, data, sizeof(data));
}

void AtemDriver::autoTransition() {
    if (!state_.initialized) {
        T_LOGW(TAG, "not_init:auto");
        return;
    }

    uint8_t data[4] = { 0, 0, 0, 0 };  // ME 0
    createCommandPacket(ATEM_CMD_AUTO, data, sizeof(data));
}

void AtemDriver::setPreview(uint16_t source_id) {
    if (!state_.initialized) {
        T_LOGW(TAG, "not_init:prev");
        return;
    }

    uint8_t data[4];
    data[0] = 0;  // ME 0
    data[1] = 0;
    AtemProtocol::setU16(data, 2, source_id);

    createCommandPacket(ATEM_CMD_CHANGE_PREVIEW, data, sizeof(data));
}

// ============================================================================
// 콜백 설정
// ============================================================================

void AtemDriver::setTallyCallback(std::function<void()> callback) {
    tally_callback_ = callback;
}

void AtemDriver::setConnectionCallback(std::function<void(connection_state_t)> callback) {
    connection_callback_ = callback;
}

// ============================================================================
// 패킷 생성
// ============================================================================

void AtemDriver::createHelloPacket(uint8_t* buf) {
    /*
     * ATEM Hello 패킷 (정확히 20바이트)
     *
     * 헤더: (flags << 11) | (length & 0x07FF)
     * flags = 0x02 (HELLO_PACKET)
     * (0x02 << 11) | (20 & 0x07FF) = 0x1014
     */
    memset(buf, 0, ATEM_HELLO_PACKET_SIZE);

    // 헤더 워드 생성
    uint16_t header_word = (ATEM_FLAG_HELLO << 11) | (ATEM_HELLO_PACKET_SIZE & 0x07FF);
    AtemProtocol::setU16(buf, 0, header_word);

    // Session ID: 초기값 0 (byte 2-3)
    AtemProtocol::setU16(buf, 2, 0x0000);

    // ACK ID: 초기값 0 (byte 4-5)
    AtemProtocol::setU16(buf, 4, 0x0000);

    // 추가 플래그
    buf[9] = 0x3a;
    buf[12] = 0x01;
}

void AtemDriver::createAckPacket(uint8_t* buf, uint16_t session_id, uint16_t packet_id) {
    /*
     * ACK 패킷 (정확히 12바이트)
     *
     * 헤더: (flags << 11) | (length & 0x07FF)
     * flags = 0x10 (ACK)
     * (0x10 << 11) | (12 & 0x07FF) = 0x800C
     */
    memset(buf, 0, ATEM_ACK_PACKET_SIZE);

    // 헤더 워드 생성
    uint16_t header_word = (ATEM_FLAG_ACK << 11) | (ATEM_ACK_PACKET_SIZE & 0x07FF);
    AtemProtocol::setU16(buf, 0, header_word);

    // Session ID (byte 2-3)
    AtemProtocol::setU16(buf, 2, session_id);

    // ACK ID (byte 4-5) - 확인할 패킷 ID
    AtemProtocol::setU16(buf, 4, packet_id);
}

void AtemDriver::createKeepalivePacket(uint8_t* buf) {
    /*
     * Keepalive 패킷 (정확히 12바이트) - ACK 형태
     */
    memset(buf, 0, ATEM_ACK_PACKET_SIZE);

    // 헤더 워드 생성 (ACK 패킷과 동일)
    uint16_t header_word = (ATEM_FLAG_ACK << 11) | (ATEM_ACK_PACKET_SIZE & 0x07FF);
    AtemProtocol::setU16(buf, 0, header_word);

    // Session ID
    AtemProtocol::setU16(buf, 2, state_.session_id);

    // ACK ID - 마지막 받은 패킷 ID
    AtemProtocol::setU16(buf, 4, state_.remote_packet_id);
}

int AtemDriver::createCommandPacket(const char* cmd, const uint8_t* data, uint16_t length) {
    /*
     * 명령 패킷 구성
     */
    uint16_t cmd_length = ATEM_CMD_HEADER_LENGTH + length;
    uint16_t packet_length = ATEM_HEADER_LENGTH + cmd_length;

    if (packet_length > tx_buffer_.size()) {
        T_LOGE(TAG, "fail:pkt_size");
        return -1;
    }

    memset(tx_buffer_.data(), 0, packet_length);

    // 헤더 구성: flags = ACK_REQUEST (0x01) - 응답 요청
    uint16_t header_word = (ATEM_FLAG_ACK_REQUEST << 11) | (packet_length & 0x07FF);
    AtemProtocol::setU16(tx_buffer_.data(), 0, header_word);
    AtemProtocol::setU16(tx_buffer_.data(), 2, state_.session_id);
    AtemProtocol::setU16(tx_buffer_.data(), 4, 0);  // ACK ID

    // Packet ID 증가
    state_.local_packet_id++;
    AtemProtocol::setU16(tx_buffer_.data(), 10, state_.local_packet_id);

    // 명령 헤더
    AtemProtocol::setU16(tx_buffer_.data(), 12, cmd_length);
    AtemProtocol::setCommand(tx_buffer_.data(), 16, cmd);

    // 명령 데이터
    if (length > 0) {
        memcpy(tx_buffer_.data() + 20, data, length);
    }

    return sendPacket(tx_buffer_.data(), packet_length);
}

// ============================================================================
// 패킷 처리
// ============================================================================

int AtemDriver::processPacket(const uint8_t* data, uint16_t length) {
    if (length < ATEM_HEADER_LENGTH) {
        return -1;  // 패킷 길이 부족
    }

    /*
     * 헤더 파싱
     *
     * 첫 2바이트: (flags << 11) | (length & 0x07FF)
     * flags = (header_word >> 11) & 0x1F
     * length = header_word & 0x07FF
     */
    uint16_t header_word = AtemProtocol::getU16(data, 0);
    uint8_t flags = (header_word >> 11) & 0x1F;
    uint16_t session_id = AtemProtocol::getU16(data, 2);
    uint16_t remote_packet_id = AtemProtocol::getU16(data, 10);

    // Hello 응답 처리 (연결 중일 때만)
    if ((flags & ATEM_FLAG_HELLO) && conn_state_ == CONNECTION_STATE_CONNECTING) {
        T_LOGD(TAG, "hello:rx:0x%04X,%d", session_id, remote_packet_id);

        // ACK 전송
        uint8_t ack[ATEM_ACK_PACKET_SIZE];
        createAckPacket(ack, session_id, remote_packet_id);
        sendPacket(ack, ATEM_ACK_PACKET_SIZE);

        state_.connected = true;
        state_.last_contact_ms = getMillis();

        T_LOGD(TAG, "hello:ack:tx");
        setConnectionState(CONNECTION_STATE_CONNECTED);

        return 0;
    }

    // Session ID 업데이트 (첫 번째 유효한 Session ID 저장)
    if (state_.session_id == 0 && session_id != 0) {
        state_.session_id = session_id;
        T_LOGD(TAG, "Session ID set: 0x%04X", session_id);
    }

    // Session ID 검증 (설정 후)
    if (state_.session_id != 0 && session_id != 0 &&
        session_id != state_.session_id) {
        T_LOGW(TAG, "Session ID mismatch: expected=0x%04X, got=0x%04X (packet rejected)",
                 state_.session_id, session_id);
        return -1;  // Session ID 불일치 - 패킷 거부
    }

    /*
     * 중복/재전송 패킷 체크
     * - 초기화 전: 모든 패킷 파싱 (InCm이 resend로 올 수 있음)
     * - 초기화 후: 중복/재전송은 ACK만 보내고 파싱 안 함
     */
    bool is_resend = (flags & ATEM_FLAG_RESEND) != 0;
    bool skip_parsing = false;

    if (state_.initialized && remote_packet_id != 0) {
        if (remote_packet_id <= state_.last_received_packet_id) {
            skip_parsing = true;
        } else {
            // 새 패킷 - 즉시 ID 갱신 (다음 중복 체크를 위해)
            state_.last_received_packet_id = remote_packet_id;
            // 초기화 후 resend는 파싱 안 함
            if (is_resend) {
                skip_parsing = true;
            }
        }
    }

    // ACK 필요 여부 확인 및 전송
    if ((flags & ATEM_FLAG_ACK_REQUEST) && state_.session_id != 0) {
        uint8_t ack[ATEM_ACK_PACKET_SIZE];
        createAckPacket(ack, state_.session_id, remote_packet_id);
        sendPacket(ack, ATEM_ACK_PACKET_SIZE);
    }

    // 파싱 스킵
    if (skip_parsing) {
        return 0;  // ACK는 보냈으므로 성공으로 처리
    }

    // 패킷 ID 업데이트 (keepalive용)
    if (remote_packet_id > state_.remote_packet_id) {
        state_.remote_packet_id = remote_packet_id;
    }

    // 명령 추출 및 처리
    if (length > ATEM_HEADER_LENGTH) {
        parseCommands(data, length);
    }

    return 0;  // 성공
}

void AtemDriver::parseCommands(const uint8_t* data, uint16_t length) {
    uint16_t offset = ATEM_HEADER_LENGTH;

    while (offset + ATEM_CMD_HEADER_LENGTH <= length) {
        // 명령 길이 (2바이트)
        uint16_t cmd_length = AtemProtocol::getU16(data, offset);
        if (cmd_length < ATEM_CMD_HEADER_LENGTH) {
            break;  // 잘못된 명령
        }

        if (offset + cmd_length > length) {
            break;  // 패킷 범위 초과
        }

        // 명령 이름 (4바이트, offset+4~+7)
        char cmd_name[5];
        cmd_name[0] = static_cast<char>(data[offset + 4]);
        cmd_name[1] = static_cast<char>(data[offset + 5]);
        cmd_name[2] = static_cast<char>(data[offset + 6]);
        cmd_name[3] = static_cast<char>(data[offset + 7]);
        cmd_name[4] = '\0';

        // 명령 데이터 (헤더 이후)
        const uint8_t* cmd_data = data + offset + ATEM_CMD_HEADER_LENGTH;
        uint16_t cmd_data_length = cmd_length - ATEM_CMD_HEADER_LENGTH;

        // 명령 처리
        handleCommand(cmd_name, cmd_data, cmd_data_length);

        offset += cmd_length;
    }
}

void AtemDriver::handleCommand(const char* cmd_name, const uint8_t* cmd_data, uint16_t cmd_length) {
    // _ver: 프로토콜 버전
    if (AtemProtocol::cmdEquals(cmd_name, ATEM_CMD_VERSION)) {
        if (cmd_length >= 4) {
            state_.protocol_major = AtemProtocol::getU16(cmd_data, 0);
            state_.protocol_minor = AtemProtocol::getU16(cmd_data, 2);
        }
    }
    // _pin: 제품명
    else if (AtemProtocol::cmdEquals(cmd_name, ATEM_CMD_PRODUCT_ID)) {
        if (cmd_length > 0) {
            uint16_t max_len = std::min(cmd_length, static_cast<uint16_t>(sizeof(state_.product_name) - 1));
            memcpy(state_.product_name, cmd_data, max_len);
            state_.product_name[max_len] = '\0';
        }
    }
    // _top: 토폴로지
    else if (AtemProtocol::cmdEquals(cmd_name, ATEM_CMD_TOPOLOGY)) {
        if (cmd_length >= 10) {
            state_.num_mes = cmd_data[0];
            state_.num_sources = cmd_data[1];
            state_.num_dsks = cmd_data[5];
            state_.num_supersources = cmd_data[6];
            state_.topology_received = true;

            printTopology();
        }
    }
    // _TlC: Tally Config (카메라 수)
    else if (AtemProtocol::cmdEquals(cmd_name, ATEM_CMD_TALLY_CONFIG)) {
        if (cmd_length >= 5) {
            state_.num_cameras = cmd_data[4];
            state_.tally_config_received = true;

            T_LOGD(TAG, "camera count: %d", state_.num_cameras);
        }
    }
    // PrgI: Program 입력
    else if (AtemProtocol::cmdEquals(cmd_name, ATEM_CMD_PROGRAM_INPUT)) {
        if (cmd_length >= 4) {
            state_.program_input = AtemProtocol::getU16(cmd_data, 2);
        }
    }
    // PrvI: Preview 입력
    else if (AtemProtocol::cmdEquals(cmd_name, ATEM_CMD_PREVIEW_INPUT)) {
        if (cmd_length >= 4) {
            state_.preview_input = AtemProtocol::getU16(cmd_data, 2);
        }
    }
    // TlIn: Tally By Index
    else if (AtemProtocol::cmdEquals(cmd_name, ATEM_CMD_TALLY_INDEX)) {
        handleTallyByIndex(cmd_data, cmd_length);
    }
    // InCm: 초기화 완료
    else if (AtemProtocol::cmdEquals(cmd_name, ATEM_CMD_INIT_COMPLETE)) {
        if (!state_.initialized) {
            state_.initialized = true;
            T_LOGD(TAG, "[%s] init complete", config_.name.c_str());
            setConnectionState(CONNECTION_STATE_READY);
        }
    }
}

void AtemDriver::handleTallyByIndex(const uint8_t* data, uint16_t length) {
    const uint16_t TLIN_HEADER_SIZE = 2;

    if (length < TLIN_HEADER_SIZE) {
        return;  // 헤더도 없으면 무시
    }

    // 헤더에서 소스 수 읽기 (Big Endian)
    uint16_t source_count = AtemProtocol::getU16(data, 0);
    uint16_t tally_count = length - TLIN_HEADER_SIZE;

    // 실제 Tally 데이터 시작 위치
    const uint8_t* tally_data = data + TLIN_HEADER_SIZE;

    // 채널 수 결정: 헤더의 source_count와 실제 데이터 길이 중 작은 값
    uint16_t count = std::min({source_count, tally_count, static_cast<uint16_t>(20)});

    // 실제 탈리 신호가 있는 채널만 추적
    std::vector<uint16_t> program_channels;
    std::vector<uint16_t> preview_channels;

    // packed 데이터 직접 생성
    state_.tally_packed = 0;  // 초기화

    // 최대 처리할 채널 수 결정
    uint16_t process_count = count;

    // camera_limit 적용
    if (config_.camera_limit > 0 && process_count > config_.camera_limit) {
        process_count = config_.camera_limit;
    }

    // 실제 카메라 수 적용
    if (state_.num_cameras > 0 && process_count > state_.num_cameras) {
        process_count = state_.num_cameras;
    }

    for (uint16_t i = 0; i < process_count; i++) {
        uint16_t channel_num = i + 1;
        uint8_t raw_flags = tally_data[i] & 0x03;

        // packed 데이터에 직접 설정
        if (raw_flags != 0) {
            uint8_t channel_index = channel_num - 1;  // 0-based
            uint8_t shift = channel_index * 2;
            state_.tally_packed |= (static_cast<uint64_t>(raw_flags) << shift);

            tally_status_t status = static_cast<tally_status_t>(raw_flags);
            if (status == TALLY_STATUS_PROGRAM || status == TALLY_STATUS_BOTH) {
                program_channels.push_back(channel_num);
            }
            if (status == TALLY_STATUS_PREVIEW || status == TALLY_STATUS_BOTH) {
                preview_channels.push_back(channel_num);
            }
        }
    }

    // PackedTally로 변환하여 실제 바이트 배열 출력
    packed_data_t current = getPackedTally();

    // hex 문자열 생성
    char hex_str[64] = "";
    for (uint8_t i = 0; i < current.data_size && i < 10; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02X", current.data[i]);
        strcat(hex_str, buf);
        if (i < current.data_size - 1) strcat(hex_str, " ");
    }

    T_LOGD(TAG, "Tally: [%s] (%d channels)", hex_str, current.channel_count);

    // Program/Preview 카메라 목록 출력
    if (!program_channels.empty() || !preview_channels.empty()) {
        char program_list[64] = "";
        char preview_list[64] = "";

        for (size_t i = 0; i < program_channels.size(); i++) {
            if (i > 0) strcat(program_list, ",");
            char num[8];
            sprintf(num, "%d", program_channels[i]);
            strcat(program_list, num);
        }

        for (size_t i = 0; i < preview_channels.size(); i++) {
            if (i > 0) strcat(preview_list, ",");
            char num[8];
            sprintf(num, "%d", preview_channels[i]);
            strcat(preview_list, num);
        }

        T_LOGD(TAG, "Tally: PGM[%s] PVW[%s]",
                 program_channels.empty() ? "-" : program_list,
                 preview_channels.empty() ? "-" : preview_list);
    }

    // Tally 콜백 호출
    // 드라이버 레벨에서 중복 방지하지 않음 (SwitcherService에서 처리)
    // 모든 TlIn 패킷을 상위 계층으로 전달하여 변경 누락 방지
    if (state_.tally_packed != 0 && tally_callback_) {
        tally_callback_();
    }
}

void AtemDriver::updateTallyPacked() {
    // 이미 handleTallyByIndex에서 처리됨
}

void AtemDriver::printTopology() {
    if (!state_.topology_received || !state_.tally_config_received) {
        return;
    }

    T_LOGD(TAG, "top:%s,%d.%d,me:%d,src:%d,cam:%d,dsk:%d,ss:%d",
            state_.product_name, state_.protocol_major, state_.protocol_minor,
            state_.num_mes, state_.num_sources, state_.num_cameras,
            state_.num_dsks, state_.num_supersources);
}

// ============================================================================
// 네트워크 유틸리티
// ============================================================================

int AtemDriver::sendPacket(const uint8_t* data, uint16_t length) {
    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.ip.c_str(), &remote_addr.sin_addr);

    ssize_t sent = sendto(sock_fd_, data, length, 0,
                          (struct sockaddr*)&remote_addr, sizeof(remote_addr));

    if (sent != length) {
        T_LOGE(TAG, "fail:tx:%d/%d", (int)sent, length);
        return -1;
    }

    return 0;
}

uint32_t AtemDriver::getMillis() const {
    return static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void AtemDriver::setConnectionState(connection_state_t new_state) {
    if (conn_state_ != new_state) {
        conn_state_ = new_state;

        const char* state_name = connection_state_to_string(new_state);
        T_LOGD(TAG, "[%s] state:%s", config_.name.c_str(), state_name);

        if (connection_callback_) {
            connection_callback_(new_state);
        }
    }
}
