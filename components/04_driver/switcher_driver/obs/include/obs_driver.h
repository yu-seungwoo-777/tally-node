/**
 * @file ObsDriver.h
 * @brief OBS WebSocket Driver (Driver Layer)
 *
 * 역할: OBS Studio와 WebSocket 통신
 * - ISwitcherPort 인터페이스 구현
 * - obs-websocket-js 프로토콜 처리 (v5)
 */

#ifndef OBS_DRIVER_H
#define OBS_DRIVER_H

#include "TallyTypes.h"
#include <array>
#include <string>
#include <functional>

#ifdef __cplusplus

// ============================================================================
// 상수
// ============================================================================

#define OBS_DEFAULT_PORT           4455     ///< OBS WebSocket 포트
#define OBS_CONNECT_TIMEOUT_MS     5000     ///< 연결 타임아웃
#define OBS_MAX_SILENCE_TIME_MS    5000     ///< 최대 무응답 시간
#define OBS_RECV_TIMEOUT_MS        100      ///< 수신 타임아웃

// Opcode (WebSocket)
#define WS_OPCODE_CONTINUATION     0x00
#define WS_OPCODE_TEXT             0x01
#define WS_OPCODE_BINARY           0x02
#define WS_OPCODE_CLOSE            0x08
#define WS_OPCODE_PING             0x09
#define WS_OPCODE_PONG             0x0A

// OBS Request Types
#define OBS_OP_GET_STATS           "GetStats"
#define OBS_OP_GET_SCENE_LIST      "GetSceneList"
#define OBS_OP_GET_TRANSITION_LIST "GetTransitionList"
#define OBS_OP_SET_CURRENT_SCENE   "SetCurrentScene"
#define OBS_OP_TRANSITION_TO_PROGRAM "TransitionToProgram"

// ============================================================================
// 구조체
// ============================================================================

/**
 * @brief OBS Driver 설정
 */
struct ObsConfig {
    std::string name;            ///< 스위처 이름 (로그용)
    std::string ip;              ///< OBS IP 주소
    uint16_t port;               ///< 포트 (기본 4455)
    std::string password;        ///< 비밀번호 (옵션)
    uint8_t camera_limit;        ///< 카메라 제한 (0 = 자동)

    ObsConfig()
        : name("OBS")
        , ip("")
        , port(OBS_DEFAULT_PORT)
        , password("")
        , camera_limit(0)
    {}
};

/**
 * @brief OBS 내부 상태
 */
struct ObsState {
    bool connected;
    bool authenticated;
    bool initialized;
    uint32_t last_update_ms;

    // Tally 데이터 (패킹된 형태)
    uint64_t tally_packed;
    uint8_t num_cameras;

    // Program/Preview (Scene 이름)
    std::string program_scene;
    std::string preview_scene;

    // WebSocket
    uint32_t message_id;

    ObsState()
        : connected(false)
        , authenticated(false)
        , initialized(false)
        , last_update_ms(0)
        , tally_packed(0)
        , num_cameras(0)
        , program_scene("")
        , preview_scene("")
        , message_id(0)
    {}
};

// ============================================================================
// 드라이버 클래스
// ============================================================================

/**
 * @brief OBS Driver 클래스
 *
 * ISwitcherPort 구현, OBS WebSocket 프로토콜 처리
 */
class ObsDriver : public ISwitcherPort {
public:
    explicit ObsDriver(const ObsConfig& config);
    ~ObsDriver() override;

    // ISwitcherPort 구현
    bool initialize() override;
    void connect() override;
    void disconnect() override;
    int loop() override;

    connection_state_t getConnectionState() const override;
    bool isConnected() const override;
    bool isInitialized() const override;
    packed_data_t getPackedTally() const override;
    uint8_t getCameraCount() const override;
    uint32_t getLastUpdateTime() const override;
    switcher_type_t getType() const override { return SWITCHER_TYPE_OBS; }
    uint32_t getConnectTimeout() const override { return OBS_CONNECT_TIMEOUT_MS; }

    tally_status_t getChannelTally(uint8_t channel) const override;

    void cut() override;
    void autoTransition() override;
    void setPreview(uint16_t source_id) override;

    void setTallyCallback(std::function<void()> callback) override;
    void setConnectionCallback(std::function<void(connection_state_t)> callback) override;

private:
    // 설정
    ObsConfig config_;

    // 상태
    ObsState state_;
    connection_state_t conn_state_;

    // 소켓
    int sock_fd_;

    // 버퍼
    std::array<uint8_t, 8192> rx_buffer_;

    // Packed Tally 데이터 (캐시)
    mutable packed_data_t cached_packed_;

    // 콜백
    std::function<void()> tally_callback_;
    std::function<void(connection_state_t)> connection_callback_;

    // 연결 타임아웃 추적
    uint32_t connect_attempt_time_;

    // WebSocket 핸드셰이크 상태
    enum { WS_STATE_DISCONNECTED, WS_STATE_CONNECTING, WS_STATE_HANDSHAKE, WS_STATE_CONNECTED };
    int ws_state_;

    // ========================================================================
    // WebSocket 메서드
    // ========================================================================

    int sendWebSocketFrame(const uint8_t* data, size_t length, uint8_t opcode = WS_OPCODE_TEXT);
    int receiveWebSocketFrame(uint8_t* buffer, size_t max_length);
    bool performHandshake();
    std::string createHandshakeRequest();
    bool parseHandshakeResponse(const char* response);

    // ========================================================================
    // OBS 프로토콜 메서드
    // ========================================================================

    int sendOBSRequest(const char* request_type, const char* params = nullptr);
    int parseOBSMessage(const char* json_data);
    void handleSceneChange(const char* scene_name, bool is_program);
    void updateTallyPacked();

    // ========================================================================
    // 유틸리티
    // ========================================================================

    uint32_t getMillis() const;
    void setConnectionState(connection_state_t new_state);
    std::string base64Encode(const uint8_t* data, size_t length);
};

#endif // __cplusplus

#endif // OBS_DRIVER_H
