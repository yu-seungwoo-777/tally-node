/**
 * @file DeviceIdManager.cpp
 * @brief 장치 ID 관리자 구현
 *
 * WiFi MAC 주소 기반 장치 ID 생성 및 NVS 저장
 */

#include "info/info_types.h"
#include "log.h"
#include "log_tags.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char* TAG = TAG_INFO;

/**
 * @brief WiFi MAC 주소 기반 장치 ID 생성
 * @param device_id 결과 저장 버퍼 (최소 INFO_DEVICE_ID_MAX_LEN)
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t info_generate_device_id_from_mac(char* device_id)
{
    if (device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // WiFi MAC 주소 가져오기 (RX 모드에서도 동작하도록)
    uint8_t mac[6];
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        LOG_0(TAG, "WiFi MAC 주소 읽기 실패: %s", esp_err_to_name(err));
        return err;
    }

    // MAC 주소 마지막 4바이트를 16진수 문자열로 변환
    // 예: MAC=28:37:2F:8D:2C:28 -> "2C28"
    snprintf(device_id, INFO_DEVICE_ID_MAX_LEN, "%02X%02X",
             mac[4], mac[5]);

    LOG_0(TAG, "MAC 주소 기반 장치 ID 생성: %s (MAC=%02X:%02X:%02X:%02X:%02X:%02X)",
          device_id, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return ESP_OK;
}

/**
 * @brief NVS에서 장치 ID 로드
 * @param device_id 결과 저장 버퍼 (최소 INFO_DEVICE_ID_MAX_LEN)
 * @return ESP_OK 성공, ESP_ERR_NOT_FOUND 저장된 ID 없음, 그 외 에러
 */
esp_err_t info_load_device_id_from_nvs(char* device_id)
{
    if (device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(INFO_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        // 네임스페이스 없음 = 첫 부팅
        LOG_0(TAG, "NVS 네임스페이스 없음: %s", esp_err_to_name(err));
        return ESP_ERR_NOT_FOUND;
    }

    size_t len = INFO_DEVICE_ID_MAX_LEN;
    err = nvs_get_str(handle, INFO_NVS_KEY_DEVICE_ID, device_id, &len);
    nvs_close(handle);

    if (err == ESP_OK) {
        LOG_0(TAG, "NVS에서 장치 ID 로드: %s", device_id);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        LOG_0(TAG, "NVS에 장치 ID 없음");
        err = ESP_ERR_NOT_FOUND;
    } else {
        LOG_0(TAG, "NVS 장치 ID 로드 실패: %s", esp_err_to_name(err));
    }

    return err;
}

/**
 * @brief NVS에 장치 ID 저장
 * @param device_id 저장할 장치 ID
 * @return ESP_OK 성공, 그 외 에러
 */
esp_err_t info_save_device_id_to_nvs(const char* device_id)
{
    if (device_id == NULL || strlen(device_id) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(INFO_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG, "NVS 열기 실패: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, INFO_NVS_KEY_DEVICE_ID, device_id);
    if (err != ESP_OK) {
        LOG_0(TAG, "NVS 장치 ID 저장 실패: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // 생성 타입 저장
    uint8_t gen_type = INFO_ID_GEN_MAC_BASED;
    nvs_set_u8(handle, INFO_NVS_KEY_ID_TYPE, gen_type);

    // 첫 부팅 플래그 설정
    uint8_t first_boot = 0;
    nvs_set_u8(handle, INFO_NVS_KEY_FIRST_BOOT, first_boot);

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        LOG_0(TAG, "NVS에 장치 ID 저장: %s", device_id);
    }

    return err;
}

/**
 * @brief 첫 부팅 여부 확인
 * @return true 첫 부팅, false 첫 부팅 아님
 */
bool info_is_first_boot(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(INFO_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return true;  // 네임스페이스 없음 = 첫 부팅
    }

    uint8_t first_boot = 1;  // 기본값은 첫 부팅
    nvs_get_u8(handle, INFO_NVS_KEY_FIRST_BOOT, &first_boot);
    nvs_close(handle);

    return first_boot != 0;
}

/**
 * @brief 시스템 정보 구조체 초기화
 * @param info 초기화할 구조체 포인터
 */
void info_system_info_init(info_system_info_t* info)
{
    if (info == NULL) {
        return;
    }

    memset(info, 0, sizeof(info_system_info_t));

    // 기본값 설정
    info->battery_percent = 100.0f;
    info->temperature = 25.0f;
    strcpy(info->wifi_mac, "00:00:00:00:00:00");
    strcpy(info->device_id, "UNKNOWN");
}