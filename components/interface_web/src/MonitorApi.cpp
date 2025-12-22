/**
 * @file MonitorApi.cpp
 * @brief 시스템 모니터링 REST API 구현 (TX 전용)
 */

// TX 모드에서만 빌드
#ifdef DEVICE_MODE_TX

#include "MonitorApi.h"
#include "SystemMonitor.h"
#include "NetworkManager.h"
#include "SwitcherManager.h"
#include "service/TallyDispatcher.h"
#include "LoRaManager.h"
#include "LoRaCore.h"
#include "switcher.h"
#include "info/info_manager.h"
#include "cJSON.h"
#include <string.h>

esp_err_t MonitorApi::statusHandler(httpd_req_t* req)
{
    NetworkStatus status = NetworkManager::getStatus();

    cJSON* root = cJSON_CreateObject();

    // WiFi AP
    cJSON* wifi_ap = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi_ap, "active", status.wifi_ap.active);
    cJSON_AddBoolToObject(wifi_ap, "connected", status.wifi_ap.connected);
    cJSON_AddStringToObject(wifi_ap, "ip", status.wifi_ap.ip);
    cJSON_AddItemToObject(root, "wifi_ap", wifi_ap);

    // WiFi STA
    cJSON* wifi_sta = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi_sta, "active", status.wifi_sta.active);
    cJSON_AddBoolToObject(wifi_sta, "connected", status.wifi_sta.connected);
    cJSON_AddStringToObject(wifi_sta, "ip", status.wifi_sta.ip);
    cJSON_AddItemToObject(root, "wifi_sta", wifi_sta);

    // WiFi 상세
    cJSON* wifi_detail = cJSON_CreateObject();
    cJSON_AddNumberToObject(wifi_detail, "ap_clients", status.wifi_detail.ap_clients);
    cJSON_AddNumberToObject(wifi_detail, "sta_rssi", status.wifi_detail.sta_rssi);
    cJSON_AddItemToObject(root, "wifi_detail", wifi_detail);

    // Ethernet
    cJSON* ethernet = cJSON_CreateObject();
    cJSON_AddBoolToObject(ethernet, "active", status.ethernet.active);
    cJSON_AddBoolToObject(ethernet, "connected", status.ethernet.connected);
    cJSON_AddStringToObject(ethernet, "ip", status.ethernet.ip);
    cJSON_AddStringToObject(ethernet, "netmask", status.ethernet.netmask);
    cJSON_AddStringToObject(ethernet, "gateway", status.ethernet.gateway);
    cJSON_AddItemToObject(root, "ethernet", ethernet);

    // Ethernet 상세
    cJSON* eth_detail = cJSON_CreateObject();
    cJSON_AddBoolToObject(eth_detail, "link_up", status.eth_detail.link_up);
    cJSON_AddBoolToObject(eth_detail, "dhcp_mode", status.eth_detail.dhcp_mode);
    cJSON_AddStringToObject(eth_detail, "mac", status.eth_detail.mac);
    cJSON_AddItemToObject(root, "eth_detail", eth_detail);

    // Switcher 정보
    cJSON* switcher = cJSON_CreateObject();

    // 전체 상태
    cJSON_AddBoolToObject(switcher, "initialized", SwitcherManager::isInitialized());
    cJSON_AddBoolToObject(switcher, "dual_mode", SwitcherManager::isDualMode());
    cJSON_AddNumberToObject(switcher, "active_count", SwitcherManager::getActiveSwitcherCount());
    cJSON_AddBoolToObject(switcher, "comm_initialized", TallyDispatcher::isInitialized());

    // Primary 스위처
    cJSON* primary = cJSON_CreateObject();
    bool primary_connected = SwitcherManager::isConnected(SWITCHER_INDEX_PRIMARY);
    cJSON_AddBoolToObject(primary, "connected", primary_connected);

    if (primary_connected) {
        switcher_t* handle = SwitcherManager::getHandle(SWITCHER_INDEX_PRIMARY);
        if (handle) {
            switcher_state_t state;
            if (switcher_get_state(handle, &state) == SWITCHER_OK) {
                cJSON_AddNumberToObject(primary, "program", state.program_input);
                cJSON_AddNumberToObject(primary, "preview", state.preview_input);
            }

            // 스위처 정보 추가
            switcher_info_t info;
            if (switcher_get_info(handle, &info) == SWITCHER_OK) {
                cJSON_AddStringToObject(primary, "product", info.product_name);
                cJSON_AddNumberToObject(primary, "num_cameras", info.num_cameras);
            }
        }
    }
    cJSON_AddItemToObject(switcher, "primary", primary);

    // Secondary 스위처
    cJSON* secondary = cJSON_CreateObject();
    bool secondary_connected = SwitcherManager::isConnected(SWITCHER_INDEX_SECONDARY);
    cJSON_AddBoolToObject(secondary, "connected", secondary_connected);

    if (secondary_connected) {
        switcher_t* handle = SwitcherManager::getHandle(SWITCHER_INDEX_SECONDARY);
        if (handle) {
            switcher_state_t state;
            if (switcher_get_state(handle, &state) == SWITCHER_OK) {
                cJSON_AddNumberToObject(secondary, "program", state.program_input);
                cJSON_AddNumberToObject(secondary, "preview", state.preview_input);
            }

            // 스위처 정보 추가
            switcher_info_t info;
            if (switcher_get_info(handle, &info) == SWITCHER_OK) {
                cJSON_AddStringToObject(secondary, "product", info.product_name);
                cJSON_AddNumberToObject(secondary, "num_cameras", info.num_cameras);
            }
        }
    }
    cJSON_AddItemToObject(switcher, "secondary", secondary);

    cJSON_AddItemToObject(root, "switcher", switcher);

    // LoRa 상태
    cJSON* lora = cJSON_CreateObject();
    lora_status_t lora_status = LoRaManager::getStatus();
    cJSON_AddBoolToObject(lora, "initialized", lora_status.is_initialized);
    cJSON_AddNumberToObject(lora, "chip_type", (int)lora_status.chip_type);
    cJSON_AddStringToObject(lora, "chip_name", LoRaCore::getChipName());
    cJSON_AddNumberToObject(lora, "frequency", lora_status.frequency);
    cJSON_AddNumberToObject(lora, "freq_min", lora_status.freq_min);
    cJSON_AddNumberToObject(lora, "freq_max", lora_status.freq_max);
    cJSON_AddNumberToObject(lora, "sync_word", lora_status.sync_word);
    cJSON_AddItemToObject(root, "lora", lora);

    char* json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t MonitorApi::healthHandler(httpd_req_t* req)
{
    SystemHealth health = SystemMonitor::getHealth();

    cJSON* root = cJSON_CreateObject();

    // 업타임
    cJSON_AddNumberToObject(root, "uptime_sec", (double)health.uptime_sec);

    // 온도
    cJSON* temp_obj = cJSON_CreateObject();
    if (health.temperature_celsius > 0.0f) {
        cJSON_AddNumberToObject(temp_obj, "celsius", health.temperature_celsius);
    }
    cJSON_AddItemToObject(root, "temperature", temp_obj);

    // 전압 및 배터리
    cJSON* voltage_obj = cJSON_CreateObject();
    if (health.voltage > 0.0f) {
        cJSON_AddNumberToObject(voltage_obj, "volts", health.voltage);
        cJSON_AddNumberToObject(voltage_obj, "percentage", health.battery_percent);
    }
    cJSON_AddItemToObject(root, "voltage", voltage_obj);

    // Device 정보
    if (info_manager_is_initialized()) {
        info_system_info_t info;
        if (info_manager_get_system_info(&info) == ESP_OK) {
            cJSON_AddStringToObject(root, "device_id", info.device_id);
            cJSON_AddStringToObject(root, "wifi_mac", info.wifi_mac);

            // 패킷 통계
            cJSON* stats_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(stats_obj, "packet_tx", info.packet_count_tx);
            cJSON_AddNumberToObject(stats_obj, "packet_rx", info.packet_count_rx);
            cJSON_AddNumberToObject(stats_obj, "error_count", info.error_count);
            cJSON_AddItemToObject(root, "packet_stats", stats_obj);
        } else {
            // 실패 시 기본값
            cJSON_AddStringToObject(root, "device_id", "????");
            cJSON_AddStringToObject(root, "wifi_mac", "00:00:00:00:00:00");
        }
    } else {
        // InfoManager 미초기화 시 기본값
        cJSON_AddStringToObject(root, "device_id", "????");
        cJSON_AddStringToObject(root, "wifi_mac", "00:00:00:00:00:00");
    }

    char* json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

#endif  // DEVICE_MODE_TX
