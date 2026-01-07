/**
 * @file event_bus.h
 * @brief 이벤트 버스 - 레이어 간 결합도 제거를 위한 비동기 이벤트 시스템
 *
 * 레이어 아키텍처 준수를 위한 통신 메커니즘:
 * - 상위 레이어는 이벤트를 발행(Publish)만 하고, 누가 구독하는지 모름
 * - 하위 레이어는 이벤트를 구독(Subscribe)하고 반응
 * - 단방향 통신: 01_app → 02 → 03 → 04 → 05
 *
 * @section event_data_usage 이벤트 데이터 사용 가이드
 *
 * 이벤트 데이터 포인터는 발행 직후에 해제될 수 있으므로, 데이터를 이벤트로 전달할 때는
 * 반드시 **정적(static) 구조체**를 사용해야 합니다:
 *
 * @code
 * // 올바른 사용 예시 (정적 구조체)
 * static switcher_status_event_t s_status;  // 정적 변수
 * s_status.dual_mode = true;
 * s_status.s1_connected = false;
 * event_bus_publish(EVT_SWITCHER_STATUS_CHANGED, &s_status, sizeof(s_status));
 *
 * // 잘못된 사용 예시 (스택 구조체)
 * switcher_status_event_t status;  // 스택 변수 (함수 반환 후 소멸)
 * status.dual_mode = true;
 * event_bus_publish(EVT_SWITCHER_STATUS_CHANGED, &status, sizeof(status));  // 위험!
 * @endcode
 *
 * @note 정적 구조체 사용 이유:
 *       - event_bus_publish()는 내부적으로 데이터를 복사하지만
 *       - 콜백이 비동기적으로 호출되는 경우 �택 변수가 이미 소멸될 수 있음
 *       - static 변수는 함수 수명 주기와 무관하게 메모리에 유지됨
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

#define LORA_DEVICE_ID_LEN 2

/**
 * @brief 디바이스 등록 이벤트 데이터 (EVT_DEVICE_REGISTER용)
 */
typedef struct {
    uint8_t device_id[LORA_DEVICE_ID_LEN];  // 2바이트 디바이스 ID (MAC[4]+MAC[5])
} device_register_event_t;

/**
 * @brief 단일 디바이스 정보 (device_list_event_t 내부)
 */
typedef struct __attribute__((packed)) {
    uint8_t device_id[2];     ///< 디바이스 ID (2바이트, MAC[4]+MAC[5])
    int16_t last_rssi;        ///< 마지막 RSSI (dBm)
    int8_t last_snr;          ///< 마지막 SNR (dB)
    uint8_t battery;          ///< 배터리 %
    uint8_t camera_id;        ///< 카메라 ID
    uint32_t uptime;          ///< 업타임 (초)
    uint8_t brightness;       ///< 밝기 0-255
    bool is_stopped;          ///< 기능 정지 상태
    bool is_online;           ///< 온라인 상태 (true=온라인, false=오프라인)
    uint32_t last_seen;       ///< 마지막 수신 시간 (tick)
    uint16_t ping_ms;         ///< 지연시간 (ms)
    float frequency;          ///< 현재 주파수 (MHz)
    uint8_t sync_word;        ///< 현재 sync word
} device_info_t;

/**
 * @brief 디바이스 리스트 이벤트 데이터 (EVT_DEVICE_LIST_CHANGED용)
 */
typedef struct {
    device_info_t devices[20]; ///< 온라인 디바이스 배열
    uint8_t count;             ///< 디바이스 수
    uint8_t registered_count;  ///< 등록된 총 디바이스 수
} device_list_event_t;

/**
 * @brief LoRa 패킷 이벤트 데이터 (EVT_LORA_PACKET_RECEIVED용)
 */
typedef struct __attribute__((packed)) {
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
 * @brief 채널 정보 (LoRa 스캔 결과)
 */
typedef struct __attribute__((packed)) {
    float frequency;     ///< 주파수 (MHz)
    int16_t rssi;        ///< RSSI (dBm)
    int16_t noise_floor; ///< 노이즈 플로어 (dBm)
    bool clear_channel;  ///< 깨끗한 채널 여부
} lora_channel_info_t;

/**
 * @brief LoRa 스캔 시작 이벤트 데이터 (EVT_LORA_SCAN_START용)
 */
typedef struct {
    float start_freq;  ///< 시작 주파수 (MHz)
    float end_freq;    ///< 종료 주파수 (MHz)
    float step;        ///< 스캔 간격 (MHz)
} lora_scan_start_t;

/**
 * @brief LoRa 스캔 진행 이벤트 데이터 (EVT_LORA_SCAN_PROGRESS용)
 */
typedef struct {
    uint8_t progress;           ///< 진행률 (0-100)
    float current_freq;         ///< 현재 스캔 중인 주파수 (MHz)
    lora_channel_info_t result; ///< 현재 채널 결과
} lora_scan_progress_t;

/**
 * @brief LoRa 스캔 완료 이벤트 데이터 (EVT_LORA_SCAN_COMPLETE용)
 */
typedef struct {
    lora_channel_info_t channels[100]; ///< 스캔 결과 (최대 100 채널)
    uint8_t count;                     ///< 스캔된 채널 수
} lora_scan_complete_t;

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
    uint8_t lora_chip_type;  ///< LoRa 칩 타입 (0=Unknown, 1=SX1262, 2=SX1268)
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
    bool eth_detected;       ///< W5500 칩 감지 여부
    bool eth_dhcp;           ///< Ethernet DHCP 모드
} network_status_event_t;

/**
 * @brief 네트워크 재시작 요청 타입
 */
typedef enum {
    NETWORK_RESTART_WIFI_AP = 0,      ///< WiFi AP 재시작
    NETWORK_RESTART_WIFI_STA,         ///< WiFi STA 재연결 (AP 유지)
    NETWORK_RESTART_ETHERNET,         ///< Ethernet 재시작
    NETWORK_RESTART_ALL,              ///< 전체 네트워크 재시작
} network_restart_type_t;

/**
 * @brief 네트워크 재시작 요청 이벤트 데이터 (EVT_NETWORK_RESTART_REQUEST용)
 *
 * web_server에서 발행하는 네트워크 재시작 요청
 */
typedef struct {
    network_restart_type_t type;  ///< 재시작 타입
    char ssid[33];                ///< SSID (STA 재연결용)
    char password[65];             ///< 비밀번호 (STA 재연결용)
} network_restart_request_t;

/**
 * @brief 설정 저장 요청 타입
 */
typedef enum {
    CONFIG_SAVE_WIFI_AP = 0,          ///< WiFi AP 설정 저장
    CONFIG_SAVE_WIFI_STA,             ///< WiFi STA 설정 저장
    CONFIG_SAVE_ETHERNET,             ///< Ethernet 설정 저장
    CONFIG_SAVE_SWITCHER_PRIMARY,     ///< Primary Switcher 설정 저장
    CONFIG_SAVE_SWITCHER_SECONDARY,   ///< Secondary Switcher 설정 저장
    CONFIG_SAVE_SWITCHER_DUAL,        ///< Dual 모드 설정 저장
    CONFIG_SAVE_DEVICE_BRIGHTNESS,    ///< 밝기 설정 저장
    CONFIG_SAVE_DEVICE_CAMERA_ID,     ///< 카메라 ID 설정 저장
    CONFIG_SAVE_DEVICE_RF,            ///< RF 설정 저장
} config_save_type_t;

/**
 * @brief 설정 저장 요청 이벤트 데이터 (EVT_CONFIG_CHANGED용)
 *
 * web_server에서 발행하는 설정 저장 요청
 */
typedef struct {
    config_save_type_t type;     ///< 저장 타입

    // WiFi AP
    char wifi_ap_ssid[33];       ///< AP SSID
    char wifi_ap_password[65];   ///< AP 비밀번호
    uint8_t wifi_ap_channel;     ///< AP 채널
    bool wifi_ap_enabled;        ///< AP 활성화

    // WiFi STA
    char wifi_sta_ssid[33];      ///< STA SSID
    char wifi_sta_password[65];  ///< STA 비밀번호
    bool wifi_sta_enabled;       ///< STA 활성화

    // Ethernet
    bool eth_dhcp;               ///< DHCP 모드
    char eth_static_ip[16];      ///< 고정 IP
    char eth_netmask[16];        ///< 넷마스크
    char eth_gateway[16];        ///< 게이트웨이
    bool eth_enabled;            ///< Ethernet 활성화

    // Switcher
    char switcher_type[8];       ///< Switcher 타입 ("ATEM", "OBS", "vMix")
    char switcher_ip[16];        ///< Switcher IP
    uint16_t switcher_port;      ///< Switcher 포트
    uint8_t switcher_interface;  ///< 네트워크 인터페이스 (0=Auto, 1=WiFi, 2=Ethernet)
    uint8_t switcher_camera_limit; ///< 카메라 제한 (0=무제한)
    char switcher_password[64];  ///< Switcher 비밀번호 (OBS용)
    bool switcher_dual_enabled;  ///< 듀얼 모드 활성화
    uint8_t switcher_secondary_offset; ///< Secondary 오프셋

    // Device
    uint8_t brightness;          ///< 밝기 (0-255)
    uint8_t camera_id;           ///< 카메라 ID (1-20)
    float rf_frequency;          ///< RF 주파수 (MHz)
    uint8_t rf_sync_word;        ///< RF Sync Word
} config_save_request_t;

/**
 * @brief 설정 데이터 이벤트 (EVT_CONFIG_DATA_CHANGED용)
 *
 * config_service에서 발행하는 전체 설정 데이터
 * web_server에서 캐시용으로 사용
 */
typedef struct __attribute__((packed)) {
    // WiFi AP
    char wifi_ap_ssid[33];
    char wifi_ap_password[65];   ///< AP 비밀번호
    uint8_t wifi_ap_channel;
    bool wifi_ap_enabled;

    // WiFi STA
    char wifi_sta_ssid[33];
    char wifi_sta_password[65];  ///< STA 비밀번호
    bool wifi_sta_enabled;

    // Ethernet
    bool eth_dhcp_enabled;
    char eth_static_ip[16];
    char eth_static_netmask[16];
    char eth_static_gateway[16];
    bool eth_enabled;

    // Device
    uint8_t device_brightness;
    uint8_t device_camera_id;
    float device_rf_frequency;
    uint8_t device_rf_sync_word;
    uint8_t device_rf_sf;         // Spreading Factor
    uint8_t device_rf_cr;          // Coding Rate
    float device_rf_bw;           // Bandwidth
    int8_t device_rf_tx_power;    // TX Power

    // Switcher Primary
    uint8_t primary_type;        // 0=ATEM, 1=OBS, 2=vMix
    char primary_ip[16];
    uint16_t primary_port;
    uint8_t primary_interface;
    uint8_t primary_camera_limit;
    char primary_password[64];   // OBS 비밀번호

    // Switcher Secondary
    uint8_t secondary_type;      // 0=ATEM, 1=OBS, 2=vMix
    char secondary_ip[16];
    uint16_t secondary_port;
    uint8_t secondary_interface;
    uint8_t secondary_camera_limit;
    char secondary_password[64]; // OBS 비밀번호

    // Switcher Dual
    bool dual_enabled;
    uint8_t secondary_offset;
} config_data_event_t;

/**
 * @brief 라이센스 상태 이벤트 데이터 (EVT_LICENSE_STATE_CHANGED용)
 *
 * license_service에서 발행하는 라이센스 상태 변경 정보
 */
typedef struct __attribute__((packed)) {
    uint8_t device_limit;      ///< 0 = 미등록, 1~255 = 제한
    uint8_t state;             ///< 라이센스 상태 (license_state_t)
    uint32_t grace_remaining;  ///< 유예 기간 남은 시간 (초)
} license_state_event_t;

/**
 * @brief 라이센스 검증 요청 이벤트 데이터 (EVT_LICENSE_VALIDATE용)
 *
 * web_server에서 발행하는 라이센스 키 검증 요청
 */
typedef struct {
    char key[17];              ///< 라이센스 키 (16자 + null)
} license_validate_event_t;

// ============================================================================
// 이벤트 타입 정의
// ============================================================================

/**
 * @brief 이벤트 타입 정의
 */
typedef enum {
    // 시스템 이벤트 (01_app)
    EVT_SYSTEM_READY = 0,
    EVT_CONFIG_CHANGED,         ///< 설정 저장 요청 (data: config_save_request_t)
    EVT_CONFIG_DATA_CHANGED,    ///< 설정 데이터 변경 (data: config_data_event_t)
    EVT_CONFIG_DATA_REQUEST,    ///< 설정 데이터 요청 (data: 없음, 응답으로 EVT_CONFIG_DATA_CHANGED 발행)
    EVT_BRIGHTNESS_CHANGED,     ///< 밝기 설정 변경 (data: uint8_t, 0-255)
    EVT_CAMERA_ID_CHANGED,      ///< 카메라 ID 변경 (data: uint8_t, 1-20)
    EVT_RF_CHANGED,             ///< RF 설정 변경 (data: lora_rf_event_t) - 드라이버 적용 + broadcast
    EVT_RF_SAVED,               ///< RF NVS 저장 요청 (data: lora_rf_event_t) - broadcast 완료 후
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
    EVT_LORA_TX_COMMAND,           ///< TX→RX 명령 수신 (0xE0~0xEF) (data: lora_packet_event_t)
    EVT_LORA_RX_RESPONSE,          ///< RX→TX 응답 수신 (0xD0~0xDF) (data: lora_packet_event_t)
    EVT_LORA_PACKET_RECEIVED,      ///< Packet received (data: lora_packet_event_t)
    EVT_LORA_PACKET_SENT,
    EVT_LORA_SEND_REQUEST,         ///< Send request (data: lora_send_request_t)
    EVT_LORA_SCAN_START,           ///< Scan start request (data: lora_scan_start_t)
    EVT_LORA_SCAN_PROGRESS,        ///< Scan progress (data: lora_scan_progress_t)
    EVT_LORA_SCAN_COMPLETE,        ///< Scan complete (data: lora_scan_complete_t)
    EVT_LORA_SCAN_STOP,            ///< Scan stop request (data: none)

    // Network events (03_service)
    EVT_NETWORK_STATUS_CHANGED,   ///< Network status changed (data: network_status_event_t)
    EVT_NETWORK_CONNECTED,
    EVT_NETWORK_DISCONNECTED,
    EVT_NETWORK_RESTART_REQUEST,  ///< Network restart request (data: network_restart_request_t)

    // Switcher events (03_service)
    EVT_SWITCHER_CONNECTED,
    EVT_SWITCHER_DISCONNECTED,
    EVT_SWITCHER_STATUS_CHANGED,  ///< Switcher status changed (data: switcher_status_event_t)
    EVT_TALLY_STATE_CHANGED,      ///< Tally state changed (data: tally_event_data_t)

    // UI 이벤트 (02_presentation)
    EVT_DISPLAY_UPDATE_REQUEST,

    // LED 이벤트 (02_presentation)
    EVT_LED_STATE_CHANGED,

    // 디바이스 관리 이벤트 (03_service → ConfigService)
    EVT_DEVICE_REGISTER,         ///< 디바이스 등록 요청 (data: device_register_event_t)
    EVT_DEVICE_UNREGISTER,       ///< 디바이스 등록 해제 요청 (data: device_register_event_t)
    EVT_DEVICE_LIST_CHANGED,     ///< 디바이스 리스트 변경 (data: device_list_event_t)
    EVT_DEVICE_BRIGHTNESS_REQUEST, ///< 디바이스 밝기 설정 요청 (data: uint8_t[3] = {device_id[0], device_id[1], brightness})
    EVT_DEVICE_CAMERA_ID_REQUEST, ///< 디바이스 카메라 ID 설정 요청 (data: uint8_t[3] = {device_id[0], device_id[1], camera_id})
    EVT_DEVICE_PING_REQUEST,     ///< 디바이스 PING 요청 (data: uint8_t[2] = {device_id[0], device_id[1]})

    // 라이센스 이벤트 (03_service → 03_service)
    EVT_LICENSE_STATE_CHANGED,   ///< 라이센스 상태 변경 (data: license_state_event_t)
    EVT_LICENSE_VALIDATE,        ///< 라이센스 검증 요청 (data: license_validate_event_t)

    // 최대 이벤트 수
   _EVT_MAX
} event_type_t;

/**
 * @brief 이벤트 버스 내부 버퍼 크기
 *
 * 가장 큰 이벤트 데이터: lora_scan_complete_t (약 901바이트)
 * 2048바이트로 할당하여 여유 있게 처리
 */
#define EVENT_DATA_BUFFER_SIZE 2048

/**
 * @brief 이벤트 데이터 구조체 (내부 버퍼 방식)
 *
 * 데이터 포인터 대신 내부 버퍼에 값을 복사하여 저장합니다.
 * 발행자는 stack 변수를 안전하게 사용할 수 있습니다.
 */
typedef struct {
    event_type_t type;                 ///< 이벤트 타입
    uint8_t data[EVENT_DATA_BUFFER_SIZE]; ///< 내부 데이터 버퍼 (복사본)
    size_t data_size;                  ///< 실제 데이터 크기
    uint32_t timestamp;                ///< 타임스탬프 (ms)
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
