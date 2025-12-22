/**
 * @file DeviceIdManager.cpp
 * @brief Device ID 관리자 구현 (NVS 기반)
 */

#include "DeviceIdManager.h"
#include "log.h"
#include "log_tags.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char* TAG = TAG_MONITOR;

// NVS 설정
const char* DeviceIdManager::NAMESPACE = "device_config";
const char* DeviceIdManager::KEY_DEVICE_ID = "device_id";

// NVS 핸들 (전역 변수)
static nvs_handle_t s_nvs_handle = 0;
static bool s_nvs_initialized = false;

esp_err_t DeviceIdManager::init()
{
    esp_err_t ret;

    // 이미 초기화되었으면 생략
    if (s_nvs_initialized) {
        return ESP_OK;
    }

    // NVS 열기
    ret = nvs_open(NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) {
        LOG_0(TAG, "NVS 열기 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 기존 Device ID 확인
    char existing_id[DEVICE_ID_MAX_LEN];
    size_t required_size = 0;

    ret = nvs_get_str(s_nvs_handle, KEY_DEVICE_ID, NULL, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // Device ID가 없으면 생성
        LOG_1(TAG, "Device ID 없음, 자동 생성...");

        char new_id[DEVICE_ID_MAX_LEN];
        ret = generateDeviceIdFromMac(new_id);
        if (ret != ESP_OK) {
            LOG_0(TAG, "Device ID 생성 실패: %s", esp_err_to_name(ret));
            nvs_close(s_nvs_handle);
            return ret;
        }

        // NVS에 저장
        ret = nvs_set_str(s_nvs_handle, KEY_DEVICE_ID, new_id);
        if (ret != ESP_OK) {
            LOG_0(TAG, "Device ID NVS 저장 실패: %s", esp_err_to_name(ret));
            nvs_close(s_nvs_handle);
            return ret;
        }

        // 커밋
        ret = nvs_commit(s_nvs_handle);
        if (ret != ESP_OK) {
            LOG_0(TAG, "NVS 커밋 실패: %s", esp_err_to_name(ret));
            nvs_close(s_nvs_handle);
            return ret;
        }

        LOG_0(TAG, "Device ID 생성 및 저장 완료: %s", new_id);
    } else if (ret != ESP_OK) {
        LOG_0(TAG, "Device ID 조회 실패: %s", esp_err_to_name(ret));
        nvs_close(s_nvs_handle);
        return ret;
    }

    s_nvs_initialized = true;
    LOG_1(TAG, "DeviceIdManager 초기화 완료");

    return ESP_OK;
}

esp_err_t DeviceIdManager::getDeviceId(char* deviceId)
{
    if (!deviceId) {
        return ESP_ERR_INVALID_ARG;
    }

    // 초기화 확인
    if (!s_nvs_initialized) {
        esp_err_t ret = init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    // NVS에서 Device ID 읽기
    size_t required_size = DEVICE_ID_MAX_LEN;
    esp_err_t ret = nvs_get_str(s_nvs_handle, KEY_DEVICE_ID, deviceId, &required_size);

    if (ret != ESP_OK) {
        LOG_0(TAG, "Device ID 읽기 실패: %s", esp_err_to_name(ret));

        // 실패 시 임시 ID 생성
        strcpy(deviceId, "FFFF");
        return ESP_FAIL;
    }

    LOG_1(TAG, "Device ID 조회: %s", deviceId);
    return ESP_OK;
}

esp_err_t DeviceIdManager::setDeviceId(const char* deviceId)
{
    if (!deviceId || strlen(deviceId) != 4) {
        return ESP_ERR_INVALID_ARG;
    }

    // 초기화 확인
    if (!s_nvs_initialized) {
        esp_err_t ret = init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    // NVS에 저장
    esp_err_t ret = nvs_set_str(s_nvs_handle, KEY_DEVICE_ID, deviceId);
    if (ret != ESP_OK) {
        LOG_0(TAG, "Device ID 설정 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 커밋
    ret = nvs_commit(s_nvs_handle);
    if (ret != ESP_OK) {
        LOG_0(TAG, "NVS 커밋 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    LOG_0(TAG, "Device ID 설정 완료: %s", deviceId);
    return ESP_OK;
}

// 임시 WiFi netif 초기화 및 MAC 주소 획득
static esp_err_t initWifiForMacAddress(uint8_t* mac)
{
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;

    // WiFi netif 초기화
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        LOG_0(TAG, "esp_netif_init 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 기본 WiFi 이벤트 루프 생성
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        LOG_0(TAG, "이벤트 루프 생성 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // WiFi STA용 netif 생성
    esp_netif_t* netif = esp_netif_create_default_wifi_sta();
    if (!netif) {
        LOG_0(TAG, "WiFi STA netif 생성 실패");
        return ESP_FAIL;
    }

    // WiFi 초기화 (기본 설정)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        LOG_0(TAG, "WiFi 초기화 실패: %s", esp_err_to_name(ret));
        esp_netif_destroy(netif);
        return ret;
    }

    // WiFi STA 모드 설정
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        LOG_0(TAG, "WiFi 모드 설정 실패: %s", esp_err_to_name(ret));
        esp_wifi_deinit();
        esp_netif_destroy(netif);
        return ret;
    }

    // MAC 주소 가져오기
    ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (ret != ESP_OK) {
        LOG_0(TAG, "WiFi MAC 주소 가져오기 실패: %s", esp_err_to_name(ret));
    } else {
        LOG_1(TAG, "WiFi MAC 주소 획득 성공: %02X:%02X:%02X:%02X:%02X:%02X",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // 정리: WiFi 설정 해제
    esp_wifi_deinit();
    esp_netif_destroy(netif);

    return ret;
}

esp_err_t DeviceIdManager::generateDeviceIdFromMac(char* deviceId)
{
    if (!deviceId) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t mac[6];
    esp_err_t ret;

    // 1. 먼저 esp_read_mac으로 시도 (기본 이페스 MAC)
    ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret == ESP_OK) {
        // 기본 MAC이 아닌 실제 MAC인지 확인 (ESP 기본값: 24:0A:C4:XX:XX:XX 등)
        bool is_default_mac = (mac[0] == 0x24 && mac[1] == 0x0A && mac[2] == 0xC4) ||
                              (mac[0] == 0x30 && mac[1] == 0xAE && mac[2] == 0xA4) ||
                              (mac[0] == 0x84 && mac[1] == 0xFC && mac[2] == 0x03);

        if (!is_default_mac) {
            LOG_1(TAG, "실제 WiFi MAC 주소 발견");
            goto success;
        }
    }

    // 2. 기본 MAC이면 WiFi 임시 초기화로 실제 MAC 획득 시도
    LOG_1(TAG, "기본 MAC 주소 감지, WiFi 초기화로 실제 MAC 획득 시도");
    ret = initWifiForMacAddress(mac);
    if (ret != ESP_OK) {
        LOG_0(TAG, "WiFi 초기화로 MAC 획득 실패: %s", esp_err_to_name(ret));

        // 실패 시 기본값
        strcpy(deviceId, "FFFF");
        return ESP_FAIL;
    }

success:
    // MAC 마지막 4자리를 16진수 대문자로 변환
    snprintf(deviceId, DEVICE_ID_MAX_LEN, "%02X%02X", mac[4], mac[5]);

    LOG_0(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X → Device ID: %s",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], deviceId);

    return ESP_OK;
}