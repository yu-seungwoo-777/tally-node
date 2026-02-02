/**
 * @file web_server_config.cpp
 * @brief Web Server 설정 파싱 함수 구현
 */

#include "web_server_config.h"
#include "event_bus.h"
#include "t_log.h"
#include "string.h"

static const char* TAG = "02_WS_Config";

// ============================================================================
// IP 주소 검증 함수
// ============================================================================

/**
 * @brief IPv4 주소 형식 검증
 * @param ip IP 주소 문자열
 * @return true 유효한 IPv4 주소, false 잘못된 주소
 */
static bool is_valid_ipv4(const char* ip)
{
    if (!ip || ip[0] == '\0') {
        return true;  // 빈 문자열 허용 (미설정)
    }

    // 길이 체크 (최소 7자: "0.0.0.0", 최대 15자: "255.255.255.255")
    size_t len = strlen(ip);
    if (len < 7 || len > 15) {
        return false;
    }

    // 옥텟 분리 및 검증
    int octet_count = 0;
    int dot_count = 0;
    int current_value = 0;
    bool has_digit = false;

    for (size_t i = 0; i <= len; i++) {
        if (i == len || ip[i] == '.') {
            // 옥텟 끝 또는 점을 만남
            if (!has_digit) {
                return false;  // 점 연속 또는 숫자 없음
            }
            if (current_value > 255) {
                return false;  // 옥텟 값 초과
            }
            octet_count++;
            current_value = 0;
            has_digit = false;

            if (i < len && ip[i] == '.') {
                dot_count++;
            }
        } else if (ip[i] >= '0' && ip[i] <= '9') {
            current_value = current_value * 10 + (ip[i] - '0');
            has_digit = true;
        } else {
            return false;  // 잘못된 문자
        }
    }

    // 4개 옥텟, 3개 점
    return (octet_count == 4 && dot_count == 3);
}

extern "C" {

// ============================================================================
// 스위처 설정 파싱 함수
// ============================================================================

void web_server_config_parse_switcher_common_fields(cJSON* root, config_save_request_t* save_req)
{
    cJSON* type = cJSON_GetObjectItem(root, "type");
    cJSON* ip = cJSON_GetObjectItem(root, "ip");
    cJSON* port = cJSON_GetObjectItem(root, "port");
    cJSON* interface = cJSON_GetObjectItem(root, "interface");
    cJSON* camera_limit = cJSON_GetObjectItem(root, "cameraLimit");
    cJSON* password = cJSON_GetObjectItem(root, "password");

    if (type && type->valuestring) {
        strncpy(save_req->switcher_type, type->valuestring,
               sizeof(save_req->switcher_type) - 1);
    }
    if (ip && cJSON_IsString(ip)) {
        strncpy(save_req->switcher_ip, ip->valuestring,
               sizeof(save_req->switcher_ip) - 1);
    }
    if (port && cJSON_IsNumber(port)) {
        save_req->switcher_port = (uint16_t)port->valueint;
    }
    if (interface && cJSON_IsNumber(interface)) {
        save_req->switcher_interface = (uint8_t)interface->valueint;
    } else {
        save_req->switcher_interface = 0; // Default: Auto
    }
    if (camera_limit && cJSON_IsNumber(camera_limit)) {
        save_req->switcher_camera_limit = (uint8_t)camera_limit->valueint;
    } else {
        save_req->switcher_camera_limit = 0; // Default: unlimited
    }
    if (password && cJSON_IsString(password)) {
        strncpy(save_req->switcher_password, password->valuestring,
               sizeof(save_req->switcher_password) - 1);
    } else {
        save_req->switcher_password[0] = '\0';
    }
}

esp_err_t web_server_config_parse_switcher_primary(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_SWITCHER_PRIMARY;
    web_server_config_parse_switcher_common_fields(root, save_req);
    return ESP_OK;
}

esp_err_t web_server_config_parse_switcher_secondary(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_SWITCHER_SECONDARY;
    web_server_config_parse_switcher_common_fields(root, save_req);
    return ESP_OK;
}

esp_err_t web_server_config_parse_switcher_dual(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_SWITCHER_DUAL;

    // 파라미터 호환성: dualEnabled/enabled, secondaryOffset/offset
    cJSON* enabled = cJSON_GetObjectItem(root, "enabled");
    cJSON* dual_enabled = cJSON_GetObjectItem(root, "dualEnabled");
    cJSON* offset = cJSON_GetObjectItem(root, "offset");
    cJSON* secondary_offset = cJSON_GetObjectItem(root, "secondaryOffset");

    // dualEnabled 또는 enabled 사용 (우선순위: dualEnabled > enabled)
    if (dual_enabled && cJSON_IsBool(dual_enabled)) {
        save_req->switcher_dual_enabled = cJSON_IsTrue(dual_enabled);
    } else if (enabled && cJSON_IsBool(enabled)) {
        save_req->switcher_dual_enabled = cJSON_IsTrue(enabled);
    }

    // secondaryOffset 또는 offset 사용 (우선순위: secondaryOffset > offset)
    if (secondary_offset && cJSON_IsNumber(secondary_offset)) {
        save_req->switcher_secondary_offset = (uint8_t)secondary_offset->valueint;
    } else if (offset && cJSON_IsNumber(offset)) {
        save_req->switcher_secondary_offset = (uint8_t)offset->valueint;
    }

    T_LOGD(TAG, "Publishing Dual Mode save event: enabled=%d, offset=%d",
             save_req->switcher_dual_enabled, save_req->switcher_secondary_offset);

    return ESP_OK;
}

// ============================================================================
// 네트워크 설정 파싱 함수
// ============================================================================

esp_err_t web_server_config_parse_network_ap(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_WIFI_AP;

    cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON* password = cJSON_GetObjectItem(root, "password");
    cJSON* channel = cJSON_GetObjectItem(root, "channel");
    cJSON* enabled = cJSON_GetObjectItem(root, "enabled");

    if (ssid && cJSON_IsString(ssid)) {
        strncpy(save_req->wifi_ap_ssid, ssid->valuestring,
               sizeof(save_req->wifi_ap_ssid) - 1);
    }
    if (password && cJSON_IsString(password)) {
        strncpy(save_req->wifi_ap_password, password->valuestring,
               sizeof(save_req->wifi_ap_password) - 1);
    } else {
        save_req->wifi_ap_password[0] = '\0';  // password 없음
    }
    if (channel && cJSON_IsNumber(channel)) {
        save_req->wifi_ap_channel = (uint8_t)channel->valueint;
    }
    if (enabled && cJSON_IsBool(enabled)) {
        save_req->wifi_ap_enabled = cJSON_IsTrue(enabled);
    }

    T_LOGD(TAG, "Publishing AP save event: ssid=%s, pass_len=%d, ch=%d, en=%d",
             save_req->wifi_ap_ssid, strlen(save_req->wifi_ap_password),
             save_req->wifi_ap_channel, save_req->wifi_ap_enabled);

    return ESP_OK;
}

esp_err_t web_server_config_parse_network_wifi(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_WIFI_STA;

    cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON* password = cJSON_GetObjectItem(root, "password");
    cJSON* enabled = cJSON_GetObjectItem(root, "enabled");

    if (ssid && cJSON_IsString(ssid)) {
        strncpy(save_req->wifi_sta_ssid, ssid->valuestring,
               sizeof(save_req->wifi_sta_ssid) - 1);
    }
    if (password && cJSON_IsString(password)) {
        strncpy(save_req->wifi_sta_password, password->valuestring,
               sizeof(save_req->wifi_sta_password) - 1);
    } else {
        save_req->wifi_sta_password[0] = '\0';  // password 없음
    }
    if (enabled && cJSON_IsBool(enabled)) {
        save_req->wifi_sta_enabled = cJSON_IsTrue(enabled);
    }

    T_LOGD(TAG, "Publishing STA save event: ssid=%s, pass_len=%d, en=%d",
             save_req->wifi_sta_ssid, strlen(save_req->wifi_sta_password),
             save_req->wifi_sta_enabled);

    return ESP_OK;
}

esp_err_t web_server_config_parse_network_ethernet(cJSON* root, config_save_request_t* save_req)
{
    save_req->type = CONFIG_SAVE_ETHERNET;

    cJSON* ip = cJSON_GetObjectItem(root, "staticIp");
    cJSON* gateway = cJSON_GetObjectItem(root, "gateway");
    cJSON* netmask = cJSON_GetObjectItem(root, "netmask");
    cJSON* dhcp = cJSON_GetObjectItem(root, "dhcp");
    cJSON* enabled = cJSON_GetObjectItem(root, "enabled");

    // DHCP가 아닌 경우에만 IP 검증
    bool use_dhcp = true;  // 기본값
    if (dhcp && cJSON_IsBool(dhcp)) {
        use_dhcp = cJSON_IsTrue(dhcp);
        save_req->eth_dhcp = use_dhcp;
    }

    if (!use_dhcp) {
        // Static IP 모드: IP, gateway, netmask 모두 필수검증
        if (ip && cJSON_IsString(ip)) {
            if (!is_valid_ipv4(ip->valuestring)) {
                T_LOGW(TAG, "Invalid static IP: %s", ip->valuestring);
                return ESP_ERR_INVALID_ARG;
            }
            strncpy(save_req->eth_static_ip, ip->valuestring,
                   sizeof(save_req->eth_static_ip) - 1);
        } else {
            // 필수값 누락
            T_LOGW(TAG, "Static IP required but not provided");
            return ESP_ERR_INVALID_ARG;
        }

        if (gateway && cJSON_IsString(gateway)) {
            if (!is_valid_ipv4(gateway->valuestring)) {
                T_LOGW(TAG, "Invalid gateway: %s", gateway->valuestring);
                return ESP_ERR_INVALID_ARG;
            }
            strncpy(save_req->eth_gateway, gateway->valuestring,
                   sizeof(save_req->eth_gateway) - 1);
        } else {
            T_LOGW(TAG, "Gateway required but not provided");
            return ESP_ERR_INVALID_ARG;
        }

        if (netmask && cJSON_IsString(netmask)) {
            if (!is_valid_ipv4(netmask->valuestring)) {
                T_LOGW(TAG, "Invalid netmask: %s", netmask->valuestring);
                return ESP_ERR_INVALID_ARG;
            }
            strncpy(save_req->eth_netmask, netmask->valuestring,
                   sizeof(save_req->eth_netmask) - 1);
        } else {
            T_LOGW(TAG, "Netmask required but not provided");
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        // DHCP 모드: IP 필드가 있어도 무시
        save_req->eth_static_ip[0] = '\0';
        save_req->eth_gateway[0] = '\0';
        save_req->eth_netmask[0] = '\0';
    }

    if (enabled && cJSON_IsBool(enabled)) {
        save_req->eth_enabled = cJSON_IsTrue(enabled);
    }

    T_LOGD(TAG, "Publishing Ethernet save event: dhcp=%d, en=%d",
             save_req->eth_dhcp, save_req->eth_enabled);

    return ESP_OK;
}

// ============================================================================
// 네트워크 재시작 이벤트 발행 함수
// ============================================================================

void web_server_config_publish_network_restart(const config_save_request_t* save_req)
{
    network_restart_request_t restart_req;

    memset(&restart_req, 0, sizeof(restart_req));

    if (save_req->type == CONFIG_SAVE_WIFI_AP) {
        restart_req.type = NETWORK_RESTART_WIFI_AP;
        event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
    } else if (save_req->type == CONFIG_SAVE_WIFI_STA) {
        if (save_req->wifi_sta_enabled) {
            restart_req.type = NETWORK_RESTART_WIFI_STA;
            strncpy(restart_req.ssid, save_req->wifi_sta_ssid,
                   sizeof(restart_req.ssid) - 1);
            strncpy(restart_req.password, save_req->wifi_sta_password,
                   sizeof(restart_req.password) - 1);
            event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
        } else {
            restart_req.type = NETWORK_RESTART_WIFI_AP;
            event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
        }
    } else if (save_req->type == CONFIG_SAVE_ETHERNET) {
        restart_req.type = NETWORK_RESTART_ETHERNET;
        event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
    }
}

} // extern "C"
