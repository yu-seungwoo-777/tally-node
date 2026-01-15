/**
 * @file web_server_json.cpp
 * @brief Web Server JSON 생성 함수 구현
 */

#include "web_server_json.h"
#include "web_server_cache.h"
#include "app_types.h"
#include <cmath>
#include <cstring>

static const char* TAG = "02_WS_JSON";
// 사용하지 않는 변수 경고 방지
static inline void suppress_unused_tag_warning(void) { (void)TAG; }

extern "C" {

// ============================================================================
// 헬퍼 함수 구현
// ============================================================================

uint8_t web_server_json_get_channel_state(const uint8_t* data, uint8_t channel)
{
    if (!data || channel < 1 || channel > 20) {
        return 0;
    }
    uint8_t byte_idx = (channel - 1) / 4;
    uint8_t bit_idx = ((channel - 1) % 4) * 2;
    return (data[byte_idx] >> bit_idx) & 0x03;
}

void web_server_json_packed_to_hex(const uint8_t* data, uint8_t size, char* out, size_t out_size)
{
    if (!data || !out || out_size < (size * 2 + 1)) {
        if (out) out[0] = '\0';
        return;
    }
    for (uint8_t i = 0; i < size; i++) {
        snprintf(out + (i * 2), 3, "%02X", data[i]);
    }
}

// ============================================================================
// 네트워크 JSON 생성 함수
// ============================================================================

cJSON* web_server_json_create_network_ap(void)
{
    cJSON* ap = cJSON_CreateObject();
    if (!ap) {
        return nullptr;
    }

    const web_server_data_t* cache = web_server_cache_get();

    if (web_server_cache_is_config_valid()) {
        cJSON_AddBoolToObject(ap, "enabled", cache->config.wifi_ap_enabled);
        cJSON_AddStringToObject(ap, "ssid", cache->config.wifi_ap_ssid);
        cJSON_AddStringToObject(ap, "password", cache->config.wifi_ap_password);
        cJSON_AddNumberToObject(ap, "channel", cache->config.wifi_ap_channel);
        if (web_server_cache_is_network_valid()) {
            const char* ip = (cache->config.wifi_ap_enabled && cache->network.ap_ip[0] != '\0') ?
                           cache->network.ap_ip : "--";
            cJSON_AddStringToObject(ap, "ip", ip);
        } else {
            cJSON_AddStringToObject(ap, "ip", "--");
        }
    } else {
        cJSON_AddBoolToObject(ap, "enabled", false);
        cJSON_AddStringToObject(ap, "ssid", "--");
        cJSON_AddStringToObject(ap, "password", "");
        cJSON_AddNumberToObject(ap, "channel", 1);
        cJSON_AddStringToObject(ap, "ip", "--");
    }

    return ap;
}

cJSON* web_server_json_create_network_wifi(void)
{
    cJSON* wifi = cJSON_CreateObject();
    if (!wifi) {
        return nullptr;
    }

    const web_server_data_t* cache = web_server_cache_get();

    if (web_server_cache_is_config_valid()) {
        cJSON_AddBoolToObject(wifi, "enabled", cache->config.wifi_sta_enabled);
        cJSON_AddStringToObject(wifi, "ssid", cache->config.wifi_sta_ssid);
        cJSON_AddStringToObject(wifi, "password", cache->config.wifi_sta_password);
    } else {
        cJSON_AddBoolToObject(wifi, "enabled", false);
        cJSON_AddStringToObject(wifi, "ssid", "--");
        cJSON_AddStringToObject(wifi, "password", "");
    }

    if (web_server_cache_is_network_valid()) {
        cJSON_AddBoolToObject(wifi, "connected", cache->network.sta_connected);
        cJSON_AddStringToObject(wifi, "ip", cache->network.sta_connected ?
                               cache->network.sta_ip : "--");
    } else {
        cJSON_AddBoolToObject(wifi, "connected", false);
        cJSON_AddStringToObject(wifi, "ip", "--");
    }

    return wifi;
}

cJSON* web_server_json_create_network_ethernet(void)
{
    cJSON* ethernet = cJSON_CreateObject();
    if (!ethernet) {
        return nullptr;
    }

    const web_server_data_t* cache = web_server_cache_get();

    if (web_server_cache_is_config_valid()) {
        cJSON_AddBoolToObject(ethernet, "enabled", cache->config.eth_enabled);
        cJSON_AddBoolToObject(ethernet, "dhcp", cache->config.eth_dhcp_enabled);
        cJSON_AddStringToObject(ethernet, "staticIp", cache->config.eth_static_ip);
        cJSON_AddStringToObject(ethernet, "netmask", cache->config.eth_static_netmask);
        cJSON_AddStringToObject(ethernet, "gateway", cache->config.eth_static_gateway);
    } else {
        cJSON_AddBoolToObject(ethernet, "enabled", false);
        cJSON_AddBoolToObject(ethernet, "dhcp", true);
        cJSON_AddStringToObject(ethernet, "staticIp", "");
        cJSON_AddStringToObject(ethernet, "netmask", "");
        cJSON_AddStringToObject(ethernet, "gateway", "");
    }

    if (web_server_cache_is_network_valid()) {
        cJSON_AddBoolToObject(ethernet, "connected", cache->network.eth_connected);
        cJSON_AddBoolToObject(ethernet, "detected", cache->network.eth_detected);
        cJSON_AddStringToObject(ethernet, "ip", cache->network.eth_connected ?
                               cache->network.eth_ip : "--");
    } else {
        cJSON_AddBoolToObject(ethernet, "connected", false);
        cJSON_AddBoolToObject(ethernet, "detected", false);
        cJSON_AddStringToObject(ethernet, "ip", "--");
    }

    return ethernet;
}

// ============================================================================
// Tally JSON 생성 함수
// ============================================================================

cJSON* web_server_json_create_tally(const uint8_t* tally_data, uint8_t channel_count)
{
    cJSON* tally = cJSON_CreateObject();
    if (!tally) {
        return nullptr;
    }

    cJSON* pgm = cJSON_CreateArray();
    cJSON* pvw = cJSON_CreateArray();

    for (uint8_t i = 0; i < channel_count && i < 20; i++) {
        uint8_t state = web_server_json_get_channel_state(tally_data, i + 1);
        if (state == 1 || state == 3) {
            cJSON_AddItemToArray(pgm, cJSON_CreateNumber(i + 1));
        }
        if (state == 2 || state == 3) {
            cJSON_AddItemToArray(pvw, cJSON_CreateNumber(i + 1));
        }
    }

    cJSON_AddItemToObject(tally, "pgm", pgm);
    cJSON_AddItemToObject(tally, "pvw", pvw);

    char hex[17] = {0};
    uint8_t bytes = (channel_count + 3) / 4;
    web_server_json_packed_to_hex(tally_data, bytes, hex, sizeof(hex));
    cJSON_AddStringToObject(tally, "raw", hex);
    cJSON_AddNumberToObject(tally, "channels", channel_count);

    return tally;
}

cJSON* web_server_json_create_empty_tally(void)
{
    cJSON* tally = cJSON_CreateObject();
    if (!tally) {
        return nullptr;
    }

    cJSON_AddItemToObject(tally, "pgm", cJSON_CreateArray());
    cJSON_AddItemToObject(tally, "pvw", cJSON_CreateArray());
    cJSON_AddStringToObject(tally, "raw", "");
    cJSON_AddNumberToObject(tally, "channels", 0);

    return tally;
}

// ============================================================================
// 스위처 JSON 생성 함수
// ============================================================================

cJSON* web_server_json_create_switcher_primary(void)
{
    cJSON* primary = cJSON_CreateObject();
    if (!primary) {
        return nullptr;
    }

    const web_server_data_t* cache = web_server_cache_get();

    if (web_server_cache_is_switcher_valid()) {
        cJSON_AddBoolToObject(primary, "connected", cache->switcher.s1_connected);
        cJSON_AddStringToObject(primary, "type", cache->switcher.s1_type);
        cJSON_AddStringToObject(primary, "ip", cache->switcher.s1_ip);
        cJSON_AddNumberToObject(primary, "port", cache->switcher.s1_port);

        // cameraLimit는 switcher 이벤트에서 가져옴
        cJSON_AddNumberToObject(primary, "cameraLimit", cache->switcher.s1_camera_limit);

        // interface는 config 이벤트에서 가져옴
        if (web_server_cache_is_config_valid()) {
            cJSON_AddNumberToObject(primary, "interface", cache->config.primary_interface);
        } else {
            cJSON_AddNumberToObject(primary, "interface", 2);
        }

        cJSON* tally = web_server_json_create_tally(cache->switcher.s1_tally_data,
                                                     cache->switcher.s1_channel_count);
        if (tally) {
            cJSON_AddItemToObject(primary, "tally", tally);
        }
    } else {
        cJSON_AddBoolToObject(primary, "connected", false);
        cJSON_AddStringToObject(primary, "type", "--");
        cJSON_AddStringToObject(primary, "ip", "--");
        cJSON_AddNumberToObject(primary, "port", 0);
        // 기본값: switcher 이벤트가 없으면 0 사용
        cJSON_AddNumberToObject(primary, "cameraLimit", cache->switcher.s1_camera_limit);
        cJSON_AddNumberToObject(primary, "interface", 2);

        cJSON* tally = web_server_json_create_empty_tally();
        if (tally) {
            cJSON_AddItemToObject(primary, "tally", tally);
        }
    }

    return primary;
}

cJSON* web_server_json_create_switcher_secondary(void)
{
    cJSON* secondary = cJSON_CreateObject();
    if (!secondary) {
        return nullptr;
    }

    const web_server_data_t* cache = web_server_cache_get();

    if (web_server_cache_is_switcher_valid()) {
        cJSON_AddBoolToObject(secondary, "connected", cache->switcher.s2_connected);
        cJSON_AddStringToObject(secondary, "type", cache->switcher.s2_type);
        cJSON_AddStringToObject(secondary, "ip", cache->switcher.s2_ip);
        cJSON_AddNumberToObject(secondary, "port", cache->switcher.s2_port);

        // cameraLimit는 switcher 이벤트에서 가져옴
        cJSON_AddNumberToObject(secondary, "cameraLimit", cache->switcher.s2_camera_limit);

        // interface는 config 이벤트에서 가져옴
        if (web_server_cache_is_config_valid()) {
            cJSON_AddNumberToObject(secondary, "interface", cache->config.secondary_interface);
        } else {
            cJSON_AddNumberToObject(secondary, "interface", 1);
        }

        cJSON* tally = web_server_json_create_tally(cache->switcher.s2_tally_data,
                                                     cache->switcher.s2_channel_count);
        if (tally) {
            cJSON_AddItemToObject(secondary, "tally", tally);
        }
    } else {
        cJSON_AddBoolToObject(secondary, "connected", false);
        cJSON_AddStringToObject(secondary, "type", "--");
        cJSON_AddStringToObject(secondary, "ip", "--");
        cJSON_AddNumberToObject(secondary, "port", 0);
        // 기본값: switcher 이벤트가 없으면 0 사용
        cJSON_AddNumberToObject(secondary, "cameraLimit", cache->switcher.s2_camera_limit);
        cJSON_AddNumberToObject(secondary, "interface", 1);

        cJSON* tally = web_server_json_create_empty_tally();
        if (tally) {
            cJSON_AddItemToObject(secondary, "tally", tally);
        }
    }

    return secondary;
}

cJSON* web_server_json_create_switcher(void)
{
    cJSON* switcher = cJSON_CreateObject();
    if (!switcher) {
        return nullptr;
    }

    cJSON* primary = web_server_json_create_switcher_primary();
    if (primary) {
        cJSON_AddItemToObject(switcher, "primary", primary);
    }

    cJSON* secondary = web_server_json_create_switcher_secondary();
    if (secondary) {
        cJSON_AddItemToObject(switcher, "secondary", secondary);
    }

    const web_server_data_t* cache = web_server_cache_get();
    cJSON_AddBoolToObject(switcher, "dualEnabled",
                         web_server_cache_is_switcher_valid() ? cache->switcher.dual_mode : false);

    if (web_server_cache_is_config_valid()) {
        cJSON_AddNumberToObject(switcher, "secondaryOffset",
                              cache->config.secondary_offset);
    } else {
        cJSON_AddNumberToObject(switcher, "secondaryOffset", 4);
    }

    return switcher;
}

// ============================================================================
// 시스템 JSON 생성 함수
// ============================================================================

cJSON* web_server_json_create_system(void)
{
    cJSON* system = cJSON_CreateObject();
    if (!system) {
        return nullptr;
    }

    const web_server_data_t* cache = web_server_cache_get();

    // 펌웨어 버전 (빌드타임에 정의된 값)
    cJSON_AddStringToObject(system, "version", FIRMWARE_VERSION);

    if (web_server_cache_is_system_valid()) {
        cJSON_AddStringToObject(system, "deviceId", cache->system.device_id);
        cJSON_AddNumberToObject(system, "battery", cache->system.battery);
        cJSON_AddNumberToObject(system, "voltage",
                              round(cache->system.voltage * 10) / 10);
        cJSON_AddNumberToObject(system, "temperature",
                              round(cache->system.temperature * 10) / 10);
        cJSON_AddNumberToObject(system, "uptime", cache->system.uptime);
        cJSON_AddNumberToObject(system, "loraChipType",
                              cache->system.lora_chip_type);
    } else {
        cJSON_AddStringToObject(system, "deviceId", "0000");
        cJSON_AddNumberToObject(system, "battery", 0);
        cJSON_AddNumberToObject(system, "voltage", 0);
        cJSON_AddNumberToObject(system, "temperature", 0);
        cJSON_AddNumberToObject(system, "uptime", 0);
        cJSON_AddNumberToObject(system, "loraChipType", 0);
    }

    return system;
}

// ============================================================================
// RF/Broadcast JSON 생성 함수
// ============================================================================

cJSON* web_server_json_create_rf(void)
{
    cJSON* rf = cJSON_CreateObject();
    if (!rf) {
        return nullptr;
    }

    const web_server_data_t* cache = web_server_cache_get();

    if (web_server_cache_is_config_valid()) {
        cJSON_AddNumberToObject(rf, "frequency",
                              cache->config.device_rf_frequency);
        cJSON_AddNumberToObject(rf, "syncWord",
                              cache->config.device_rf_sync_word);
        cJSON_AddNumberToObject(rf, "spreadingFactor",
                              cache->config.device_rf_sf);
        cJSON_AddNumberToObject(rf, "codingRate", cache->config.device_rf_cr);
        cJSON_AddNumberToObject(rf, "bandwidth", cache->config.device_rf_bw);
        cJSON_AddNumberToObject(rf, "txPower", cache->config.device_rf_tx_power);
    } else {
        cJSON_AddNumberToObject(rf, "frequency", 868);
        cJSON_AddNumberToObject(rf, "syncWord", 0x12);
        cJSON_AddNumberToObject(rf, "spreadingFactor", 7);
        cJSON_AddNumberToObject(rf, "codingRate", 7);
        cJSON_AddNumberToObject(rf, "bandwidth", 250);
        cJSON_AddNumberToObject(rf, "txPower", 22);
    }

    return rf;
}

cJSON* web_server_json_create_broadcast(void)
{
    cJSON* broadcast = cJSON_CreateObject();
    if (!broadcast) {
        return nullptr;
    }

    cJSON* rf = web_server_json_create_rf();
    if (rf) {
        cJSON_AddItemToObject(broadcast, "rf", rf);
    }

    return broadcast;
}

// ============================================================================
// LED 색상 JSON 생성 함수
// ============================================================================

cJSON* web_server_json_create_led_colors(void)
{
    cJSON* led = cJSON_CreateObject();
    if (!led) {
        return nullptr;
    }

    const web_server_led_colors_t* colors = web_server_cache_get_led_colors();

    // Program 색상
    cJSON* program = cJSON_CreateObject();
    if (program) {
        cJSON_AddNumberToObject(program, "r", colors->program.r);
        cJSON_AddNumberToObject(program, "g", colors->program.g);
        cJSON_AddNumberToObject(program, "b", colors->program.b);
        cJSON_AddItemToObject(led, "program", program);
    }

    // Preview 색상
    cJSON* preview = cJSON_CreateObject();
    if (preview) {
        cJSON_AddNumberToObject(preview, "r", colors->preview.r);
        cJSON_AddNumberToObject(preview, "g", colors->preview.g);
        cJSON_AddNumberToObject(preview, "b", colors->preview.b);
        cJSON_AddItemToObject(led, "preview", preview);
    }

    // Off 색상
    cJSON* off = cJSON_CreateObject();
    if (off) {
        cJSON_AddNumberToObject(off, "r", colors->off.r);
        cJSON_AddNumberToObject(off, "g", colors->off.g);
        cJSON_AddNumberToObject(off, "b", colors->off.b);
        cJSON_AddItemToObject(led, "off", off);
    }

    return led;
}

// ============================================================================
// 라이센스 JSON 생성 함수
// ============================================================================

cJSON* web_server_json_create_license(void)
{
    cJSON* license = cJSON_CreateObject();
    if (!license) {
        return nullptr;
    }

    // 캐시된 라이센스 상태 사용 (이벤트 기반)
    uint8_t device_limit = 0;
    uint8_t state_val = 0;
    char key[17] = {0};

    if (web_server_cache_lock() == pdTRUE) {
        if (web_server_cache_is_license_valid()) {
            const web_server_data_t* cache = web_server_cache_get();
            device_limit = cache->license.device_limit;
            state_val = cache->license.state;
            strncpy(key, cache->license.key, 16);
            key[16] = '\0';
        }
        web_server_cache_unlock();
    }

    cJSON_AddNumberToObject(license, "deviceLimit", device_limit);
    cJSON_AddNumberToObject(license, "state", (int)state_val);

    const char* state_str = "unknown";
    switch (state_val) {
        case LICENSE_STATE_VALID:
            state_str = "valid";
            break;
        case LICENSE_STATE_INVALID:
            state_str = "invalid";
            break;
        case LICENSE_STATE_CHECKING:
            state_str = "checking";
            break;
    }
    cJSON_AddStringToObject(license, "stateStr", state_str);

    bool is_valid = (state_val == LICENSE_STATE_VALID);
    cJSON_AddBoolToObject(license, "isValid", is_valid);
    cJSON_AddStringToObject(license, "key", key);

    return license;
}

} // extern "C"
