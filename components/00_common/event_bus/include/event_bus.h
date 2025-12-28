/**
 * @file event_bus.h
 * @brief 이벤트 버스 - 레이어 간 결합도 제거를 위한 비동기 이벤트 시스템
 *
 * 레이어 아키텍처 준수를 위한 통신 메커니즘:
 * - 상위 레이어는 이벤트를 발행(Publish)만 하고, 누가 구독하는지 모름
 * - 하위 레이어는 이벤트를 구독(Subscribe)하고 반응
 * - 단방향 통신: 01_app → 02 → 03 → 04 → 05
 */

#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 이벤트 데이터 구조체 (공통)
// ============================================================================

/**
 * @brief LoRa RSSI/SNR 이벤트 데이터 (EVT_LORA_RSSI_CHANGED용)
 *
 * @note packed 속성으로 패딩 방지 (모든 컴파일러에서 동일한 레이아웃 보장)
 */
typedef struct __attribute__((packed)) {
    bool is_running;
    bool is_initialized;
    uint8_t chip_type;       // 0=Unknown, 1=SX1262, 2=SX1268
    float frequency;         // MHz
    int16_t rssi;            // dBm
    int8_t snr;              // dB
} lora_rssi_event_t;

#define LORA_MAX_PACKET_SIZE 256

/**
 * @brief LoRa 송신 요청 이벤트 데이터 (EVT_LORA_SEND_REQUEST용)
 */
typedef struct {
    const uint8_t* data;     // 패킷 데이터 (상수 포인터 - 발행 후 해제 가능)
    size_t length;           // 데이터 길이
} lora_send_request_t;

#define LORA_DEVICE_ID_LEN 4

/**
 * @brief 디바이스 등록 이벤트 데이터 (EVT_DEVICE_REGISTER용)
 */
typedef struct {
    uint8_t device_id[LORA_DEVICE_ID_LEN];  // 4바이트 디바이스 ID
} device_register_event_t;

/**
 * @brief LoRa 패킷 이벤트 데이터 (EVT_LORA_PACKET_RECEIVED용)
 */
typedef struct {
    uint8_t data[LORA_MAX_PACKET_SIZE];  // 패킷 데이터
    size_t length;           // 데이터 길이
    int16_t rssi;            // dBm
    float snr;               // dB
} lora_packet_event_t;

/**
 * @brief Tally 상태 이벤트 데이터 (EVT_TALLY_STATE_CHANGED용)
 */
typedef struct __attribute__((packed)) {
    uint8_t source;          // 스위처 소스 (0=Primary, 1=Secondary)
    uint8_t channel_count;   // 채널 수 (1-20)
    uint8_t tally_data[8];   // Packed 데이터 (최대 20채널 = 5바이트)
    uint64_t tally_value;    // 64비트 packed 값
} tally_event_data_t;

/**
 * @brief RF 설정 이벤트 데이터 (EVT_RF_CHANGED용)
 */
typedef struct {
    float frequency;    ///< 주파수 (MHz)
    uint8_t sync_word;  ///< Sync Word
} lora_rf_event_t;

/**
 * @brief 시스템 정보 이벤트 데이터 (EVT_INFO_UPDATED용)
 *
 * hardware_service에서 발행하는 시스템 상태 정보
 */
typedef struct __attribute__((packed)) {
    char device_id[5];       ///< 디바이스 ID (4자리 hex 문자열)
    uint8_t battery;         ///< 배터리 % (0-100)
    float voltage;           ///< 전압 (V)
    float temperature;       ///< 온도 (°C)
    int16_t rssi;            ///< LoRa RSSI (dBm)
    float snr;               ///< LoRa SNR (dB)
    uint32_t uptime;         ///< 업타임 (초)
    bool stopped;            ///< 기능 정지 상태
} system_info_event_t;

/**
 * @brief 스위처 상태 이벤트 데이터 (EVT_SWITCHER_STATUS_CHANGED용)
 *
 * switcher_service에서 발행하는 스위처 연결 상태 정보
 */
typedef struct __attribute__((packed)) {
    bool dual_mode;          ///< 듀얼 모드 활성화 여부
    bool s1_connected;       ///< Primary 연결 여부
    bool s2_connected;       ///< Secondary 연결 여부
    char s1_type[8];         ///< Primary 타입 ("ATEM", "OBS", "vMix", "NONE")
    char s2_type[8];         ///< Secondary 타입
    char s1_ip[16];          ///< Primary IP 주소
    char s2_ip[16];          ///< Secondary IP 주소
    uint16_t s1_port;        ///< Primary 포트
    uint16_t s2_port;        ///< Secondary 포트

    // Tally 데이터 (결합 전 개별 상태)
    uint8_t s1_channel_count; ///< Primary 채널 수
    uint8_t s1_tally_data[8]; ///< Primary packed 데이터
    uint8_t s2_channel_count; ///< Secondary 채널 수
    uint8_t s2_tally_data[8]; ///< Secondary packed 데이터
} switcher_status_event_t;

/**
 * @brief 네트워크 상태 이벤트 데이터 (EVT_NETWORK_STATUS_CHANGED용)
 *
 * network_service에서 발행하는 네트워크 상태 정보
 */
typedef struct __attribute__((packed)) {
    // WiFi AP (Page 2)
    char ap_ssid[33];        ///< AP SSID
    char ap_ip[16];          ///< AP IP 주소
    bool ap_enabled;         ///< AP 활성화 여부

    // WiFi STA (Page 3)
    char sta_ssid[33];       ///< STA SSID
    char sta_ip[16];         ///< STA IP 주소
    bool sta_connected;      ///< STA 연결 여부

    // Ethernet (Page 4)
    char eth_ip[16];         ///< Ethernet IP 주소
    bool eth_connected;      ///< Ethernet 연결 여부
    bool eth_dhcp;           ///< Ethernet DHCP 모드
} network_status_event_t;

// ============================================================================
// 이벤트 타입 정의
// ============================================================================

/**
 * @brief 이벤트 타입 정의
 */
typedef enum {
    // 시스템 이벤트 (01_app)
    EVT_SYSTEM_READY = 0,
    EVT_CONFIG_CHANGED,
    EVT_BRIGHTNESS_CHANGED,     ///< 밝기 설정 변경 (data: uint8_t, 0-255)
    EVT_CAMERA_ID_CHANGED,      ///< 카메라 ID 변경 (data: uint8_t, 1-20)
    EVT_RF_CHANGED,             ///< RF 설정 변경 (data: lora_rf_event_t)
    EVT_STOP_CHANGED,           ///< 기능 정지 상태 변경 (data: bool, true=정지)

    // 버튼 이벤트 (03_service → 01_app)
    EVT_BUTTON_SINGLE_CLICK,    ///< 버튼 단일 클릭 (짧게 눌름)
    EVT_BUTTON_LONG_PRESS,      ///< 버튼 롱프레스 시작 (1000ms)
    EVT_BUTTON_LONG_RELEASE,    ///< 버튼 롱프레스 해제

    // 정보/상태 이벤트
    EVT_INFO_UPDATED,

    // LoRa 이벤트 (03_service)
    EVT_LORA_STATUS_CHANGED,
    EVT_LORA_RSSI_CHANGED,         ///< RSSI/SNR 변경 (data: lora_rssi_event_t)
    EVT_LORA_PACKET_RECEIVED,      ///< 패킷 수신 (data: lora_packet_event_t)
    EVT_LORA_PACKET_SENT,
    EVT_LORA_SEND_REQUEST,         ///< 송신 요청 (data: lora_send_request_t)

    // 네트워크 이벤트 (03_service)
    EVT_NETWORK_STATUS_CHANGED,   ///< 네트워크 상태 변경 (data: network_status_event_t)
    EVT_NETWORK_CONNECTED,
    EVT_NETWORK_DISCONNECTED,

    // 스위처 이벤트 (03_service)
    EVT_SWITCHER_CONNECTED,
    EVT_SWITCHER_DISCONNECTED,
    EVT_SWITCHER_STATUS_CHANGED,  ///< 스위처 상태 변경 (data: switcher_status_event_t)
    EVT_TALLY_STATE_CHANGED,      ///< Tally 상태 변경 (data: tally_event_data_t)

    // UI 이벤트 (02_presentation)
    EVT_DISPLAY_UPDATE_REQUEST,

    // LED 이벤트 (02_presentation)
    EVT_LED_STATE_CHANGED,

    // 디바이스 관리 이벤트 (03_service → ConfigService)
    EVT_DEVICE_REGISTER,         ///< 디바이스 등록 요청 (data: device_register_event_t)
    EVT_DEVICE_UNREGISTER,       ///< 디바이스 등록 해제 요청 (data: device_register_event_t)

    // 최대 이벤트 수
   _EVT_MAX
} event_type_t;

/**
 * @brief 이벤트 데이터 구조체
 */
typedef struct {
    event_type_t type;        ///< 이벤트 타입
    const void* data;         ///< 이벤트 데이터 (상수 포인터 - 수명 주의)
    size_t data_size;         ///< 데이터 크기
    uint32_t timestamp;       ///< 타임스탬프 (ms)
} event_data_t;

/**
 * @brief 이벤트 콜백 함수 타입
 *
 * @param event 이벤트 데이터
 * @return esp_err_t ESP_OK 또는 에러 코드
 */
typedef esp_err_t (*event_callback_t)(const event_data_t* event);

/**
 * @brief 이벤트 버스 초기화
 *
 * @return esp_err_t ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t event_bus_init(void);

/**
 * @brief 이벤트 발행 (Publish)
 *
 * @param type 이벤트 타입
 * @param data 이벤트 데이터 (NULL 가능)
 * @param data_size 데이터 크기
 * @return esp_err_t ESP_OK 성공
 *
 * @note data 포인터는 발행 직후에 해제되어도 됨 (내부 복사)
 */
esp_err_t event_bus_publish(event_type_t type, const void* data, size_t data_size);

/**
 * @brief 이벤트 구독 (Subscribe)
 *
 * @param type 이벤트 타입
 * @param callback 콜백 함수
 * @return esp_err_t ESP_OK 성공
 *
 * @note 콜백은 컨텍스트가 보장됨 (FreeRTOS task 내부)
 */
esp_err_t event_bus_subscribe(event_type_t type, event_callback_t callback);

/**
 * @brief 이벤트 구독 취소
 *
 * @param type 이벤트 타입
 * @param callback 콜백 함수
 * @return esp_err_t ESP_OK 성공
 */
esp_err_t event_bus_unsubscribe(event_type_t type, event_callback_t callback);

/**
 * @brief 이벤트 타입 이름 가져오기 (디버깅용)
 *
 * @param type 이벤트 타입
 * @return const char* 이벤트 이름 문자열
 */
const char* event_type_to_string(event_type_t type);

#ifdef __cplusplus
}
#endif

#endif // EVENT_BUS_H
