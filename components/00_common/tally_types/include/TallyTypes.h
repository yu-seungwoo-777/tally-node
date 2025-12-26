/**
 * @file TallyTypes.h
 * @brief Tally 공통 타입 정의
 *
 * 역할: 모든 Switcher 컴포넌트에서 사용하는 공통 타입과 인터페이스
 */

#ifndef TALLY_TYPES_H
#define TALLY_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 열거형 (Enum)
// ============================================================================

/**
 * @brief Switcher 타입
 */
typedef enum {
    SWITCHER_TYPE_ATEM = 0,    ///< Blackmagic ATEM (UDP, 포트 9910)
    SWITCHER_TYPE_OBS = 1,     ///< OBS Studio (WebSocket, 포트 4455)
    SWITCHER_TYPE_VMIX = 2     ///< vMix (TCP, 포트 8099)
} switcher_type_t;

/**
 * @brief 스위처 역할 (Primary/Secondary)
 */
typedef enum {
    SWITCHER_ROLE_PRIMARY = 0,     ///< Primary 스위처
    SWITCHER_ROLE_SECONDARY = 1    ///< Secondary 스위처
} switcher_role_t;

/**
 * @brief 네트워크 인터페이스 타입
 *
 * 스위처가 사용할 네트워크 인터페이스
 * NVS 값과 일치: 1=WiFi, 2=Ethernet
 */
typedef enum {
    TALLY_NET_WIFI = 1,        ///< WiFi 사용
    TALLY_NET_ETHERNET = 2    ///< Ethernet 사용
} tally_network_if_t;

/**
 * @brief Tally 상태 (2비트)
 *
 * ATEM 프로토콜과 동일한 비트 배치:
 * - bit0 (0x01) = Program
 * - bit1 (0x02) = Preview
 */
typedef enum {
    TALLY_STATUS_OFF = 0,        ///< 0b00: Off
    TALLY_STATUS_PROGRAM = 1,    ///< 0b01: Program (bit0)
    TALLY_STATUS_PREVIEW = 2,    ///< 0b10: Preview (bit1)
    TALLY_STATUS_BOTH = 3        ///< 0b11: Program + Preview
} tally_status_t;

/**
 * @brief Switcher 연결 상태
 */
typedef enum {
    CONNECTION_STATE_DISCONNECTED = 0,    ///< 연결 안됨
    CONNECTION_STATE_CONNECTING = 1,      ///< 연결 시도 중
    CONNECTION_STATE_CONNECTED = 2,       ///< 연결됨
    CONNECTION_STATE_INITIALIZING = 3,    ///< 초기화 중
    CONNECTION_STATE_READY = 4            ///< 사용 가능
} connection_state_t;

// ============================================================================
// 구조체 (Struct)
// ============================================================================

/**
 * @brief Packed 데이터 구조체
 *
 * 가변 길이 Packed 데이터:
 * - 채널당 2비트 필요
 * - (채널 수 + 3) / 4 바이트로 계산
 * - 예: 4채널=1바이트, 8채널=2바이트, 20채널=5바이트
 */
typedef struct {
    uint8_t* data;              ///< packed 데이터 바이트 배열
    uint8_t data_size;          ///< 바이트 배열 크기
    uint8_t channel_count;      ///< 실제 채널 수
} packed_data_t;

// tally_event_data_t는 event_bus.h로 이동 (EVT_TALLY_STATE_CHANGED 이벤트용)

/**
 * @brief Switcher 상태 구조체
 */
typedef struct {
    connection_state_t state;   ///< 연결 상태
    uint8_t camera_count;       ///< 카메라 수
    uint32_t last_update_time;  ///< 마지막 업데이트 시간 (ms)
    bool tally_changed;         ///< Tally 변경 여부
} switcher_status_t;

/**
 * @brief Switcher 설정 구조체
 */
typedef struct {
    const char* name;           ///< 스위처 이름 (로그용)
    switcher_type_t type;       ///< Switcher 타입
    tally_network_if_t interface; ///< 네트워크 인터페이스
    char ip[64];                ///< IP 주소
    uint16_t port;              ///< 포트 번호 (0 = 기본값 사용)
    char password[64];          ///< 비밀번호 (OBS용, 옵션)
    uint8_t camera_limit;       ///< 카메라 제한 (0 = 자동)
} switcher_config_t;

// ============================================================================
// 상수 (Constants)
// ============================================================================

#define TALLY_MAX_CHANNELS          20      ///< 최대 Tally 채널 수
#define TALLY_CONNECT_TIMEOUT_MS    5000    ///< 연결 타임아웃
#define TALLY_MAX_SILENCE_MS        5000    ///< 최대 무응답 시간

// ============================================================================
// 콜백 타입 (Callback Types)
// ============================================================================

/**
 * @brief Tally 변경 콜백
 */
typedef void (*tally_callback_t)(void);

/**
 * @brief 연결 상태 변경 콜백
 * @param state 새로운 연결 상태
 */
typedef void (*connection_callback_t)(connection_state_t state);

// ============================================================================
// PackedData 함수 (C 인터페이스)
// ============================================================================

/**
 * @brief PackedData 초기화
 * @param packed PackedData 구조체 포인터
 * @param channel_count 채널 수
 */
void packed_data_init(packed_data_t* packed, uint8_t channel_count);

/**
 * @brief PackedData 정리
 * @param packed PackedData 구조체 포인터
 */
void packed_data_cleanup(packed_data_t* packed);

/**
 * @brief 채널 플래그 설정
 * @param packed PackedData 구조체 포인터
 * @param channel 채널 번호 (1-based)
 * @param flags 플래그 (0=OFF, 1=PGM, 2=PVW, 3=BOTH)
 */
void packed_data_set_channel(packed_data_t* packed, uint8_t channel, uint8_t flags);

/**
 * @brief 채널 플래그 조회
 * @param packed PackedData 구조체 포인터
 * @param channel 채널 번호 (1-based)
 * @return 플래그 (0=OFF, 1=PGM, 2=PVW, 3=BOTH)
 */
uint8_t packed_data_get_channel(const packed_data_t* packed, uint8_t channel);

/**
 * @brief 두 PackedData 복사
 * @param dest 목적지 PackedData 포인터
 * @param src 원본 PackedData 포인터
 */
void packed_data_copy(packed_data_t* dest, const packed_data_t* src);

/**
 * @brief 두 PackedData 비교
 * @param a 첫 번째 PackedData 포인터
 * @param b 두 번째 PackedData 포인터
 * @return 동일 여부
 */
bool packed_data_equals(const packed_data_t* a, const packed_data_t* b);

/**
 * @brief PackedData 유효성 확인
 * @param packed PackedData 구조체 포인터
 * @return 유효 여부
 */
bool packed_data_is_valid(const packed_data_t* packed);

/**
 * @brief 데이터를 64비트 정수로 변환
 * @param packed PackedData 구조체 포인터
 * @return 64비트 packed 값
 */
uint64_t packed_data_to_uint64(const packed_data_t* packed);

/**
 * @brief 64비트 정수에서 PackedData로 변환
 * @param packed PackedData 구조체 포인터
 * @param value 64비트 packed 값
 * @param channel_count 채널 수
 */
void packed_data_from_uint64(packed_data_t* packed, uint64_t value, uint8_t channel_count);

/**
 * @brief PackedData를 16진수 문자열로 변환
 * @param packed PackedData 구조체 포인터
 * @param buf 출력 버퍼
 * @param buf_size 버퍼 크기 (최소 data_size * 2 + 1)
 * @return buf 포인터
 */
char* packed_data_to_hex(const packed_data_t* packed, char* buf, size_t buf_size);

/**
 * @brief PackedData를 Tally 문자열로 포맷 (공통 해석 함수)
 * @param packed PackedData 구조체 포인터
 * @param buf 출력 버퍼
 * @param buf_size 버퍼 크기 (최소 64)
 * @return buf 포인터
 *
 * 출력 형식: "PGM[1,2] PVW[3]" 또는 "PGM[-] PVW[-]"
 *
 * TX/RX 공통으로 사용:
 * - TX: 로그 출력용
 * - RX: 수신 패킷 해석용
 */
char* packed_data_format_tally(const packed_data_t* packed, char* buf, size_t buf_size);

// ============================================================================
// SwitcherConfig 함수
// ============================================================================

/**
 * @brief SwitcherConfig 기본값 초기화
 * @param config SwitcherConfig 구조체 포인터
 */
void switcher_config_init(switcher_config_t* config);

// ============================================================================
// SwitcherStatus 함수
// ============================================================================

/**
 * @brief SwitcherStatus 기본값 초기화
 * @param status SwitcherStatus 구조체 포인터
 */
void switcher_status_init(switcher_status_t* status);

// ============================================================================
// 유틸리티 함수
// ============================================================================

/**
 * @brief Switcher 타입을 문자열로 변환
 * @param type Switcher 타입
 * @return 타입 문자열
 */
const char* switcher_type_to_string(switcher_type_t type);

/**
 * @brief 연결 상태를 문자열로 변환
 * @param state 연결 상태
 * @return 상태 문자열
 */
const char* connection_state_to_string(connection_state_t state);

/**
 * @brief Tally 상태를 문자열로 변환
 * @param status Tally 상태
 * @return 상태 문자열
 */
const char* tally_status_to_string(tally_status_t status);

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ 인터페이스 (ISwitcherPort)
// ============================================================================

#ifdef __cplusplus

#include <functional>
#include <memory>

/**
 * @brief Switcher Port 인터페이스
 *
 * 모든 Switcher Driver(ATEM, OBS, vMix)가 구현해야 하는 공통 인터페이스
 */
class ISwitcherPort {
public:
    virtual ~ISwitcherPort() = default;

    // ========================================================================
    // 기본 연결
    // ========================================================================

    /**
     * @brief 드라이버 초기화
     * @return 성공 여부
     */
    virtual bool initialize() = 0;

    /**
     * @brief 스위처 연결 시작
     */
    virtual void connect() = 0;

    /**
     * @brief 스위처 연결 해제
     */
    virtual void disconnect() = 0;

    /**
     * @brief 루프 처리 (주기적으로 호출)
     * @return 처리된 패킷 수 또는 에러 코드 (<0)
     */
    virtual int loop() = 0;

    // ========================================================================
    // 상태 조회
    // ========================================================================

    /**
     * @brief 연결 상태 조회
     * @return 현재 연결 상태
     */
    virtual connection_state_t getConnectionState() const = 0;

    /**
     * @brief 연결 여부 확인
     * @return 연결되어 있으면 true
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief 초기화 여부 확인
     * @return 초기화되어 있으면 true
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Packed Tally 데이터 조회
     * @return Packed Tally 데이터
     */
    virtual packed_data_t getPackedTally() const = 0;

    /**
     * @brief 카메라 수 조회
     * @return 카메라 수
     */
    virtual uint8_t getCameraCount() const = 0;

    /**
     * @brief 마지막 업데이트 시간 조회
     * @return 마지막 업데이트 시간 (ms)
     */
    virtual uint32_t getLastUpdateTime() const = 0;

    /**
     * @brief Switcher 타입 조회
     * @return Switcher 타입
     */
    virtual switcher_type_t getType() const = 0;

    /**
     * @brief 연결 타임아웃 조회
     * @return 타임아웃 시간 (ms)
     */
    virtual uint32_t getConnectTimeout() const = 0;

    // ========================================================================
    // Tally 데이터
    // ========================================================================

    /**
     * @brief 채널 Tally 상태 조회
     * @param channel 채널 번호 (1-based)
     * @return Tally 상태
     */
    virtual tally_status_t getChannelTally(uint8_t channel) const = 0;

    // ========================================================================
    // 제어
    // ========================================================================

    /**
     * @brief Cut (즉시 전환)
     */
    virtual void cut() = 0;

    /**
     * @brief Auto Transition (자동 전환)
     */
    virtual void autoTransition() = 0;

    /**
     * @brief Preview 소스 설정
     * @param source_id 소스 ID
     */
    virtual void setPreview(uint16_t source_id) = 0;

    // ========================================================================
    // 콜백 설정
    // ========================================================================

    /**
     * @brief Tally 변경 콜백 설정
     * @param callback 콜백 함수
     */
    virtual void setTallyCallback(std::function<void()> callback) = 0;

    /**
     * @brief 연결 상태 변경 콜백 설정
     * @param callback 콜백 함수
     */
    virtual void setConnectionCallback(std::function<void(connection_state_t)> callback) = 0;
};

#endif // __cplusplus

#endif // TALLY_TYPES_H
