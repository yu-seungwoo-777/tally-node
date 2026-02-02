/**
 * @file VmixDriver.cpp
 * @brief vMix TCP Driver 구현
 *
 * 역할: vMix XML 프로토콜 로직 구현
 */

#include "vmix_driver.h"
#include "t_log.h"
#include "esp_heap_caps.h"
#include <sys/socket.h>
#include <sys/poll.h>

// ============================================================================
// 태그
// ============================================================================

static const char* TAG = "04_Vmix";
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <stdio.h>
#include <sstream>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================================
// 생성자/소멸자
// ============================================================================

VmixDriver::VmixDriver(const VmixConfig& config)
    : config_(config)
    , state_()
    , conn_state_(CONNECTION_STATE_DISCONNECTED)
    , sock_fd_(-1)
    , rx_buffer_(nullptr)
    , cached_packed_(TALLY_MAX_CHANNELS)  // RAII 자동 초기화
    , tally_callback_()
    , connection_callback_()
    , connect_attempt_time_(0)
    , reconnect_retry_count_(0)
    , reconnect_backoff_ms_(0)
    , needs_reconnect_delay_(false)
    , last_disconnect_time_(0)
    , version_requested_(false)
{
    // PSRAM에 rx_buffer 할당 (4KB)
    rx_buffer_ = (uint8_t*)heap_caps_malloc(RX_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!rx_buffer_) {
        T_LOGW(TAG, "PSRAM allocation failed, using internal RAM");
        rx_buffer_ = (uint8_t*)malloc(RX_BUFFER_SIZE);
    }
    if (rx_buffer_) {
        memset(rx_buffer_, 0, RX_BUFFER_SIZE);
        T_LOGD(TAG, "rx_buffer allocated: %zu bytes (%s)",
               RX_BUFFER_SIZE,
               heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0 ? "PSRAM" : "Internal RAM");
    }
}

VmixDriver::~VmixDriver() {
    disconnect();
    // rx_buffer 해제
    if (rx_buffer_) {
        heap_caps_free(rx_buffer_);
        rx_buffer_ = nullptr;
    }
    // cached_packed_은 자동 정리 (RAII)
}

// ============================================================================
// 연결 관리
// ============================================================================

bool VmixDriver::initialize() {
    if (sock_fd_ >= 0) {
        T_LOGD(TAG, "ok:already");
        return true;
    }

    // TCP 소켓 생성
    sock_fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd_ < 0) {
        T_LOGE(TAG, "fail:socket:%d", errno);
        return false;
    }

    // P1: TCP Keepalive 활성화
    int keepalive = 1;
    if (setsockopt(sock_fd_, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        T_LOGW(TAG, "fail:keepalive:%d", errno);
        // 계속 진행 (치명적 오류 아님)
    }

#ifdef TCP_KEEPIDLE
    int keepidle = VMIX_KEEPALIVE_IDLE_SEC;
    if (setsockopt(sock_fd_, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
        T_LOGW(TAG, "fail:keepidle:%d", errno);
    }
#endif

#ifdef TCP_KEEPINTVL
    int keepintvl = VMIX_KEEPALIVE_INTERVAL_SEC;
    if (setsockopt(sock_fd_, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
        T_LOGW(TAG, "fail:keepintvl:%d", errno);
    }
#endif

#ifdef TCP_KEEPCNT
    int keepcnt = VMIX_KEEPALIVE_COUNT;
    if (setsockopt(sock_fd_, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
        T_LOGW(TAG, "fail:keepcnt:%d", errno);
    }
#endif

    // 논블로킹 모드 설정
    int flags = fcntl(sock_fd_, F_GETFL, 0);
    fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

    T_LOGD(TAG, "init:ok:fd=%d", sock_fd_);
    return true;
}

void VmixDriver::connect() {
    if (conn_state_ != CONNECTION_STATE_DISCONNECTED) {
        T_LOGW(TAG, "connect:busy");
        return;
    }

    // P3: 재연결 백오프 체크
    if (!shouldAllowReconnect()) {
        return;  // 아직 백오프 대기 시간 중
    }

    // 소켓이 유효하지 않으면 다시 초기화
    // 이전 소켓이 남아있으면 먼저 정리 (LwIP 안정성 확보)
    if (sock_fd_ >= 0) {
        T_LOGW(TAG, "connect:cleaning_old_socket fd=%d", sock_fd_);
        close(sock_fd_);
        sock_fd_ = -1;
    }

    if (!initialize()) {
        T_LOGE(TAG, "fail:reinit");
        updateBackoffOnDisconnect(false);
        return;
    }

    T_LOGD(TAG, "connect:%s:%d fd=%d", config_.ip.c_str(), config_.port, sock_fd_);

    // 연결 시도 시작 시간 기록 (상태 초기화 전에 설정)
    connect_attempt_time_ = getMillis();

    // 상태 초기화
    state_ = VmixState();
    version_requested_ = false;  // 연결 시 VERSION 요청 상태 초기화
    setConnectionState(CONNECTION_STATE_CONNECTING);

    // 서버 주소 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config_.port);
    if (inet_pton(AF_INET, config_.ip.c_str(), &server_addr.sin_addr) <= 0) {
        T_LOGE(TAG, "fail:invalid_ip:%s", config_.ip.c_str());
        disconnect();
        return;
    }

    // 비블로킹 연결 시도
    int result = ::connect(sock_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (result < 0) {
        if (errno == EINPROGRESS) {
            T_LOGD(TAG, "connecting...");
        } else {
            // 상세 오류 로그
            T_LOGE(TAG, "fail:connect:%d fd=%d", errno, sock_fd_);
            disconnect();
        }
    } else {
        // 즉시 연결 성공
        T_LOGD(TAG, "ok");
        state_.connected = true;
        state_.last_update_ms = getMillis();
        setConnectionState(CONNECTION_STATE_READY);

        // 첫 요청은 다음 폴링 사이클로 미룸 (TCP 안정화 대기)

        // P3: 연결 성공 시 백오프 초기화
        resetBackoff();
    }
}

void VmixDriver::disconnect() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }

    bool was_connected = state_.connected;
    state_.connected = false;
    state_.initialized = false;
    state_.last_update_ms = 0;  // 타임아웃 계산을 위해 초기화

    // 연결 시도 시간 초기화 (타임아웃 계산 오류 방지)
    connect_attempt_time_ = 0;

    // P3: 연결이 있었으면 백오프 상태 갱신 (의도적인 종료가 아닌 경우)
    if (was_connected) {
        T_LOGD(TAG, "disconnect");
        last_disconnect_time_ = getMillis();
        needs_reconnect_delay_ = true;
    }

    setConnectionState(CONNECTION_STATE_DISCONNECTED);
}

// ============================================================================
// 루프 처리
// ============================================================================

int VmixDriver::loop() {
    // P3: 백오프 대기 중이면 시간 체크
    if (conn_state_ == CONNECTION_STATE_DISCONNECTED) {
        if (needs_reconnect_delay_ && !shouldAllowReconnect()) {
            return -1;  // 아직 백오프 대기 시간 중
        }
        return -1;  // 완전히 유휴 상태
    }

    int processed = 0;
    uint32_t now = getMillis();

    // CONNECTING 상태 처리
    if (conn_state_ == CONNECTION_STATE_CONNECTING) {
        // 연결 타임아웃 체크
        if (now - connect_attempt_time_ > VMIX_CONNECT_TIMEOUT_MS) {
            T_LOGE(TAG, "fail:timeout");
            disconnect();
            return -1;
        }

        // 연결 완료 확인 (getsockopt + poll로 쓰기 가능 여부 확인)
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sock_fd_, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
            if (error == 0) {
                // TCP 3-way-handshake 완료, 추가로 쓰기 가능 여부 확인
                struct pollfd pfd;
                pfd.fd = sock_fd_;
                pfd.events = POLLOUT;
                pfd.revents = 0;

                int poll_ret = poll(&pfd, 1, 0);  // 비블로킹 확인
                if (poll_ret > 0 && (pfd.revents & POLLOUT)) {
                    // 소켓이 쓰기 가능한 상태
                    state_.connected = true;
                    state_.last_update_ms = now;
                    setConnectionState(CONNECTION_STATE_READY);
                    T_LOGD(TAG, "ok");

                    // P3: 연결 성공 시 백오프 초기화
                    resetBackoff();
                }
                // 아직 쓰기 가능하지 않으면 다음 루프에서 다시 확인
            } else if (error != EINPROGRESS) {
                // 연결 실패
                T_LOGE(TAG, "fail:connect:%d", error);
                disconnect();
                return -1;
            }
        }
    }

    // 데이터 수신
    if (state_.connected) {
        int received = receiveData();
        if (received > 0) {
            processed += received;
        }

        // 타임아웃 체크
        // last_update_ms가 0이면 연결 직후이므로 connect_attempt_time_ 기준으로 체크
        uint32_t last_activity = state_.last_update_ms;
        if (last_activity == 0) {
            last_activity = connect_attempt_time_;
        }

        // connect_attempt_time_이 0이면 아직 연결 시도 전, 타임아웃 체크 스킵
        if (last_activity == 0) {
            // 아직 연결 시도 전, 타임아웃 체크하지 않음
        } else {
            // uint32_t wrap-around 방어: last_activity > now이면 wrap 발생
            uint32_t elapsed;
            if (last_activity > now) {
                // 타이머 wrap-around 발생 (약 49.7일마다)
                // now + (UINT32_MAX - last_activity + 1) 계산
                elapsed = now + (0xFFFFFFFF - last_activity + 1);
            } else {
                elapsed = now - last_activity;
            }

            // 최대 타임아웃 60초으로 제한 (이상치 방어)
            if (elapsed > VMIX_MAX_SILENCE_TIME_MS && elapsed < 60000) {
                T_LOGW(TAG, "timeout:%ums", elapsed);
                disconnect();
                return -1;
            }
        }

        // 주기적 폴링
        // 연결 완료 후 안정화 시간 확보를 위해 connect_attempt_time_ 사용
        static uint32_t last_poll = 0;

        // 연결 직후에는 첫 폴링을 지연 (TCP 안정화 대기)
        uint32_t time_since_ready = now - connect_attempt_time_;
        if (time_since_ready < VMIX_POLLING_INTERVAL_MS) {
            // 아직 안정화 시간이 지나지 않음
            return 0;
        }

        // 연결 직후 첫 폴링 타임스탬프 설정 (이후 정상 폴링)
        if (last_poll == 0 || last_poll < connect_attempt_time_) {
            last_poll = connect_attempt_time_;
        }

        if (now - last_poll > VMIX_POLLING_INTERVAL_MS) {
            // P2: 첫 폴링에서 VERSION 요청 (Tally 전에)
            if (!version_requested_ && !state_.version_received) {
                sendCommand(VMIX_CMD_VERSION);
                T_LOGD(TAG, "version:requested");
                version_requested_ = true;
            }

            sendCommand(VMIX_CMD_TALLY);
            // 폴링 로그 제거 (너무 자주 출력됨)
            last_poll = now;
        }
    }

    return processed;
}

// ============================================================================
// 상태 조회
// ============================================================================

connection_state_t VmixDriver::getConnectionState() const {
    return conn_state_;
}

bool VmixDriver::isConnected() const {
    return state_.connected;
}

bool VmixDriver::isInitialized() const {
    return state_.initialized;
}

packed_data_t VmixDriver::getPackedTally() const {
    // 필터링된 채널 수 결정
    uint8_t channel_count = state_.num_cameras;

    if (config_.camera_limit > 0 && channel_count > config_.camera_limit) {
        channel_count = config_.camera_limit;
    }

    if (channel_count > TALLY_MAX_CHANNELS) {
        channel_count = TALLY_MAX_CHANNELS;
    }

    // 캐시된 데이터와 채널 수가 다르면 재생성 (RAII resize)
    if (cached_packed_.channelCount() != channel_count) {
        cached_packed_.resize(channel_count);
    }

    // 64비트 값에서 비트 추출하여 PackedData로 변환
    for (uint8_t i = 0; i < channel_count; i++) {
        uint8_t flags = (state_.tally_packed >> (i * 2)) & 0x03;
        cached_packed_.setChannel(i + 1, flags);
    }

    return *cached_packed_.get();
}

uint8_t VmixDriver::getCameraCount() const {
    return state_.num_cameras;
}

uint32_t VmixDriver::getLastUpdateTime() const {
    return state_.last_update_ms;
}

tally_status_t VmixDriver::getChannelTally(uint8_t channel) const {
    if (channel < 1 || channel > state_.num_cameras) {
        return TALLY_STATUS_OFF;
    }

    uint8_t flags = (state_.tally_packed >> ((channel - 1) * 2)) & 0x03;
    return static_cast<tally_status_t>(flags);
}

// ============================================================================
// 제어 명령
// ============================================================================

void VmixDriver::cut() {
    if (!state_.connected) {
        T_LOGW(TAG, "not_conn:cut");
        return;
    }
    sendCommand(VMIX_CMD_CUT);
}

void VmixDriver::autoTransition() {
    if (!state_.connected) {
        T_LOGW(TAG, "not_conn:auto");
        return;
    }
    sendCommand(VMIX_CMD_AUTO);
}

void VmixDriver::setPreview(uint16_t source_id) {
    if (!state_.connected) {
        T_LOGW(TAG, "not_conn:prev");
        return;
    }

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "%s %d", VMIX_CMD_PREVIEW, source_id);
    sendCommand(cmd);
}

// ============================================================================
// 콜백 설정
// ============================================================================

void VmixDriver::setTallyCallback(std::function<void()> callback) {
    tally_callback_ = callback;
}

void VmixDriver::setConnectionCallback(std::function<void(connection_state_t)> callback) {
    connection_callback_ = callback;
}

// ============================================================================
// 내부 메서드
// ============================================================================

int VmixDriver::sendCommand(const char* cmd) {
    if (sock_fd_ < 0) {
        return -1;
    }

    std::string command = cmd;
    command += "\r\n";  // vMix는 CRLF로 끝남

    ssize_t sent = send(sock_fd_, command.c_str(), command.length(), 0);

    if (sent < 0) {
        T_LOGE(TAG, "fail:tx:%d", errno);
        return -1;
    }

    return static_cast<int>(sent);
}

int VmixDriver::receiveData() {
    if (sock_fd_ < 0) {
        return -1;
    }

    ssize_t received = recv(sock_fd_, rx_buffer_, RX_BUFFER_SIZE - 1, 0);

    if (received <= 0) {
        if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // P1: 네트워크 장애 감지
            if (errno == ECONNRESET) {
                T_LOGE(TAG, "err:conn_reset");
            } else if (errno == EPIPE) {
                T_LOGE(TAG, "err:broken_pipe");
            } else if (errno == ECONNABORTED) {
                T_LOGE(TAG, "err:conn_aborted");
            } else {
                T_LOGE(TAG, "receive error (errno=%d)", errno);
            }
            disconnect();
        }
        return 0;
    }

    rx_buffer_[received] = '\0';

    // 수신 로그 제거 (너무 자주 출력됨)

    // 데이터를 수신했으면 항상 last_update_ms 갱신 (타임아웃 방지)
    state_.last_update_ms = getMillis();

    // 데이터 파싱 (TALLY 또는 VERSION)
    std::string response(reinterpret_cast<char*>(rx_buffer_), received);
    int parsed = 0;

    // P2: VERSION 응답 확인
    if (response.find("VERSION OK") != std::string::npos) {
        parsed += parseVersionData(response.c_str());
    }

    // TALLY 데이터 파싱
    parsed += parseTallyData(response.c_str());

    return parsed;
}

int VmixDriver::parseTallyData(const char* data) {
    // vMix TCP API TALLY 응답 형식:
    // TALLY OK 0121...\r\n
    // 각 숫자: 0 = off, 1 = program, 2 = preview

    std::string response(data);
    int updates = 0;

    // "TALLY OK " 접두사 찾기
    const char* prefix = "TALLY OK ";
    size_t prefix_len = strlen(prefix);

    size_t tally_pos = response.find(prefix);
    if (tally_pos == std::string::npos) {
        // 다른 형식의 응답 (VERSION OK 등)은 무시
        return 0;
    }

    // 숫자 부분 시작 위치
    size_t data_start = tally_pos + prefix_len;

    // 숫자 부분 추출 (CRLF 또는 끝까지)
    size_t data_end = response.find("\r", data_start);
    if (data_end == std::string::npos) {
        data_end = response.find("\n", data_start);
    }
    if (data_end == std::string::npos) {
        data_end = response.length();
    }

    // 이전 상태와 비교를 위해 저장
    uint64_t old_tally_packed = state_.tally_packed;
    state_.tally_packed = 0;  // 상태 초기화

    // 각 숫자 파싱
    for (size_t i = data_start; i < data_end && (i - data_start) < TALLY_MAX_CHANNELS; i++) {
        char c = response[i];
        uint8_t channel = i - data_start;

        if (c >= '0' && c <= '2') {
            uint8_t tally_value = c - '0';  // 0 = off, 1 = program, 2 = preview

            // tally_packed에 상태 저장 (각 채널 2비트 사용)
            if (tally_value == 1) {
                // Program (비트 0 설정)
                state_.tally_packed |= (1ULL << (channel * 2));
            } else if (tally_value == 2) {
                // Preview (비트 1 설정)
                state_.tally_packed |= (2ULL << (channel * 2));
            }
            // 0인 경우는 이미 초기화됨
        }
    }

    // 채널 수 업데이트
    state_.num_cameras = data_end - data_start;
    if (state_.num_cameras > TALLY_MAX_CHANNELS) {
        state_.num_cameras = TALLY_MAX_CHANNELS;
    }

    // 상태가 변경되었는지 확인
    if (state_.tally_packed != old_tally_packed) {
        updates = 1;

        // Tally 콜백 호출
        if (tally_callback_) {
            tally_callback_();
        }

        T_LOGD(TAG, "tally:%d", state_.num_cameras);
    }

    return updates;
}

// ============================================================================
// P2: VERSION 응답 파싱
// ============================================================================

int VmixDriver::parseVersionData(const char* data) {
    // vMix TCP API VERSION 응답 형식:
    // VERSION OK <version_string>\r\n
    // 예: VERSION OK vMix 26.0.0.23\r\n

    std::string response(data);
    int updates = 0;

    // "VERSION OK " 접두사 찾기
    const char* prefix = "VERSION OK ";
    size_t prefix_len = strlen(prefix);

    size_t version_pos = response.find(prefix);
    if (version_pos == std::string::npos) {
        return 0;
    }

    // 버전 문자열 추출
    size_t data_start = version_pos + prefix_len;
    size_t data_end = response.find("\r", data_start);
    if (data_end == std::string::npos) {
        data_end = response.find("\n", data_start);
    }
    if (data_end == std::string::npos) {
        data_end = response.length();
    }

    std::string version_str = response.substr(data_start, data_end - data_start);

    // 버전 문자열 저장
    if (!version_str.empty()) {
        strncpy(state_.version_string, version_str.c_str(), sizeof(state_.version_string) - 1);
        state_.version_string[sizeof(state_.version_string) - 1] = '\0';
        state_.version_received = true;

        // 초기화 완료 표시
        if (!state_.initialized) {
            state_.initialized = true;
        }

        T_LOGI(TAG, "vmix:%s", state_.version_string);
        updates = 1;
    }

    return updates;
}

void VmixDriver::updateTallyPacked() {
    // parseTallyData에서 처리됨
}

uint32_t VmixDriver::getMillis() const {
    return static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void VmixDriver::setConnectionState(connection_state_t new_state) {
    if (conn_state_ != new_state) {
        conn_state_ = new_state;

        const char* state_name = connection_state_to_string(new_state);
        T_LOGD(TAG, "[%s] state:%s", config_.name.c_str(), state_name);

        if (connection_callback_) {
            connection_callback_(new_state);
        }
    }
}

// ============================================================================
// P3: 재연결 백오프 헬퍼 함수
// ============================================================================

void VmixDriver::resetBackoff() {
    reconnect_retry_count_ = 0;
    reconnect_backoff_ms_ = 0;
    needs_reconnect_delay_ = false;
}

void VmixDriver::updateBackoffOnDisconnect(bool success) {
    if (success) {
        // 연결 성공 후 정상 종료
        resetBackoff();
    } else {
        // 연결 실패 또는 비정상 종료
        reconnect_retry_count_++;

        // 지수 백오프 계산: 1s, 2s, 4s, 8s, 16s, 최대 30s
        if (reconnect_retry_count_ == 1) {
            reconnect_backoff_ms_ = 1000;   // 첫 실패: 1초
        } else {
            uint32_t next_backoff = reconnect_backoff_ms_ * 2;
            if (next_backoff > 30000) {
                next_backoff = 30000;  // 최대 30초
            }
            reconnect_backoff_ms_ = next_backoff;
        }

        last_disconnect_time_ = getMillis();
        needs_reconnect_delay_ = true;

        T_LOGD(TAG, "backoff:%dms (retry:%d)", (int)reconnect_backoff_ms_, reconnect_retry_count_);
    }
}

bool VmixDriver::shouldAllowReconnect() const {
    if (!needs_reconnect_delay_) {
        return true;  // 대기 필요 없음
    }

    uint32_t now = getMillis();
    uint32_t elapsed = now - last_disconnect_time_;

    if (elapsed >= reconnect_backoff_ms_) {
        return true;  // 대기 시간 경과
    }

    return false;  // 아직 대기 중
}
