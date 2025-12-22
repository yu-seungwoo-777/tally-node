/**
 * @file ConfigCore.cpp
 * @brief NVS 기반 설정 관리 Core 구현
 */

#include "ConfigCore.h"
#include "log.h"
#include "log_tags.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// button_core (system 컴포넌트의 일부)
#ifdef DEVICE_MODE_RX
#include "button_poll.h"
#endif

// TX 모드에서 DisplayManager 함수 사용
#ifdef DEVICE_MODE_TX
extern "C" {
    void DisplayManager_onSwitcherConfigChanged(void);
}
#endif

static const char* TAG = TAG_CONFIG;


/* NVS 네임스페이스 */
// NVS 네임스페이스와 기본값은 ConfigCore.h에서 관리

// 정적 멤버 초기화
Config ConfigCore::s_config = {};
#ifdef DEVICE_MODE_TX
ConfigSwitcher ConfigCore::s_switchers[SWITCHER_INDEX_MAX] = {};
#endif
bool ConfigCore::s_initialized = false;
uint64_t ConfigCore::s_last_button_time = 0;  // 더 이상 사용 안 함
uint8_t ConfigCore::s_button_id = 0;          // button_core 컴포넌트 버튼 ID

void ConfigCore::loadDefaults()
{
    // 공통 설정
    strncpy(s_config.system.device_name, CONFIG_DEFAULT_DEVICE_NAME, sizeof(s_config.system.device_name) - 1);

    // LoRa 설정
    s_config.lora.frequency = CONFIG_DEFAULT_LORA_FREQUENCY;
    s_config.lora.sync_word = CONFIG_DEFAULT_LORA_SYNC_WORD;

#ifdef DEVICE_MODE_TX
    // WiFi STA
    strncpy(s_config.wifi_sta.ssid, CONFIG_DEFAULT_WIFI_STA_SSID, sizeof(s_config.wifi_sta.ssid) - 1);
    strncpy(s_config.wifi_sta.password, CONFIG_DEFAULT_WIFI_STA_PASS, sizeof(s_config.wifi_sta.password) - 1);

    // WiFi AP
    strncpy(s_config.wifi_ap.ssid, CONFIG_DEFAULT_WIFI_AP_SSID, sizeof(s_config.wifi_ap.ssid) - 1);
    strncpy(s_config.wifi_ap.password, CONFIG_DEFAULT_WIFI_AP_PASS, sizeof(s_config.wifi_ap.password) - 1);

    // Ethernet
    s_config.eth.dhcp_enabled = CONFIG_DEFAULT_ETH_DHCP;
    strncpy(s_config.eth.static_ip, CONFIG_DEFAULT_ETH_STATIC_IP, sizeof(s_config.eth.static_ip) - 1);
    strncpy(s_config.eth.static_netmask, CONFIG_DEFAULT_ETH_NETMASK, sizeof(s_config.eth.static_netmask) - 1);
    strncpy(s_config.eth.static_gateway, CONFIG_DEFAULT_ETH_GATEWAY, sizeof(s_config.eth.static_gateway) - 1);

    // TX 전용 시스템 설정
    s_config.udp_port = CONFIG_DEFAULT_UDP_PORT;
    s_config.web_port = CONFIG_DEFAULT_WEB_PORT;
    s_config.dual_mode = CONFIG_DEFAULT_DUAL_MODE;

    // Switcher Primary
    s_switchers[SWITCHER_INDEX_PRIMARY].type = CONFIG_DEFAULT_SW0_TYPE;
    s_switchers[SWITCHER_INDEX_PRIMARY].interface = CONFIG_DEFAULT_SW0_INTERFACE;
    strncpy(s_switchers[SWITCHER_INDEX_PRIMARY].ip, CONFIG_DEFAULT_SW0_IP, sizeof(s_switchers[SWITCHER_INDEX_PRIMARY].ip) - 1);
    s_switchers[SWITCHER_INDEX_PRIMARY].port = CONFIG_DEFAULT_SW0_PORT;
    strncpy(s_switchers[SWITCHER_INDEX_PRIMARY].password, CONFIG_DEFAULT_SW0_PASSWORD, sizeof(s_switchers[SWITCHER_INDEX_PRIMARY].password) - 1);
    s_switchers[SWITCHER_INDEX_PRIMARY].camera_offset = CONFIG_DEFAULT_SW0_CAMERA_OFFSET;
    s_switchers[SWITCHER_INDEX_PRIMARY].camera_limit = CONFIG_DEFAULT_SW0_CAMERA_LIMIT;

    // Switcher Secondary
    s_switchers[SWITCHER_INDEX_SECONDARY].type = CONFIG_DEFAULT_SW1_TYPE;
    s_switchers[SWITCHER_INDEX_SECONDARY].interface = CONFIG_DEFAULT_SW1_INTERFACE;
    strncpy(s_switchers[SWITCHER_INDEX_SECONDARY].ip, CONFIG_DEFAULT_SW1_IP, sizeof(s_switchers[SWITCHER_INDEX_SECONDARY].ip) - 1);
    s_switchers[SWITCHER_INDEX_SECONDARY].port = CONFIG_DEFAULT_SW1_PORT;
    strncpy(s_switchers[SWITCHER_INDEX_SECONDARY].password, CONFIG_DEFAULT_SW1_PASSWORD, sizeof(s_switchers[SWITCHER_INDEX_SECONDARY].password) - 1);
    s_switchers[SWITCHER_INDEX_SECONDARY].camera_offset = CONFIG_DEFAULT_SW1_CAMERA_OFFSET;
    s_switchers[SWITCHER_INDEX_SECONDARY].camera_limit = CONFIG_DEFAULT_SW1_CAMERA_LIMIT;
#endif

#ifdef DEVICE_MODE_RX
    // RX 전용 시스템 설정
    s_config.led_brightness = CONFIG_DEFAULT_LED_BRIGHTNESS;
    s_config.camera_id = CONFIG_DEFAULT_CAMERA_ID;
    s_config.max_camera_num = CONFIG_DEFAULT_MAX_CAMERA_NUM;
#endif
}

#ifdef DEVICE_MODE_TX
esp_err_t ConfigCore::loadWiFiSTA()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        LOG_1(TAG, "WiFi STA 설정 없음, 기본값 사용");
        return ESP_ERR_NVS_NOT_FOUND;
    }

    size_t len;

    // SSID
    len = sizeof(s_config.wifi_sta.ssid);
    err = nvs_get_str(handle, "sta_ssid", s_config.wifi_sta.ssid, &len);
    if (err != ESP_OK) {
        strncpy(s_config.wifi_sta.ssid, CONFIG_DEFAULT_WIFI_STA_SSID, sizeof(s_config.wifi_sta.ssid) - 1);
    }

    // Password
    len = sizeof(s_config.wifi_sta.password);
    err = nvs_get_str(handle, "sta_password", s_config.wifi_sta.password, &len);
    if (err != ESP_OK) {
        strncpy(s_config.wifi_sta.password, CONFIG_DEFAULT_WIFI_STA_PASS, sizeof(s_config.wifi_sta.password) - 1);
    }

    nvs_close(handle);
    return ESP_OK;
}
#endif  // DEVICE_MODE_TX

#ifdef DEVICE_MODE_TX
esp_err_t ConfigCore::loadWiFiAP()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        LOG_1(TAG, "WiFi AP 설정 없음, 기본값 사용");
        return ESP_ERR_NVS_NOT_FOUND;
    }

    size_t len;

    // SSID
    len = sizeof(s_config.wifi_ap.ssid);
    err = nvs_get_str(handle, "ap_ssid", s_config.wifi_ap.ssid, &len);
    if (err != ESP_OK) {
        strncpy(s_config.wifi_ap.ssid, CONFIG_DEFAULT_WIFI_AP_SSID, sizeof(s_config.wifi_ap.ssid) - 1);
    }

    // Password
    len = sizeof(s_config.wifi_ap.password);
    err = nvs_get_str(handle, "ap_password", s_config.wifi_ap.password, &len);
    if (err != ESP_OK) {
        strncpy(s_config.wifi_ap.password, CONFIG_DEFAULT_WIFI_AP_PASS, sizeof(s_config.wifi_ap.password) - 1);
    }

    nvs_close(handle);
    return ESP_OK;
}
#endif  // DEVICE_MODE_TX

#ifdef DEVICE_MODE_TX
esp_err_t ConfigCore::loadEthernet()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_ETH, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        LOG_1(TAG, "Ethernet 설정 없음, 기본값 사용");
        return ESP_ERR_NVS_NOT_FOUND;
    }

    size_t len;
    uint8_t dhcp_val = 1;

    // DHCP
    err = nvs_get_u8(handle, "dhcp_enabled", &dhcp_val);
    s_config.eth.dhcp_enabled = (dhcp_val != 0);

    // Static IP
    len = sizeof(s_config.eth.static_ip);
    err = nvs_get_str(handle, "static_ip", s_config.eth.static_ip, &len);
    if (err != ESP_OK) {
        strncpy(s_config.eth.static_ip, CONFIG_DEFAULT_ETH_STATIC_IP, sizeof(s_config.eth.static_ip) - 1);
    }

    // Netmask
    len = sizeof(s_config.eth.static_netmask);
    err = nvs_get_str(handle, "static_netmask", s_config.eth.static_netmask, &len);
    if (err != ESP_OK) {
        strncpy(s_config.eth.static_netmask, CONFIG_DEFAULT_ETH_NETMASK, sizeof(s_config.eth.static_netmask) - 1);
    }

    // Gateway
    len = sizeof(s_config.eth.static_gateway);
    err = nvs_get_str(handle, "static_gateway", s_config.eth.static_gateway, &len);
    if (err != ESP_OK) {
        strncpy(s_config.eth.static_gateway, CONFIG_DEFAULT_ETH_GATEWAY, sizeof(s_config.eth.static_gateway) - 1);
    }

    nvs_close(handle);
    return ESP_OK;
}
#endif  // DEVICE_MODE_TX

esp_err_t ConfigCore::loadLoRa()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_LORA, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        LOG_1(TAG, "LoRa 설정 없음, 기본값 사용");
        return ESP_ERR_NVS_NOT_FOUND;
    }

    // 주파수 읽기 (uint32_t로 저장된 값을 float로 변환)
    uint32_t freq_u32 = 0;
    err = nvs_get_u32(handle, "frequency", &freq_u32);
    if (err == ESP_OK) {
        memcpy(&s_config.lora.frequency, &freq_u32, sizeof(float));
        if (s_config.lora.frequency < 410.0f || s_config.lora.frequency > 960.0f) {
            LOG_0(TAG, "LoRa 주파수가 범위를 벗어남, 기본값 사용: %.1f MHz", s_config.lora.frequency);
            s_config.lora.frequency = CONFIG_DEFAULT_LORA_FREQUENCY;
        }
    }

    // Sync Word 읽기
    err = nvs_get_u8(handle, "sync_word", &s_config.lora.sync_word);
    if (err != ESP_OK) {
        s_config.lora.sync_word = CONFIG_DEFAULT_LORA_SYNC_WORD;
    }

    nvs_close(handle);
    LOG_1(TAG, "LoRa 설정 로드 완료: %.1f MHz, Sync Word: 0x%02X",
           s_config.lora.frequency, s_config.lora.sync_word);
    return ESP_OK;
}

esp_err_t ConfigCore::loadSystem()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_SYSTEM, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        LOG_1(TAG, "시스템 설정 없음, 기본값 사용");
        return ESP_ERR_NVS_NOT_FOUND;
    }

    size_t len;

    // Device Name (공통)
    len = sizeof(s_config.system.device_name);
    err = nvs_get_str(handle, "device_name", s_config.system.device_name, &len);
    if (err != ESP_OK) {
        strncpy(s_config.system.device_name, CONFIG_DEFAULT_DEVICE_NAME, sizeof(s_config.system.device_name) - 1);
    }

#ifdef DEVICE_MODE_TX
    // UDP Port (TX 전용)
    err = nvs_get_u16(handle, "udp_port", &s_config.udp_port);
    if (err != ESP_OK) {
        s_config.udp_port = CONFIG_DEFAULT_UDP_PORT;
    }

    // Web Port (TX 전용)
    err = nvs_get_u16(handle, "web_port", &s_config.web_port);
    if (err != ESP_OK) {
        s_config.web_port = CONFIG_DEFAULT_WEB_PORT;
    }

    // Dual Mode (TX 전용)
    uint8_t dual_mode_val = 0;
    err = nvs_get_u8(handle, "dual_mode", &dual_mode_val);
    if (err != ESP_OK) {
        s_config.dual_mode = CONFIG_DEFAULT_DUAL_MODE;
    } else {
        s_config.dual_mode = (dual_mode_val != 0);
    }
#endif

#ifdef DEVICE_MODE_RX
    // LED Brightness (RX 전용)
    err = nvs_get_u8(handle, "led_brightness", &s_config.led_brightness);
    if (err != ESP_OK) {
        s_config.led_brightness = CONFIG_DEFAULT_LED_BRIGHTNESS;
    }

    // Camera ID (RX 전용)
    err = nvs_get_u8(handle, "camera_id", &s_config.camera_id);
    if (err != ESP_OK) {
        s_config.camera_id = CONFIG_DEFAULT_CAMERA_ID;
    }

    // Max Camera Num (RX 전용)
    err = nvs_get_u8(handle, "max_camera_num", &s_config.max_camera_num);
    if (err != ESP_OK) {
        s_config.max_camera_num = CONFIG_DEFAULT_MAX_CAMERA_NUM;
    }
#endif

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t ConfigCore::init()
{
    if (s_initialized) {
        LOG_1(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    // 기본값 로드
    loadDefaults();

    // NVS에서 설정 로드 (없으면 기본값 유지)
#ifdef DEVICE_MODE_TX
    loadWiFiSTA();
    loadWiFiAP();
    loadEthernet();
#endif
    loadSystem();
    loadLoRa();

  #ifdef DEVICE_MODE_TX
    // 스위처 설정 로드
    for (int i = 0; i < SWITCHER_INDEX_MAX; i++) {
        loadSwitcher((switcher_index_t)i);
    }
#endif

    s_initialized = true;

    // NVS 설정 요약 출력 (부팅 로그)
    LOG_0(TAG, "========================================");
    LOG_0(TAG, "NVS 설정 로드 완료 (%s 모드)",
#ifdef DEVICE_MODE_TX
        "TX"
#else
        "RX"
#endif
    );
    LOG_0(TAG, "========================================");
    LOG_0(TAG, "장치명:     %s", s_config.system.device_name);
    LOG_0(TAG, "LoRa 주파수: %.1f MHz", s_config.lora.frequency);
    LOG_0(TAG, "LoRa Sync:  0x%02X", s_config.lora.sync_word);

#ifdef DEVICE_MODE_TX
    // TX 전용 설정 출력
    LOG_0(TAG, "UDP 포트:   %d", s_config.udp_port);
    LOG_0(TAG, "웹 포트:    %d", s_config.web_port);
    LOG_0(TAG, "듀얼 모드:   %s", s_config.dual_mode ? "켜짐" : "꺼짐");
    LOG_0(TAG, "WiFi AP:    %s", s_config.wifi_ap.ssid);
    LOG_0(TAG, "WiFi STA:   %s", s_config.wifi_sta.ssid[0] ? s_config.wifi_sta.ssid : "(없음)");
    LOG_0(TAG, "Ethernet:   %s", s_config.eth.dhcp_enabled ? "DHCP" : "Static");
    LOG_0(TAG, "");

    // 스위처 설정 출력
    for (int i = 0; i < SWITCHER_INDEX_MAX; i++) {
        const char* sw_name = (i == 0) ? "PRIMARY" : "SECONDARY";
        const char* type_str = (s_switchers[i].type == SWITCHER_TYPE_ATEM) ? "ATEM" :
                               (s_switchers[i].type == SWITCHER_TYPE_OBS) ? "OBS" : "vMix";
        LOG_0(TAG, "스위처 %s", sw_name);
        LOG_0(TAG, "- 타입:     %s", type_str);
        LOG_0(TAG, "- IP:       %s:%d", s_switchers[i].ip, s_switchers[i].port);
        LOG_0(TAG, "- Offset:   %d", s_switchers[i].camera_offset);
        LOG_0(TAG, "- Limit:    %d", s_switchers[i].camera_limit);
        if (i < SWITCHER_INDEX_MAX - 1) LOG_0(TAG, "");
    }
#endif

#ifdef DEVICE_MODE_RX
    // RX 전용 설정 출력
    LOG_0(TAG, "LED 밝기:   %d", s_config.led_brightness);
    LOG_0(TAG, "카메라 ID:  %d", s_config.camera_id);
#endif

    LOG_0(TAG, "========================================");
    LOG_0(TAG, "");

    return ESP_OK;
}

const Config& ConfigCore::getAll()
{
    return s_config;
}

#ifdef DEVICE_MODE_TX
const ConfigWiFiSTA& ConfigCore::getWiFiSTA()
{
    return s_config.wifi_sta;
}

const ConfigWiFiAP& ConfigCore::getWiFiAP()
{
    return s_config.wifi_ap;
}

const ConfigEthernet& ConfigCore::getEthernet()
{
    return s_config.eth;
}
#endif

const ConfigSystemCommon& ConfigCore::getSystem()
{
    return s_config.system;
}

#ifdef DEVICE_MODE_TX
esp_err_t ConfigCore::setWiFiSTA(const ConfigWiFiSTA& config)
{
    if (!s_initialized) {
        return ESP_FAIL;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG, "NVS 열기 실패: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // SSID 저장
    err = nvs_set_str(handle, "sta_ssid", config.ssid);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Password 저장
    err = nvs_set_str(handle, "sta_password", config.password);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Commit
    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        // 메모리 캐시 업데이트
        memcpy(&s_config.wifi_sta, &config, sizeof(ConfigWiFiSTA));
        LOG_0(TAG, "WiFi STA 설정 저장: %s", config.ssid);
    }

    return err;
}

esp_err_t ConfigCore::setWiFiAP(const ConfigWiFiAP& config)
{
    if (!s_initialized) {
        return ESP_FAIL;
    }

    // 비밀번호 길이 검증 (최소 8자)
    if (strlen(config.password) < 8) {
        LOG_0(TAG, "WiFi AP 비밀번호는 최소 8자 이상이어야 합니다");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG, "NVS 열기 실패: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // SSID 저장
    err = nvs_set_str(handle, "ap_ssid", config.ssid);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Password 저장
    err = nvs_set_str(handle, "ap_password", config.password);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Commit
    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        // 메모리 캐시 업데이트
        memcpy(&s_config.wifi_ap, &config, sizeof(ConfigWiFiAP));
        LOG_0(TAG, "WiFi AP 설정 저장: %s", config.ssid);
    }

    return err;
}

esp_err_t ConfigCore::setEthernet(const ConfigEthernet& config)
{
    if (!s_initialized) {
        return ESP_FAIL;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_ETH, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG, "NVS 열기 실패: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // DHCP 저장
    uint8_t dhcp_val = config.dhcp_enabled ? 1 : 0;
    err = nvs_set_u8(handle, "dhcp_enabled", dhcp_val);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Static IP 저장
    err = nvs_set_str(handle, "static_ip", config.static_ip);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Netmask 저장
    err = nvs_set_str(handle, "static_netmask", config.static_netmask);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Gateway 저장
    err = nvs_set_str(handle, "static_gateway", config.static_gateway);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Commit
    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        // 메모리 캐시 업데이트
        memcpy(&s_config.eth, &config, sizeof(ConfigEthernet));
        LOG_0(TAG, "Ethernet 설정 저장: %s", config.dhcp_enabled ? "DHCP" : "Static");
    }

    return err;
}
#endif

esp_err_t ConfigCore::factoryReset()
{
    LOG_0(TAG, "공장 초기화 시작...");

    // NVS 삭제
    esp_err_t err = nvs_flash_erase_partition("nvs");
    if (err != ESP_OK) {
        LOG_0(TAG, "NVS 삭제 실패: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // NVS 재초기화
    err = nvs_flash_init();
    if (err != ESP_OK) {
        LOG_0(TAG, "NVS 재초기화 실패: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // 기본값으로 재설정
    loadDefaults();

    LOG_0(TAG, "공장 초기화 완료");
    return ESP_OK;
}

// ============================================================================
// 스위처 설정 (TX 전용)
// ============================================================================

#ifdef DEVICE_MODE_TX
esp_err_t ConfigCore::loadSwitcher(switcher_index_t index)
{
    if (index >= SWITCHER_INDEX_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_SWITCHER, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG, "스위처 %d 설정 없음, 기본값 사용", index);
        return ESP_ERR_NVS_NOT_FOUND;
    }

    // NVS 키 이름
    char key[32];
    size_t len;

    // Type
    snprintf(key, sizeof(key), "sw%d_type", index);
    uint8_t type_val = 0;
    err = nvs_get_u8(handle, key, &type_val);
    if (err == ESP_OK) {
        s_switchers[index].type = (switcher_type_t)type_val;
    }

    // Interface
    snprintf(key, sizeof(key), "sw%d_if", index);
    uint8_t if_val = 0;
    err = nvs_get_u8(handle, key, &if_val);
    if (err == ESP_OK) {
        s_switchers[index].interface = (switcher_interface_t)if_val;
    }

    // IP
    snprintf(key, sizeof(key), "sw%d_ip", index);
    len = sizeof(s_switchers[index].ip);
    nvs_get_str(handle, key, s_switchers[index].ip, &len);

    // Port
    snprintf(key, sizeof(key), "sw%d_port", index);
    uint16_t port_val = 0;
    err = nvs_get_u16(handle, key, &port_val);
    if (err == ESP_OK) {
        s_switchers[index].port = port_val;
    }

    // Password
    snprintf(key, sizeof(key), "sw%d_password", index);
    len = sizeof(s_switchers[index].password);
    nvs_get_str(handle, key, s_switchers[index].password, &len);

    // Camera Offset
    snprintf(key, sizeof(key), "sw%d_offset", index);
    uint8_t offset_val = 0;
    err = nvs_get_u8(handle, key, &offset_val);
    if (err == ESP_OK) {
        s_switchers[index].camera_offset = offset_val;
    }

    // Camera Limit
    snprintf(key, sizeof(key), "sw%d_limit", index);
    uint8_t limit_val = 0;
    err = nvs_get_u8(handle, key, &limit_val);
    if (err == ESP_OK) {
        s_switchers[index].camera_limit = limit_val;
    }

    nvs_close(handle);
    return ESP_OK;
}
#endif  // DEVICE_MODE_TX

#ifdef DEVICE_MODE_TX
ConfigSwitcher ConfigCore::getSwitcher(switcher_index_t index)
{
    // 기본값
    static const ConfigSwitcher defaults[SWITCHER_INDEX_MAX] = {
        // Primary
        {
            .type = SWITCHER_TYPE_ATEM,
            .interface = SWITCHER_INTERFACE_WIFI_STA,
            .ip = "192.168.0.240",
            .port = 9910,
            .password = "",
            .camera_offset = 0,
            .camera_limit = 0
        },
        // Secondary
        {
            .type = SWITCHER_TYPE_ATEM,
            .interface = SWITCHER_INTERFACE_ETHERNET,
            .ip = "192.168.0.241",
            .port = 0,
            .password = "",
            .camera_offset = 4,
            .camera_limit = 0
        }
    };

    if (index >= SWITCHER_INDEX_MAX) {
        return defaults[0];
    }

    // 초기화되지 않았으면 기본값 반환
    if (!s_initialized) {
        return defaults[index];
    }

    return s_switchers[index];
}

esp_err_t ConfigCore::setSwitcher(switcher_index_t index, const ConfigSwitcher& config)
{
    if (!s_initialized) {
        return ESP_FAIL;
    }

    if (index >= SWITCHER_INDEX_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_SWITCHER, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG, "NVS 열기 실패: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // NVS 키 이름
    char key[32];

    // Type 저장
    snprintf(key, sizeof(key), "sw%d_type", index);
    err = nvs_set_u8(handle, key, (uint8_t)config.type);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Interface 저장
    snprintf(key, sizeof(key), "sw%d_if", index);
    err = nvs_set_u8(handle, key, (uint8_t)config.interface);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // IP 저장
    snprintf(key, sizeof(key), "sw%d_ip", index);
    err = nvs_set_str(handle, key, config.ip);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Port 저장
    snprintf(key, sizeof(key), "sw%d_port", index);
    err = nvs_set_u16(handle, key, config.port);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Password 저장
    snprintf(key, sizeof(key), "sw%d_password", index);
    err = nvs_set_str(handle, key, config.password);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Camera Offset 저장
    snprintf(key, sizeof(key), "sw%d_offset", index);
    err = nvs_set_u8(handle, key, config.camera_offset);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Camera Limit 저장
    snprintf(key, sizeof(key), "sw%d_limit", index);
    err = nvs_set_u8(handle, key, config.camera_limit);
    if (err != ESP_OK) {
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Commit
    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        // 메모리 캐시 업데이트
        ConfigSwitcher old_config = s_switchers[index];
        memcpy(&s_switchers[index], &config, sizeof(ConfigSwitcher));
        const char* sw_name = (index == SWITCHER_INDEX_PRIMARY) ? "Primary" : "Secondary";
        LOG_1(TAG_CONFIG, "스위처 %s 설정 저장: %s (type:%d, offset:%d, limit:%d)",
                 sw_name, config.ip, config.type, config.camera_offset, config.camera_limit);

        // 설정이 변경된 경우 DisplayManager에 알림
        // ConfigCore에서는 저장만 하고 디스플레이 업데이트는 SwitcherManager에서 처리
        if (old_config.type != config.type ||
            strcmp(old_config.ip, config.ip) != 0 ||
            old_config.port != config.port) {
            // 변경사항이 있지만 즉시 알리지 않음
        }
    } else {
        LOG_0(TAG_CONFIG, "스위처 %d NVS commit 실패: %d", index, err);
    }

    return err;
}
#endif

#ifdef DEVICE_MODE_TX
bool ConfigCore::getDualMode()
{
    return s_config.dual_mode;
}

esp_err_t ConfigCore::setDualMode(bool dual_mode)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_SYSTEM, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG, "듀얼 모드 저장 실패: NVS 열기 실패 (%s)", esp_err_to_name(err));
        return err;
    }

    // NVS에 저장
    uint8_t value = dual_mode ? 1 : 0;
    err = nvs_set_u8(handle, "dual_mode", value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK) {
        // 메모리 캐시 업데이트
        bool old_dual_mode = s_config.dual_mode;
        s_config.dual_mode = dual_mode;
        LOG_0(TAG, "듀얼 모드 설정 저장: %s", dual_mode ? "듀얼" : "싱글");

        // 듀얼 모드 변경사항이 있지만 SwitcherManager::restartAll()에서 처리
        if (old_dual_mode != dual_mode) {
            // 변경사항이 있지만 즉시 알리지 않음
        }
    } else {
        LOG_0(TAG, "듀얼 모드 NVS commit 실패: %s", esp_err_to_name(err));
    }

    return err;
}
#else
bool ConfigCore::getDualMode()
{
    // RX 모드에서는 항상 false 반환
    return false;
}
#endif

#ifdef DEVICE_MODE_RX
uint8_t ConfigCore::getCameraId()
{
    return s_config.camera_id;
}

esp_err_t ConfigCore::setCameraId(uint8_t camera_id)
{
    if (camera_id < 1 || camera_id > 20) {
        LOG_0(TAG, "카메라 ID 저장 실패: 유효하지 않은 값 (%d)", camera_id);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_SYSTEM, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG, "카메라 ID 저장 실패: NVS 열기 실패 (%s)", esp_err_to_name(err));
        return err;
    }

    // NVS에 저장
    err = nvs_set_u8(handle, "camera_id", camera_id);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK) {
        // 메모리 캐시 업데이트
        s_config.camera_id = camera_id;
        LOG_0(TAG, "카메라 ID 설정 저장: %d", camera_id);
    } else {
        LOG_0(TAG, "카메라 ID NVS commit 실패: %s", esp_err_to_name(err));
    }

    return err;
}

uint8_t ConfigCore::getMaxCameraNum()
{
    return s_config.max_camera_num;
}

esp_err_t ConfigCore::setMaxCameraNum(uint8_t max_camera_num)
{
    if (max_camera_num < 1 || max_camera_num > 20) {
        LOG_0(TAG, "최대 카메라 수 저장 실패: 유효하지 않은 값 (%d)", max_camera_num);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_SYSTEM, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG, "최대 카메라 수 저장 실패: NVS 열기 실패 (%s)", esp_err_to_name(err));
        return err;
    }

    // NVS에 저장
    err = nvs_set_u8(handle, "max_camera_num", max_camera_num);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK) {
        s_config.max_camera_num = max_camera_num;
        LOG_0(TAG, "최대 카메라 수 설정 저장: %d", max_camera_num);
    } else {
        LOG_0(TAG, "최대 카메라 수 NVS commit 실패: %s", esp_err_to_name(err));
    }

    return err;
}
#endif

/* ============================================================================
 * LoRa 관련 (공통)
 * ============================================================================
 */

ConfigLoRa ConfigCore::getLoRa()
{
    return s_config.lora;
}

esp_err_t ConfigCore::setLoRa(const ConfigLoRa& config)
{
    // 입력값 검증
    if (config.frequency < 410.0f || config.frequency > 960.0f) {
        LOG_0(TAG, "LoRa 주파수 저장 실패: 유효하지 않은 값 (%.1f MHz)", config.frequency);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_LORA, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG, "LoRa 설정 저장 실패: NVS 열기 실패 (%s)", esp_err_to_name(err));
        return err;
    }

    // 주파수 저장 (float을 uint32_t로 변환)
    uint32_t freq_u32;
    memcpy(&freq_u32, &config.frequency, sizeof(float));
    err = nvs_set_u32(handle, "frequency", freq_u32);
    if (err != ESP_OK) {
        LOG_0(TAG, "주파수 저장 실패: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Sync Word 저장
    err = nvs_set_u8(handle, "sync_word", config.sync_word);
    if (err != ESP_OK) {
        LOG_0(TAG, "Sync Word 저장 실패: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // NVS commit
    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        // 메모리 캐시 업데이트
        s_config.lora = config;
        LOG_0(TAG, "LoRa 설정 저장 완료: %.1f MHz, Sync Word: 0x%02X",
               config.frequency, config.sync_word);
    } else {
        LOG_0(TAG, "LoRa 설정 NVS commit 실패: %s", esp_err_to_name(err));
    }

    return err;
}

/* ============================================================================
 * C 인터페이스 구현 (button_actions.c에서 사용)
 * ============================================================================
 */
#ifdef DEVICE_MODE_RX

extern "C" {
    uint8_t config_get_camera_id(void) {
        return ConfigCore::getCameraId();
    }

    esp_err_t config_set_camera_id(uint8_t camera_id) {
        return ConfigCore::setCameraId(camera_id);
    }

    uint8_t config_get_max_camera_num(void) {
        return ConfigCore::getMaxCameraNum();
    }

    esp_err_t config_set_max_camera_num(uint8_t max_camera_num) {
        return ConfigCore::setMaxCameraNum(max_camera_num);
    }
}

#endif
