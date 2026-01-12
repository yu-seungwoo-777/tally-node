/**
 * @file AtemDriver.h
 * @brief ATEM UDP Driver (Driver Layer)
 *
 * 역할: ATEM 스위처와 UDP 통신
 * - ISwitcherPort 인터페이스 구현
 * - ATEM 프로토콜 처리
 */

#ifndef ATEM_DRIVER_H
#define ATEM_DRIVER_H

#include "TallyTypes.h"
#include "AtemProtocol.h"
#include <array>
#include <string>
#include <functional>

#ifdef __cplusplus

/**
 * @brief ATEM Driver 설정
 */
struct AtemConfig {
    std::string name;            ///< 스위처 이름 (로그용, 예: "Primary", "Secondary")
    std::string ip;              ///< ATEM IP 주소
    uint16_t port;               ///< 포트 (기본 9910)
    uint8_t camera_limit;        ///< 카메라 제한 (0 = 자동)
    std::string local_bind_ip;   ///< 로컬 바인딩 IP (비어있으면 INADDR_ANY)

    AtemConfig()
        : name("ATEM")
        , ip("")
        , port(ATEM_DEFAULT_PORT)
        , camera_limit(0)
        , local_bind_ip("")  // 기본값: 자동 선택 (INADDR_ANY)
    {}
};

/**
 * @brief ATEM 내부 상태
 */
struct AtemState {
    // 연결 상태
    bool connected;
    bool initialized;
    uint16_t session_id;
    uint32_t last_contact_ms;

    // 패킷 ID 추적 (정확한 순서 보장)
    uint16_t local_packet_id;       ///< 클라이언트 → 스위처 패킷 ID
    uint16_t remote_packet_id;      ///< 스위처 → 클라이언트 마지막 패킷 ID
    uint16_t last_received_packet_id; ///< 중복 패킷 체크용

    // Keepalive
    uint32_t last_keepalive_ms;

    // 기기 정보
    uint8_t protocol_major;
    uint8_t protocol_minor;
    char product_name[64];

    // 토폴로지 정보
    uint8_t num_mes;          ///< Mix Effect 수
    uint8_t num_sources;      ///< 소스 총 수
    uint8_t num_cameras;      ///< 카메라 수 (_TlC에서 설정)
    uint8_t num_dsks;         ///< DSK 수
    uint8_t num_supersources; ///< SuperSource 수
    bool topology_received;   ///< _top 수신 여부
    bool tally_config_received; ///< _TlC 수신 여부

    // Program/Preview (ME 0)
    uint16_t program_input;
    uint16_t preview_input;

    // Tally (패킹된 형태)
    uint64_t tally_packed;

    AtemState()
        : connected(false)
        , initialized(false)
        , session_id(0)
        , last_contact_ms(0)
        , local_packet_id(0)
        , remote_packet_id(0)
        , last_received_packet_id(0)
        , last_keepalive_ms(0)
        , protocol_major(0)
        , protocol_minor(0)
        , num_mes(0)
        , num_sources(0)
        , num_cameras(0)
        , num_dsks(0)
        , num_supersources(0)
        , topology_received(false)
        , tally_config_received(false)
        , program_input(0)
        , preview_input(0)
        , tally_packed(0)
    {
        product_name[0] = '\0';
    }
};

/**
 * @brief ATEM Driver 클래스
 *
 * ISwitcherPort 구현, ATEM UDP 프로토콜 처리
 */
class AtemDriver : public ISwitcherPort {
public:
    explicit AtemDriver(const AtemConfig& config);
    ~AtemDriver() override;

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
    switcher_type_t getType() const override { return SWITCHER_TYPE_ATEM; }
    uint32_t getConnectTimeout() const override { return ATEM_CONNECT_TIMEOUT_MS; }

    tally_status_t getChannelTally(uint8_t channel) const override;

    void cut() override;
    void autoTransition() override;
    void setPreview(uint16_t source_id) override;

    void setTallyCallback(std::function<void()> callback) override;
    void setConnectionCallback(std::function<void(connection_state_t)> callback) override;

    // ========================================================================
    // 네트워크 오류 상태 확인 (서비스 레이어용)
    // ========================================================================

    /**
     * @brief 네트워크 재시작 필요 여부 확인 및 플래그 클리어
     * @return 네트워크 재시작이 필요하면 true
     */
    bool checkAndClearNetworkRestart() {
        bool result = needsNetworkRestart_;
        needsNetworkRestart_ = false;
        return result;
    }

private:
    // 설정
    AtemConfig config_;

    // 상태
    AtemState state_;
    connection_state_t conn_state_;

    // 소켓
    int sock_fd_;

    // 버퍼
    std::array<uint8_t, ATEM_MAX_PACKET_SIZE> rx_buffer_;
    std::array<uint8_t, 64> tx_buffer_;

    // Packed Tally 데이터 (캐시)
    mutable packed_data_t cached_packed_;

    // 콜백
    std::function<void()> tally_callback_;
    std::function<void(connection_state_t)> connection_callback_;

    // 연결 타임아웃 추적
    uint32_t connect_attempt_time_;  ///< 연결 시도 시작 시간 (ms)
    uint32_t last_hello_time_;       ///< 마지막 Hello 전송 시간 (ms)

    // 네트워크 오류 감지
    uint32_t lastNetworkRestart_;    ///< 마지막 네트워크 재시작 시간 (ms)
    bool needsNetworkRestart_;       ///< 네트워크 재시작 필요 플래그

    // 콜백 중복 방지용 (마지막으로 콜백을 호출한 tally_packed 값)
    uint64_t last_tally_packed_for_callback_;

    static constexpr uint32_t RESTART_COOLDOWN_MS = 30000; ///< 30초 쿨다운

    // ========================================================================
    // 패킷 생성
    // ========================================================================

    void createHelloPacket(uint8_t* buf);
    void createAckPacket(uint8_t* buf, uint16_t session_id, uint16_t packet_id);
    void createKeepalivePacket(uint8_t* buf);
    int createCommandPacket(const char* cmd, const uint8_t* data, uint16_t length);

    // ========================================================================
    // 패킷 처리
    // ========================================================================

    int processPacket(const uint8_t* data, uint16_t length);
    void parseCommands(const uint8_t* data, uint16_t length);
    void handleCommand(const char* cmd_name, const uint8_t* cmd_data, uint16_t cmd_length);
    void handleTallyByIndex(const uint8_t* data, uint16_t length);
    void updateTallyPacked();

    // ========================================================================
    // 네트워크 유틸리티
    // ========================================================================

    int sendPacket(const uint8_t* data, uint16_t length);
    uint32_t getMillis() const;
    void setConnectionState(connection_state_t new_state);

    // ========================================================================
    // 유틸리티
    // ========================================================================

    void printTopology();
};

#endif // __cplusplus

#endif // ATEM_DRIVER_H
