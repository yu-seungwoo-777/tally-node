/**
 * @file info_manager_c.cpp
 * @brief InfoManager C 래퍼 구현
 */

#include "info/InfoManager.hpp"
#include "info/info_manager.h"
#include "log.h"
#include "log_tags.h"
#include <cstring>

static const char* TAG = TAG_INFO;

extern "C" {

esp_err_t info_manager_init(void)
{
    return info::InfoManager::init();
}

void info_manager_deinit(void)
{
    info::InfoManager::deinit();
}

bool info_manager_is_initialized(void)
{
    return info::InfoManager::get() != nullptr;
}

esp_err_t info_manager_get_device_id(char* buf, size_t buf_len)
{
    if (buf == nullptr || buf_len < INFO_DEVICE_ID_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        LOG_0(TAG, "InfoManager 미초기화");
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->getDeviceId();
    if (!result) {
        LOG_1(TAG, "장치 ID 조회 실패: %s", esp_err_to_name(result.error()));
        return result.error();
    }

    strncpy(buf, result.value().c_str(), buf_len - 1);
    buf[buf_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t info_manager_set_device_id(const char* device_id)
{
    if (device_id == nullptr || strlen(device_id) >= INFO_DEVICE_ID_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        LOG_0(TAG, "InfoManager 미초기화");
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->setDeviceId(std::string(device_id));
    if (!result) {
        LOG_1(TAG, "장치 ID 설정 실패: %s", esp_err_to_name(result.error()));
        return result.error();
    }

    return ESP_OK;
}

esp_err_t info_manager_generate_device_id(void)
{
    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        LOG_0(TAG, "InfoManager 미초기화");
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->generateDeviceId();
    if (!result) {
        LOG_0(TAG, "장치 ID 생성 실패: %s", esp_err_to_name(result.error()));
        return result.error();
    }

    return ESP_OK;
}

esp_err_t info_manager_get_system_info(info_system_info_t* info)
{
    if (info == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        LOG_0(TAG, "InfoManager 미초기화");
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->getSystemInfo();
    if (!result) {
        LOG_1(TAG, "시스템 정보 조회 실패: %s", esp_err_to_name(result.error()));
        return result.error();
    }

    *info = result.value();
    return ESP_OK;
}

esp_err_t info_manager_update_system_info(void)
{
    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        LOG_0(TAG, "InfoManager 미초기화");
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->updateSystemInfo();
    if (!result) {
        LOG_1(TAG, "시스템 정보 업데이트 실패: %s", esp_err_to_name(result.error()));
        return result.error();
    }

    return ESP_OK;
}

esp_err_t info_manager_add_observer(info_observer_fn_t callback,
                                     void* ctx,
                                     info_observer_handle_t* out_handle)
{
    if (callback == nullptr || out_handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        LOG_0(TAG, "InfoManager 미초기화");
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->addObserver(callback, ctx);
    if (!result) {
        LOG_1(TAG, "Observer 등록 실패: %s", esp_err_to_name(result.error()));
        return result.error();
    }

    *out_handle = result.value();
    return ESP_OK;
}

esp_err_t info_manager_remove_observer(info_observer_handle_t handle)
{
    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        LOG_0(TAG, "InfoManager 미초기화");
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->removeObserver(handle);
    if (!result) {
        LOG_1(TAG, "Observer 제거 실패: %s", esp_err_to_name(result.error()));
        return result.error();
    }

    return ESP_OK;
}

esp_err_t info_manager_notify_observers(void)
{
    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        LOG_0(TAG, "InfoManager 미초기화");
        return ESP_ERR_INVALID_STATE;
    }

    mgr->notifyObservers();
    return ESP_OK;
}

esp_err_t info_manager_increment_packet_tx(void)
{
    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->incrementPacketTx();
    return result.error();
}

esp_err_t info_manager_increment_packet_rx(void)
{
    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->incrementPacketRx();
    return result.error();
}

esp_err_t info_manager_set_lora_rssi(uint32_t rssi)
{
    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->setLoraRssi(rssi);
    return result.error();
}

esp_err_t info_manager_set_lora_snr(uint32_t snr)
{
    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->setLoraSnr(snr);
    return result.error();
}

esp_err_t info_manager_increment_error_count(void)
{
    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->incrementErrorCount();
    return result.error();
}

} // extern "C"