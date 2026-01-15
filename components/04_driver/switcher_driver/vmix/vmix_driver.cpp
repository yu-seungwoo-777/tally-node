/**
 * @file VmixDriver.cpp
 * @brief vMix TCP Driver 구현
 *
 * 역할: vMix XML 프로토콜 로직 구현
 */

#include "vmix_driver.h"
#include "t_log.h"
#include <sys/socket.h>

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
    , rx_buffer_()
    , cached_packed_(TALLY_MAX_CHANNELS)  // RAII 자동 초기화
    , tally_callback_()
    , connection_callback_()
    , connect_attempt_time_(0)
{
    rx_buffer_.fill(0);
}

VmixDriver::~VmixDriver() {
    disconnect();
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

    // 소켓이 유효하지 않으면 다시 초기화
    if (sock_fd_ < 0) {
        if (!initialize()) {
            T_LOGE(TAG, "fail:reinit");
            return;
        }
    }

    T_LOGD(TAG, "connect:%s:%d", config_.ip.c_str(), config_.port);

    // 상태 초기화
    state_ = VmixState();
    setConnectionState(CONNECTION_STATE_CONNECTING);

    // 연결 시도 시작 시간 기록
    connect_attempt_time_ = getMillis();

    // 서버 주소 설정
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.ip.c_str(), &server_addr.sin_addr);

    // 비블로킹 연결 시도
    int result = ::connect(sock_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (result < 0) {
        if (errno == EINPROGRESS) {
            T_LOGD(TAG, "connecting...");
        } else {
            T_LOGE(TAG, "fail:connect:%d", errno);
            setConnectionState(CONNECTION_STATE_DISCONNECTED);
        }
    } else {
        // 즉시 연결 성공
        T_LOGD(TAG, "ok");
        state_.connected = true;
        state_.last_update_ms = getMillis();
        setConnectionState(CONNECTION_STATE_READY);
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
    setConnectionState(CONNECTION_STATE_DISCONNECTED);

    if (was_connected) {
        T_LOGD(TAG, "disconnect");
    }
}

// ============================================================================
// 루프 처리
// ============================================================================

int VmixDriver::loop() {
    if (conn_state_ == CONNECTION_STATE_DISCONNECTED) {
        return -1;
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

        // 연결 완료 확인 (select/poll 또는 getsockopt)
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sock_fd_, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
            if (error == 0) {
                // 연결 성공
                state_.connected = true;
                state_.last_update_ms = now;
                setConnectionState(CONNECTION_STATE_READY);
                T_LOGD(TAG, "ok");

                // 초기 Tally 요청
                sendCommand(VMIX_CMD_TALLY);
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
        if (now - state_.last_update_ms > VMIX_MAX_SILENCE_TIME_MS) {
            T_LOGW(TAG, "timeout:%dms", (int)(now - state_.last_update_ms));
            disconnect();
            return -1;
        }

        // 주기적 폴링
        static uint32_t last_poll = 0;
        if (now - last_poll > VMIX_POLLING_INTERVAL_MS) {
            sendCommand(VMIX_CMD_TALLY);
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

    ssize_t received = recv(sock_fd_, rx_buffer_.data(), rx_buffer_.size() - 1, 0);

    if (received <= 0) {
        if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            T_LOGE(TAG, "receive error (errno=%d)", errno);
            disconnect();
        }
        return 0;
    }

    rx_buffer_[received] = '\0';

    // XML 데이터 파싱
    std::string xml_data(reinterpret_cast<char*>(rx_buffer_.data()), received);
    int parsed = parseTallyData(xml_data.c_str());

    if (parsed > 0) {
        state_.last_update_ms = getMillis();
    }

    return parsed;
}

int VmixDriver::parseTallyData(const char* xml_data) {
    // vMix XML 형식:
    // <vmix><tally><number>1</number><type>Program</type>...</tally></vmix>

    // 간단한 파싱 (실제로는 XML 파서 사용 권장)
    std::string data(xml_data);
    int updates = 0;

    // Program 상태 찾기
    size_t pos = 0;
    while ((pos = data.find("<type>Program</type>", pos)) != std::string::npos) {
        // 이전에 number 태그 찾기
        size_t num_start = data.rfind("<number>", pos);
        if (num_start != std::string::npos) {
            num_start += 8;  // "<number>" 길이
            size_t num_end = data.find("</number>", num_start);
            if (num_end != std::string::npos) {
                std::string num_str = data.substr(num_start, num_end - num_start);
                int channel = atoi(num_str.c_str());
                if (channel > 0 && channel <= TALLY_MAX_CHANNELS) {
                    uint8_t idx = channel - 1;
                    state_.tally_packed |= (1ULL << (idx * 2));  // Program 비트 설정
                    updates++;
                }
            }
        }
        pos++;
    }

    // Preview 상태 찾기
    pos = 0;
    while ((pos = data.find("<type>Preview</type>", pos)) != std::string::npos) {
        size_t num_start = data.rfind("<number>", pos);
        if (num_start != std::string::npos) {
            num_start += 8;
            size_t num_end = data.find("</number>", num_start);
            if (num_end != std::string::npos) {
                std::string num_str = data.substr(num_start, num_end - num_start);
                int channel = atoi(num_str.c_str());
                if (channel > 0 && channel <= TALLY_MAX_CHANNELS) {
                    uint8_t idx = channel - 1;
                    state_.tally_packed |= (2ULL << (idx * 2));  // Preview 비트 설정
                    updates++;
                }
            }
        }
        pos++;
    }

    if (updates > 0) {
        state_.num_cameras = TALLY_MAX_CHANNELS;  // 기본값 (실제로는 XML에서 추출)

        // Tally 콜백 호출
        if (tally_callback_) {
            tally_callback_();
        }

        T_LOGD(TAG, "tally:%d", updates);
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
