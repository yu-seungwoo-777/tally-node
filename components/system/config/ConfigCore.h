/**
 * @file ConfigCore.h
 * @brief NVS 기반 설정 관리 Core API
 *
 * Core API 원칙:
 * - 하드웨어 추상화 (NVS)
 * - 상태 최소화 (메모리 캐시만 유지)
 * - 단일 책임 (설정 저장/불러오기)
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "switcher_types.h"
#include "button_poll.h"


/* ============================================================================
 * NVS 기본값 설정
 * ============================================================================
 * 모든 기본값은 이 섹션에서 관리합니다
 */

/* 공통 설정 (TX/RX) */
#define CONFIG_DEFAULT_DEVICE_NAME        "EoRa-S3"

/* TX 모드 전용 - 네트워크 */
#define CONFIG_DEFAULT_WIFI_STA_SSID      "HOME WIFI"
#define CONFIG_DEFAULT_WIFI_STA_PASS      "33333333"
#define CONFIG_DEFAULT_WIFI_AP_SSID       "ESP32_CONFIG"
#define CONFIG_DEFAULT_WIFI_AP_PASS       "12345678"
#define CONFIG_DEFAULT_ETH_DHCP           true
#define CONFIG_DEFAULT_ETH_STATIC_IP      "192.168.0.251"
#define CONFIG_DEFAULT_ETH_NETMASK        "255.255.255.0"
#define CONFIG_DEFAULT_ETH_GATEWAY        "192.168.0.1"

/* TX 모드 전용 - 시스템 */
#define CONFIG_DEFAULT_UDP_PORT           8888
#define CONFIG_DEFAULT_WEB_PORT           80
#define CONFIG_DEFAULT_DUAL_MODE          false

/* TX 모드 전용 - Primary 스위처 */
#define CONFIG_DEFAULT_SW0_TYPE           SWITCHER_TYPE_ATEM
#define CONFIG_DEFAULT_SW0_INTERFACE      SWITCHER_INTERFACE_WIFI_STA
#define CONFIG_DEFAULT_SW0_IP             "192.168.0.240"
#define CONFIG_DEFAULT_SW0_PORT           0
#define CONFIG_DEFAULT_SW0_PASSWORD       ""
#define CONFIG_DEFAULT_SW0_CAMERA_OFFSET  0
#define CONFIG_DEFAULT_SW0_CAMERA_LIMIT   0

/* TX 모드 전용 - Secondary 스위처 */
#define CONFIG_DEFAULT_SW1_TYPE           SWITCHER_TYPE_ATEM
#define CONFIG_DEFAULT_SW1_INTERFACE      SWITCHER_INTERFACE_ETHERNET
#define CONFIG_DEFAULT_SW1_IP             "192.168.0.241"
#define CONFIG_DEFAULT_SW1_PORT           0
#define CONFIG_DEFAULT_SW1_PASSWORD       ""
#define CONFIG_DEFAULT_SW1_CAMERA_OFFSET  4
#define CONFIG_DEFAULT_SW1_CAMERA_LIMIT   0

/* LoRa 설정 (공통) */
#define CONFIG_DEFAULT_LORA_FREQUENCY      868.0f
#define CONFIG_DEFAULT_LORA_SYNC_WORD     0x12

/* RX 모드 전용 */
#define CONFIG_DEFAULT_LED_BRIGHTNESS     255
#define CONFIG_DEFAULT_CAMERA_ID          1
#define CONFIG_DEFAULT_MAX_CAMERA_NUM     20

/* NVS 네임스페이스 */
#define NVS_NAMESPACE_WIFI                "wifi"
#define NVS_NAMESPACE_ETH                 "eth"
#define NVS_NAMESPACE_SYSTEM              "system"
#define NVS_NAMESPACE_SWITCHER            "switcher"
#define NVS_NAMESPACE_LORA                "lora"

/* 스위처 인덱스 */
typedef enum {
    SWITCHER_INDEX_PRIMARY = 0,
    SWITCHER_INDEX_SECONDARY = 1,
    SWITCHER_INDEX_MAX = 2
} switcher_index_t;

/* 통신 인터페이스 타입 */
typedef enum {
    SWITCHER_INTERFACE_NONE = 0,     // 비활성화
    SWITCHER_INTERFACE_WIFI_STA = 1, // WiFi STA
    SWITCHER_INTERFACE_ETHERNET = 2  // Ethernet
} switcher_interface_t;

/* 스위처 설정 */
struct ConfigSwitcher {
    switcher_type_t type;            // ATEM, vMix, OBS, OSEE
    switcher_interface_t interface;  // WiFi STA or Ethernet
    char ip[16];                     // IP 주소
    uint16_t port;                   // 포트 (0 = 기본값)
    char password[64];               // 비밀번호 (OBS용)
    uint8_t camera_offset;           // 카메라 시작 오프셋 (0-19)
    uint8_t camera_limit;            // 카메라 개수 제한 (0=자동)
};

/* WiFi STA 설정 */
struct ConfigWiFiSTA {
    char ssid[32];
    char password[64];
};

/* WiFi AP 설정 */
struct ConfigWiFiAP {
    char ssid[32];
    char password[64];
};

/* Ethernet 설정 */
struct ConfigEthernet {
    bool dhcp_enabled;
    char static_ip[16];
    char static_netmask[16];
    char static_gateway[16];
};

/* LoRa 설정 (공통) */
struct ConfigLoRa {
    float frequency;     // 주파수 (MHz)
    uint8_t sync_word;   // 싱크 워드
};

/* 공통 시스템 설정 (device_name만 공통) */
struct ConfigSystemCommon {
    char device_name[32];
};

/* 전체 설정 */
struct Config {
    ConfigSystemCommon system;
    ConfigLoRa lora;  // LoRa 설정 (TX/RX 공통)

#ifdef DEVICE_MODE_TX
    // TX 전용 네트워크 설정
    ConfigWiFiSTA wifi_sta;
    ConfigWiFiAP wifi_ap;
    ConfigEthernet eth;

    // TX 전용 시스템 설정
    uint16_t udp_port;
    uint16_t web_port;
    bool dual_mode;          // 듀얼 모드 (Primary/Secondary 스위처 동시 사용)

    // TX 전용 스위처 설정
    ConfigSwitcher switchers[SWITCHER_INDEX_MAX];
#endif

#ifdef DEVICE_MODE_RX
    // RX 전용 시스템 설정
    uint8_t led_brightness;  // LED 밝기 (1-255)
    uint8_t camera_id;       // 카메라 ID (0-9)
    uint8_t max_camera_num;  // 최대 카메라 수 (1-20)
#endif
};

/**
 * @brief NVS 기반 설정 관리 Core
 *
 * 설계 원칙:
 * - 상태: 메모리 캐시만 유지 (g_config)
 * - 스레드 안전성: 싱글톤 패턴, init() 후 사용
 * - 성능: Cold Path (초기화, 설정 변경)
 */
class ConfigCore {
public:
    /**
     * @brief 초기화 및 설정 로드
     *
     * NVS에서 설정을 읽어 메모리에 캐싱합니다.
     * NVS에 설정이 없으면 기본값으로 초기화합니다.
     */
    static esp_err_t init();

    /**
     * @brief 전체 설정 가져오기
     */
    static const Config& getAll();

    #ifdef DEVICE_MODE_TX
    /**
     * @brief WiFi STA 설정 가져오기
     */
    static const ConfigWiFiSTA& getWiFiSTA();

    /**
     * @brief WiFi AP 설정 가져오기
     */
    static const ConfigWiFiAP& getWiFiAP();

    /**
     * @brief Ethernet 설정 가져오기
     */
    static const ConfigEthernet& getEthernet();
#endif

    /**
     * @brief 시스템 설정 가져오기
     */
    static const ConfigSystemCommon& getSystem();

    #ifdef DEVICE_MODE_TX
    /**
     * @brief WiFi STA 설정 저장
     *
     * NVS에 저장하고 메모리 캐시를 업데이트합니다.
     */
    static esp_err_t setWiFiSTA(const ConfigWiFiSTA& config);

    /**
     * @brief WiFi AP 설정 저장
     *
     * 비밀번호는 최소 8자 이상이어야 합니다.
     */
    static esp_err_t setWiFiAP(const ConfigWiFiAP& config);

    /**
     * @brief Ethernet 설정 저장
     */
    static esp_err_t setEthernet(const ConfigEthernet& config);
#endif

    #ifdef DEVICE_MODE_TX
    /**
     * @brief 스위처 설정 가져오기
     *
     * @param index 스위처 인덱스 (PRIMARY or SECONDARY)
     * @return 스위처 설정
     */
    static ConfigSwitcher getSwitcher(switcher_index_t index);

    /**
     * @brief 스위처 설정 저장
     *
     * @param index 스위처 인덱스
     * @param config 스위처 설정
     * @return 성공 시 ESP_OK
     */
    static esp_err_t setSwitcher(switcher_index_t index, const ConfigSwitcher& config);
#endif

    #ifdef DEVICE_MODE_TX
    /**
     * @brief 듀얼 모드 설정 가져오기
     *
     * @return true: 듀얼 모드 (Primary + Secondary), false: 싱글 모드 (Primary만)
     */
    static bool getDualMode();

    /**
     * @brief 듀얼 모드 설정 저장
     *
     * @param dual_mode true: 듀얼 모드, false: 싱글 모드
     * @return 성공 시 ESP_OK
     */
    static esp_err_t setDualMode(bool dual_mode);
#else
    /**
     * @brief 듀얼 모드 설정 가져오기 (RX 모드)
     *
     * @return 항상 false (RX 모드에서는 듀얼 모드 미지원)
     */
    static bool getDualMode();
#endif

    /**
     * @brief LoRa 설정 가져오기
     *
     * @return LoRa 설정
     */
    static ConfigLoRa getLoRa();

    /**
     * @brief LoRa 설정 저장
     *
     * @param config LoRa 설정
     * @return 성공 시 ESP_OK
     */
    static esp_err_t setLoRa(const ConfigLoRa& config);

    #ifdef DEVICE_MODE_RX
    /**
     * @brief 카메라 ID 설정 가져오기
     *
     * @return 카메라 ID (0-9)
     */
    static uint8_t getCameraId();

    /**
     * @brief 카메라 ID 설정 저장
     *
     * @param camera_id 카메라 ID (0-9)
     * @return 성공 시 ESP_OK
     */
    static esp_err_t setCameraId(uint8_t camera_id);

    /**
     * @brief 최대 카메라 수 설정 가져오기
     *
     * @return 최대 카메라 수 (1-20)
     */
    static uint8_t getMaxCameraNum();

    /**
     * @brief 최대 카메라 수 설정 저장
     *
     * @param max_camera_num 최대 카메라 수 (1-20)
     * @return 성공 시 ESP_OK
     */
    static esp_err_t setMaxCameraNum(uint8_t max_camera_num);
#endif

  
  /**
     * @brief 공장 초기화
     *
     * NVS를 삭제하고 기본값으로 재설정합니다.
     */
    static esp_err_t factoryReset();

private:
    // 싱글톤 패턴 - 생성자 비공개
    ConfigCore() = delete;
    ~ConfigCore() = delete;
    ConfigCore(const ConfigCore&) = delete;
    ConfigCore& operator=(const ConfigCore&) = delete;

    // 내부 구현
    static void loadDefaults();
    static esp_err_t loadWiFiSTA();
    static esp_err_t loadWiFiAP();
    static esp_err_t loadEthernet();
    static esp_err_t loadSystem();
    static esp_err_t loadLoRa();
    static esp_err_t loadSwitcher(switcher_index_t index);

    // 메모리 캐시
    static Config s_config;
#ifdef DEVICE_MODE_TX
    static ConfigSwitcher s_switchers[SWITCHER_INDEX_MAX];
#endif
    static bool s_initialized;
    static uint64_t s_last_button_time;  // 버튼 디바운싱용 (더 이상 사용 안 함)
    static uint8_t s_button_id;        // button_core 컴포넌트 버튼 ID
};

/* ============================================================================
 * C 인터페이스 (button_actions.c에서 사용)
 * ============================================================================
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEVICE_MODE_RX
/**
 * @brief 카메라 ID 가져오기 (C 호환)
 */
uint8_t config_get_camera_id(void);

/**
 * @brief 카메라 ID 설정 (C 호환)
 */
esp_err_t config_set_camera_id(uint8_t camera_id);

/**
 * @brief 최대 카메라 수 가져오기 (C 호환)
 */
uint8_t config_get_max_camera_num(void);

/**
 * @brief 최대 카메라 수 설정 (C 호환)
 */
esp_err_t config_set_max_camera_num(uint8_t max_camera_num);
#endif

#ifdef __cplusplus
}
#endif
