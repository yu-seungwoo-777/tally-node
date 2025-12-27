/**
 * @file ObsDriver.cpp
 * @brief OBS WebSocket Driver 구현
 *
 * 역할: OBS WebSocket 프로토콜 로직 구현
 */

#include "obs_driver.h"
#include "t_log.h"
#include <sys/socket.h>

// ============================================================================
// 태그
// ============================================================================

static const char* TAG = "ObsDriver";
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

ObsDriver::ObsDriver(const ObsConfig& config)
    : config_(config)
    , state_()
    , conn_state_(CONNECTION_STATE_DISCONNECTED)
    , sock_fd_(-1)
    , tally_callback_()
    , connection_callback_()
    , connect_attempt_time_(0)
    , ws_state_(WS_STATE_DISCONNECTED)
{
    rx_buffer_.fill(0);
    // 정적 버퍼 초기화
    cached_packed_.data_size = 0;
    cached_packed_.channel_count = 0;
    memset(cached_packed_.data, 0, sizeof(cached_packed_.data));
}

ObsDriver::~ObsDriver() {
    disconnect();
    packed_data_cleanup(&cached_packed_);
}

// ============================================================================
// 연결 관리
// ============================================================================

bool ObsDriver::initialize() {
    if (sock_fd_ >= 0) {
        T_LOGW(TAG, "이미 초기화됨 (sock_fd=%d)", sock_fd_);
        return true;
    }

    // TCP 소켓 생성
    sock_fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd_ < 0) {
        T_LOGE(TAG, "소켓 생성 실패 (errno=%d)", errno);
        return false;
    }

    // 논블로킹 모드 설정
    int flags = fcntl(sock_fd_, F_GETFL, 0);
    fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

    T_LOGI(TAG, "초기화 완료 (sock_fd=%d)", sock_fd_);
    return true;
}

void ObsDriver::connect() {
    if (conn_state_ != CONNECTION_STATE_DISCONNECTED) {
        T_LOGW(TAG, "이미 연결 중 또는 연결됨 (state=%d)", static_cast<int>(conn_state_));
        return;
    }

    // 소켓이 유효하지 않으면 다시 초기화
    if (sock_fd_ < 0) {
        if (!initialize()) {
            T_LOGE(TAG, "소켓 재초기화 실패");
            return;
        }
    }

    T_LOGI(TAG, "OBS 연결 시도: %s:%d", config_.ip.c_str(), config_.port);

    // 상태 초기화
    state_ = ObsState();
    ws_state_ = WS_STATE_CONNECTING;
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
            T_LOGI(TAG, "연결 진행 중...");
        } else {
            T_LOGE(TAG, "연결 실패 (errno=%d)", errno);
            disconnect();
        }
    } else {
        // 즉시 연결 성공 - 핸드셰이크 시작
        ws_state_ = WS_STATE_HANDSHAKE;
        T_LOGI(TAG, "TCP 연결 성공, 핸드셰이크 시작");
    }
}

void ObsDriver::disconnect() {
    if (sock_fd_ >= 0) {
        // Close 핸드셰이크 전송 (선택 사항)
        uint8_t close_frame[2] = { 0x88, 0x00 };  // Opcode: Close
        send(sock_fd_, close_frame, 2, 0);

        close(sock_fd_);
        sock_fd_ = -1;
    }

    bool was_connected = state_.connected;
    state_.connected = false;
    state_.authenticated = false;
    state_.initialized = false;
    ws_state_ = WS_STATE_DISCONNECTED;
    setConnectionState(CONNECTION_STATE_DISCONNECTED);

    if (was_connected) {
        T_LOGI(TAG, "연결 종료");
    }
}

// ============================================================================
// 루프 처리
// ============================================================================

int ObsDriver::loop() {
    if (conn_state_ == CONNECTION_STATE_DISCONNECTED) {
        return -1;
    }

    int processed = 0;
    uint32_t now = getMillis();

    // CONNECTING 상태 처리
    if (ws_state_ == WS_STATE_CONNECTING) {
        // 연결 타임아웃 체크
        if (now - connect_attempt_time_ > OBS_CONNECT_TIMEOUT_MS) {
            T_LOGE(TAG, "연결 타임아웃");
            disconnect();
            return -1;
        }

        // 연결 완료 확인
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sock_fd_, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
            if (error == 0) {
                ws_state_ = WS_STATE_HANDSHAKE;
                T_LOGI(TAG, "TCP 연결 성공, 핸드셰이크 시작");
            } else if (error != EINPROGRESS) {
                T_LOGE(TAG, "연결 실패 (error=%d)", error);
                disconnect();
                return -1;
            }
        }
    }

    // WebSocket 핸드셰이크
    if (ws_state_ == WS_STATE_HANDSHAKE) {
        std::string handshake = createHandshakeRequest();
        ssize_t sent = send(sock_fd_, handshake.c_str(), handshake.length(), 0);

        if (sent > 0) {
            T_LOGI(TAG, "핸드셰이크 전송");

            // 응답 대기
            uint8_t response[512];
            ssize_t received = recv(sock_fd_, response, sizeof(response) - 1, 0);

            if (received > 0) {
                response[received] = '\0';
                if (parseHandshakeResponse(reinterpret_cast<char*>(response))) {
                    ws_state_ = WS_STATE_CONNECTED;
                    state_.connected = true;
                    state_.last_update_ms = now;
                    setConnectionState(CONNECTION_STATE_READY);
                    T_LOGI(TAG, "WebSocket 핸드셰이크 성공");

                    // 초기 요청 전송
                    sendOBSRequest(OBS_OP_GET_SCENE_LIST);
                } else {
                    T_LOGE(TAG, "핸드셰이크 응답 파싱 실패");
                    disconnect();
                    return -1;
                }
            }
        }
    }

    // 데이터 수신 (WebSocket 프레임)
    if (ws_state_ == WS_STATE_CONNECTED) {
        int received = receiveWebSocketFrame(rx_buffer_.data(), rx_buffer_.size());

        if (received > 0) {
            rx_buffer_[received] = '\0';
            state_.last_update_ms = now;

            // OBS 메시지 파싱
            int parsed = parseOBSMessage(reinterpret_cast<char*>(rx_buffer_.data()));
            if (parsed > 0) {
                processed++;
            }
        } else if (received < 0) {
            // 연결 종료 또는 오류
            disconnect();
            return -1;
        }

        // 타임아웃 체크
        if (now - state_.last_update_ms > OBS_MAX_SILENCE_TIME_MS) {
            T_LOGW(TAG, "타임아웃 (무응답 %dms)", (int)(now - state_.last_update_ms));
            disconnect();
            return -1;
        }
    }

    return processed;
}

// ============================================================================
// 상태 조회
// ============================================================================

connection_state_t ObsDriver::getConnectionState() const {
    return conn_state_;
}

bool ObsDriver::isConnected() const {
    return state_.connected;
}

bool ObsDriver::isInitialized() const {
    return state_.initialized;
}

packed_data_t ObsDriver::getPackedTally() const {
    uint8_t channel_count = state_.num_cameras;

    if (config_.camera_limit > 0 && channel_count > config_.camera_limit) {
        channel_count = config_.camera_limit;
    }

    if (channel_count > TALLY_MAX_CHANNELS) {
        channel_count = TALLY_MAX_CHANNELS;
    }

    if (cached_packed_.channel_count != channel_count) {
        packed_data_cleanup(&cached_packed_);
        packed_data_init(&cached_packed_, channel_count);
    }

    for (uint8_t i = 0; i < channel_count; i++) {
        uint8_t flags = (state_.tally_packed >> (i * 2)) & 0x03;
        packed_data_set_channel(&cached_packed_, i + 1, flags);
    }

    return cached_packed_;
}

uint8_t ObsDriver::getCameraCount() const {
    return state_.num_cameras;
}

uint32_t ObsDriver::getLastUpdateTime() const {
    return state_.last_update_ms;
}

tally_status_t ObsDriver::getChannelTally(uint8_t channel) const {
    if (channel < 1 || channel > state_.num_cameras) {
        return TALLY_STATUS_OFF;
    }

    uint8_t flags = (state_.tally_packed >> ((channel - 1) * 2)) & 0x03;
    return static_cast<tally_status_t>(flags);
}

// ============================================================================
// 제어 명령
// ============================================================================

void ObsDriver::cut() {
    if (!state_.connected) {
        T_LOGW(TAG, "연결되지 않음 - cut() 무시");
        return;
    }
    // OBS는 기본적으로 Cut이 없음, 빠른 트랜지션으로 구현
    // 또는 TransitionToProgram 요청
    sendOBSRequest(OBS_OP_TRANSITION_TO_PROGRAM);
}

void ObsDriver::autoTransition() {
    if (!state_.connected) {
        T_LOGW(TAG, "연결되지 않음 - auto() 무시");
        return;
    }
    sendOBSRequest(OBS_OP_TRANSITION_TO_PROGRAM);
}

void ObsDriver::setPreview(uint16_t source_id) {
    if (!state_.connected) {
        T_LOGW(TAG, "연결되지 않음 - setPreview() 무시");
        return;
    }
    // OBS에서는 소스 ID가 아닌 Scene 이름을 사용
    // 실제 구현에서는 Scene 매핑 필요
    T_LOGW(TAG, "OBS setPreview: Scene 매핑 필요");
}

// ============================================================================
// 콜백 설정
// ============================================================================

void ObsDriver::setTallyCallback(std::function<void()> callback) {
    tally_callback_ = callback;
}

void ObsDriver::setConnectionCallback(std::function<void(connection_state_t)> callback) {
    connection_callback_ = callback;
}

// ============================================================================
// WebSocket 메서드
// ============================================================================

int ObsDriver::sendWebSocketFrame(const uint8_t* data, size_t length, uint8_t opcode) {
    if (sock_fd_ < 0) {
        return -1;
    }

    std::array<uint8_t, 16> frame;
    size_t frame_len = 0;

    // 첫 번째 바이트: FIN + Opcode
    frame[0] = 0x80 | opcode;
    frame_len++;

    // 두 번째 바이트: MASK + Payload Length
    if (length < 126) {
        frame[1] = 0x80 | static_cast<uint8_t>(length);  // MASK bit set
        frame_len = 2;
    } else if (length < 65536) {
        frame[1] = 0x80 | 126;
        frame[2] = (length >> 8) & 0xFF;
        frame[3] = length & 0xFF;
        frame_len = 4;
    } else {
        // 64-bit length 지원 필요 시 구현
        return -1;
    }

    // 마스킹 키 (임의 값)
    uint8_t masking_key[4] = { 0x12, 0x34, 0x56, 0x78 };

    // 마스킹 키 복사
    memcpy(frame.data() + frame_len, masking_key, 4);
    frame_len += 4;

    // 헤더 전송
    ssize_t sent = send(sock_fd_, frame.data(), frame_len, 0);
    if (sent < 0) {
        return -1;
    }

    // 마스킹된 데이터 전송
    std::vector<uint8_t> masked_data(length);
    for (size_t i = 0; i < length; i++) {
        masked_data[i] = data[i] ^ masking_key[i % 4];
    }

    sent = send(sock_fd_, masked_data.data(), length, 0);
    if (sent < 0) {
        return -1;
    }

    return static_cast<int>(sent);
}

int ObsDriver::receiveWebSocketFrame(uint8_t* buffer, size_t max_length) {
    if (sock_fd_ < 0) {
        return -1;
    }

    // 헤더 수신 (최소 2바이트)
    uint8_t header[2];
    ssize_t received = recv(sock_fd_, header, 2, 0);

    if (received <= 0) {
        if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            return -1;
        }
        return 0;
    }

    // Opcode 추출
    uint8_t opcode = header[0] & 0x0F;

    // Close 프레임 처리
    if (opcode == WS_OPCODE_CLOSE) {
        T_LOGI(TAG, "Close 프레임 수신");
        return -1;
    }

    // Payload Length
    uint8_t payload_len = header[1] & 0x7F;
    size_t actual_len = payload_len;
    size_t header_len = 2;

    if (payload_len == 126) {
        // 2바이트 길이
        uint8_t ext_len[2];
        recv(sock_fd_, ext_len, 2, 0);
        actual_len = (ext_len[0] << 8) | ext_len[1];
        header_len += 2;
    } else if (payload_len == 127) {
        // 8바이트 길이
        uint8_t ext_len[8];
        recv(sock_fd_, ext_len, 8, 0);
        // 64-bit 처리
        header_len += 8;
    }

    // MASK bit 확인 (서버 → 클라이언트는 MASK 없음)
    bool masked = (header[1] & 0x80) != 0;
    if (masked) {
        // 마스킹 키 스킵
        uint8_t masking_key[4];
        recv(sock_fd_, masking_key, 4, 0);
        header_len += 4;
    }

    // 페이로드 수신
    if (actual_len > max_length) {
        actual_len = max_length;
    }

    received = recv(sock_fd_, buffer, actual_len, 0);

    if (received <= 0) {
        return 0;
    }

    // 마스킹 해제 (필요한 경우)
    if (masked) {
        // 실제 마스킹 해제 로직 필요
    }

    return static_cast<int>(received);
}

std::string ObsDriver::createHandshakeRequest() {
    std::stringstream ss;

    // WebSocket 키 생성 (임의 값)
    const char* sec_key = "dGhlIHNhbXBsZSBub25jZQ==";  // Base64 예시

    ss << "GET / HTTP/1.1\r\n";
    ss << "Host: " << config_.ip << ":" << config_.port << "\r\n";
    ss << "Upgrade: websocket\r\n";
    ss << "Connection: Upgrade\r\n";
    ss << "Sec-WebSocket-Key: " << sec_key << "\r\n";
    ss << "Sec-WebSocket-Version: 13\r\n";
    ss << "\r\n";

    return ss.str();
}

bool ObsDriver::parseHandshakeResponse(const char* response) {
    // "HTTP/1.1 101 Switching Protocols" 확인
    if (strstr(response, "101") == nullptr) {
        return false;
    }

    // "Upgrade: websocket" 확인
    if (strstr(response, "websocket") == nullptr) {
        return false;
    }

    return true;
}

// ============================================================================
// OBS 프로토콜 메서드
// ============================================================================

int ObsDriver::sendOBSRequest(const char* request_type, const char* params) {
    if (!state_.connected) {
        return -1;
    }

    std::stringstream ss;
    ss << "{\"request-type\":\"" << request_type << "\"";
    ss << ",\"message-id\":" << state_.message_id++;

    if (params) {
        ss << "," << params;
    }

    ss << "}";

    std::string json = ss.str();
    return sendWebSocketFrame(reinterpret_cast<const uint8_t*>(json.c_str()), json.length(), WS_OPCODE_TEXT);
}

int ObsDriver::parseOBSMessage(const char* json_data) {
    // 간단한 JSON 파싱
    std::string data(json_data);
    int updates = 0;

    // "update-type" 찾기
    size_t pos = data.find("\"update-type\":");
    if (pos != std::string::npos) {
        pos += 14;

        // 값 추출 (따옴표로 둘러싸인 문자열)
        size_t start = data.find("\"", pos);
        if (start != std::string::npos) {
            start++;
            size_t end = data.find("\"", start);
            if (end != std::string::npos) {
                std::string update_type = data.substr(start, end - start);

                if (update_type == "SwitchScenes") {
                    // Scene 변경 이벤트
                    size_t scene_pos = data.find("\"scene-name\":");
                    if (scene_pos != std::string::npos) {
                        scene_pos += 14;
                        size_t s_start = data.find("\"", scene_pos);
                        if (s_start != std::string::npos) {
                            s_start++;
                            size_t s_end = data.find("\"", s_start);
                            if (s_end != std::string::npos) {
                                std::string scene_name = data.substr(s_start, s_end - s_start);
                                handleSceneChange(scene_name.c_str(), true);
                                updates++;
                            }
                        }
                    }
                } else if (update_type == "PreviewSceneChanged") {
                    // Preview Scene 변경
                    size_t scene_pos = data.find("\"scene-name\":");
                    if (scene_pos != std::string::npos) {
                        scene_pos += 14;
                        size_t s_start = data.find("\"", scene_pos);
                        if (s_start != std::string::npos) {
                            s_start++;
                            size_t s_end = data.find("\"", s_start);
                            if (s_end != std::string::npos) {
                                std::string scene_name = data.substr(s_start, s_end - s_start);
                                handleSceneChange(scene_name.c_str(), false);
                                updates++;
                            }
                        }
                    }
                }
            }
        }
    }

    if (updates > 0 && tally_callback_) {
        tally_callback_();
    }

    return updates;
}

void ObsDriver::handleSceneChange(const char* scene_name, bool is_program) {
    // OBS에서는 Scene 이름을 채널 번호로 매핑해야 함
    // 예: "Camera 1" → channel 1, "Camera 2" → channel 2

    // 간단한 구현: Scene 이름에서 숫자 추출
    std::string scene(scene_name);
    int channel = -1;

    // "Camera 1", "Cam 1", "1" 등 형식 지원
    for (size_t i = 0; i < scene.length(); i++) {
        if (isdigit(scene[i])) {
            channel = atoi(scene.c_str() + i);
            break;
        }
    }

    if (channel > 0 && channel <= TALLY_MAX_CHANNELS) {
        uint8_t idx = channel - 1;

        // 기존 플래그 초기화
        state_.tally_packed &= ~(0x03ULL << (idx * 2));

        if (is_program) {
            state_.tally_packed |= (1ULL << (idx * 2));  // Program 비트
            state_.program_scene = scene_name;
            T_LOGI(TAG, "Program: %s (채널 %d)", scene_name, channel);
        } else {
            state_.tally_packed |= (2ULL << (idx * 2));  // Preview 비트
            state_.preview_scene = scene_name;
            T_LOGI(TAG, "Preview: %s (채널 %d)", scene_name, channel);
        }

        state_.num_cameras = TALLY_MAX_CHANNELS;
    }
}

void ObsDriver::updateTallyPacked() {
    // handleSceneChange에서 처리됨
}

// ============================================================================
// 유틸리티
// ============================================================================

uint32_t ObsDriver::getMillis() const {
    return static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void ObsDriver::setConnectionState(connection_state_t new_state) {
    if (conn_state_ != new_state) {
        conn_state_ = new_state;

        const char* state_name = connection_state_to_string(new_state);
        T_LOGI(TAG, "[%s] 연결 상태: %s", config_.name.c_str(), state_name);

        if (connection_callback_) {
            connection_callback_(new_state);
        }
    }
}

std::string ObsDriver::base64Encode(const uint8_t* data, size_t length) {
    // Base64 인코딩 구현
    static const char* encode_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve(((length + 2) / 3) * 4);

    for (size_t i = 0; i < length; i += 3) {
        uint32_t value = 0;
        for (size_t j = 0; j < 3; j++) {
            value <<= 8;
            if (i + j < length) {
                value |= data[i + j];
            }
        }

        result.push_back(encode_table[(value >> 18) & 0x3F]);
        result.push_back(encode_table[(value >> 12) & 0x3F]);
        result.push_back(encode_table[(value >> 6) & 0x3F]);
        result.push_back(encode_table[value & 0x3F]);
    }

    // 패딩
    while (result.length() % 4 != 0) {
        result.push_back('=');
    }

    return result;
}
