/**
 * @file VmixDriver.h
 * @brief vMix TCP Driver (Driver Layer)
 *
 * 역할: vMix 스위처와 TCP 통신
 * - ISwitcherPort 인터페이스 구현
 * - vMix XML 프로토콜 처리
 */

#ifndef VMIX_DRIVER_H
#define VMIX_DRIVER_H

#include "TallyTypes.h"
#include <array>
#include <string>
#include <functional>

#ifdef __cplusplus

// ============================================================================
// 상수
// ============================================================================

#define VMIX_DEFAULT_PORT           8099     ///< vMix TCP 포트
#define VMIX_CONNECT_TIMEOUT_MS     5000     ///< 연결 타임아웃
#define VMIX_MAX_SILENCE_TIME_MS    5000     ///< 최대 무응답 시간
#define VMIX_POLLING_INTERVAL_MS    100      ///< 폴링 간격

// vMix XML 명령
#define VMIX_CMD_TALLY              "TALLY"
#define VMIX_CMD_ACTIVATE           "ACTIVE"
#define VMIX_CMD_PREVIEW            "PREVIEW"
#define VMIX_CMD_CUT                "Cut"
#define VMIX_CMD_AUTO               "Auto"

// ============================================================================
// 구조체
// ============================================================================

/**
 * @brief vMix Driver 설정
 */
struct VmixConfig {
    std::string name;            ///< 스위처 이름 (로그용)
    std::string ip;              ///< vMix IP 주소
    uint16_t port;               ///< 포트 (기본 8099)
    uint8_t camera_limit;        ///< 카메라 제한 (0 = 자동)

    VmixConfig()
        : name("VMIX")
        , ip("")
        , port(VMIX_DEFAULT_PORT)
        , camera_limit(0)
    {}
};

/**
 * @brief vMix 내부 상태
 */
struct VmixState {
    bool connected;
    bool initialized;
    uint32_t last_update_ms;

    // Tally 데이터 (패킹된 형태)
    uint64_t tally_packed;
    uint8_t num_cameras;

    // Program/Preview
    uint16_t program_input;
    uint16_t preview_input;

    VmixState()
        : connected(false)
        , initialized(false)
        , last_update_ms(0)
        , tally_packed(0)
        , num_cameras(0)
        , program_input(0)
        , preview_input(0)
    {}
};

// ============================================================================
// 드라이버 클래스
// ============================================================================

/**
 * @brief vMix Driver 클래스
 *
 * ISwitcherPort 구현, vMix TCP 프로토콜 처리
 */
class VmixDriver : public ISwitcherPort {
public:
    explicit VmixDriver(const VmixConfig& config);
    ~VmixDriver() override;

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
    switcher_type_t getType() const override { return SWITCHER_TYPE_VMIX; }
    uint32_t getConnectTimeout() const override { return VMIX_CONNECT_TIMEOUT_MS; }

    tally_status_t getChannelTally(uint8_t channel) const override;

    void cut() override;
    void autoTransition() override;
    void setPreview(uint16_t source_id) override;

    void setTallyCallback(std::function<void()> callback) override;
    void setConnectionCallback(std::function<void(connection_state_t)> callback) override;

private:
    // 설정
    VmixConfig config_;

    // 상태
    VmixState state_;
    connection_state_t conn_state_;

    // 소켓
    int sock_fd_;

    // 버퍼
    std::array<uint8_t, 4096> rx_buffer_;

    // Packed Tally 데이터 (캐시)
    mutable packed_data_t cached_packed_;

    // 콜백
    std::function<void()> tally_callback_;
    std::function<void(connection_state_t)> connection_callback_;

    // 연결 타임아웃 추적
    uint32_t connect_attempt_time_;

    // ========================================================================
    // 내부 메서드
    // ========================================================================

    int sendCommand(const char* cmd);
    int parseTallyData(const char* xml_data);
    void updateTallyPacked();
    int receiveData();
    uint32_t getMillis() const;
    void setConnectionState(connection_state_t new_state);
};

#endif // __cplusplus

#endif // VMIX_DRIVER_H
