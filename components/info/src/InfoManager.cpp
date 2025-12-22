/**
 * @file InfoManager.cpp
 * @brief InfoManager C++ 구현
 */

#include "info/InfoManager.hpp"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "log.h"
#include "log_tags.h"
#include <cstring>
#include <cstdio>

namespace info {

// 정적 멤버 초기화
InfoManager* InfoManager::instance_ = nullptr;
std::mutex InfoManager::init_mutex_;

esp_err_t InfoManager::init()
{
    std::lock_guard<std::mutex> lock(init_mutex_);

    if (instance_ != nullptr) {
        LOG_0(TAG_INFO, "InfoManager 이미 초기화됨");
        return ESP_OK;
    }

    instance_ = new InfoManager();
    if (!instance_) {
        LOG_0(TAG_INFO, "InfoManager 메모리 할당 실패");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = instance_->loadFromNvs();
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        LOG_0(TAG_INFO, "InfoManager NVS 로드 실패: %s", esp_err_to_name(err));
        delete instance_;
        instance_ = nullptr;
        return err;
    }

    // 장치 ID가 없으면 직접 생성
    if (instance_->device_id_.empty()) {
        LOG_0(TAG_INFO, "장치 ID 없음, MAC 주소로 생성");
        std::string new_id = instance_->generateDeviceIdFromMac();
        if (new_id.empty()) {
            LOG_0(TAG_INFO, "장치 ID 생성 실패");
            delete instance_;
            instance_ = nullptr;
            return ESP_FAIL;
        }

        // 초기화 중이므로 직접 설정 (setDeviceId() 사용 안 함)
        instance_->device_id_ = new_id;
        LOG_0(TAG_INFO, "MAC 기반 장치 ID 생성: %s", new_id.c_str());

        // NVS에 저장
        esp_err_t save_err = instance_->saveToNvs();
        if (save_err != ESP_OK) {
            LOG_0(TAG_INFO, "장치 ID 저장 실패: %s", esp_err_to_name(save_err));
        }
    }

    // 캐시된 정보 초기화
    info_system_info_init(&instance_->cached_info_);
    strncpy(instance_->cached_info_.device_id,
            instance_->device_id_.c_str(),
            INFO_DEVICE_ID_MAX_LEN - 1);
    instance_->cached_info_.device_id[INFO_DEVICE_ID_MAX_LEN - 1] = '\0';

    // 이제 초기화 완료 표시
    instance_->initialized_ = true;
    LOG_0(TAG_INFO, "InfoManager 초기화 완료 (Device ID: %s)",
          instance_->device_id_.c_str());

    return ESP_OK;
}

void InfoManager::deinit()
{
    std::lock_guard<std::mutex> lock(init_mutex_);

    if (instance_ == nullptr) {
        return;
    }

    // 변경된 내용 저장
    if (instance_->dirty_) {
        instance_->saveToNvs();
    }

    delete instance_;
    instance_ = nullptr;

    LOG_0(TAG_INFO, "InfoManager 해제 완료");
}

InfoManager* InfoManager::get()
{
    return instance_;
}

InfoManager::InfoManager()
{
}

InfoManager::~InfoManager()
{
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.clear();
}

esp_err_t InfoManager::loadFromNvs()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(INFO_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;  // 첫 부팅
    }

    char buf[INFO_DEVICE_ID_MAX_LEN];
    size_t len = sizeof(buf);
    err = nvs_get_str(handle, INFO_NVS_KEY_DEVICE_ID, buf, &len);
    if (err == ESP_OK) {
        device_id_ = buf;
        LOG_0(TAG_INFO, "NVS에서 장치 ID 로드: %s", device_id_.c_str());
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t InfoManager::saveToNvs()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(INFO_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        LOG_0(TAG_INFO, "NVS 열기 실패: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, INFO_NVS_KEY_DEVICE_ID, device_id_.c_str());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        dirty_ = false;
        LOG_1(TAG_INFO, "NVS에 장치 ID 저장: %s", device_id_.c_str());
    } else {
        LOG_0(TAG_INFO, "NVS 장치 ID 저장 실패: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

std::string InfoManager::generateDeviceIdFromMac()
{
    uint8_t mac[6];
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        LOG_0(TAG_INFO, "WiFi MAC 주소 읽기 실패: %s", esp_err_to_name(err));
        return std::string();
    }

    char device_id[INFO_DEVICE_ID_MAX_LEN];
    snprintf(device_id, INFO_DEVICE_ID_MAX_LEN, "%02X%02X",
             mac[4], mac[5]);

    LOG_0(TAG_INFO, "MAC 기반 장치 ID 생성: %s", device_id);
    return std::string(device_id);
}

Result<std::string> InfoManager::getDeviceId() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return Result<std::string>::fail(ESP_ERR_INVALID_STATE);
    }

    if (device_id_.empty()) {
        return Result<std::string>::fail(ESP_ERR_NOT_FOUND);
    }

    return Result<std::string>::ok(device_id_);
}

VoidResult InfoManager::setDeviceId(const std::string& device_id)
{
    if (device_id.length() >= INFO_DEVICE_ID_MAX_LEN) {
        return VoidResult::fail(ESP_ERR_INVALID_ARG);
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 초기화 중에도 장치 ID 설정은 허용
    // (init() 함수에서 generateDeviceId() 호출 시 필요)
    if (!initialized_ && instance_ != nullptr) {
        // 초기화 중인 경우: device_id_만 설정
        device_id_ = device_id;
        LOG_0(TAG_INFO, "장치 ID 설정 (초기화 중): %s", device_id.c_str());
        return Ok();
    } else if (!initialized_) {
        return VoidResult::fail(ESP_ERR_INVALID_STATE);
    }

    device_id_ = device_id;
    dirty_ = true;

    // 캐시 업데이트
    strncpy(cached_info_.device_id, device_id.c_str(), INFO_DEVICE_ID_MAX_LEN - 1);
    cached_info_.device_id[INFO_DEVICE_ID_MAX_LEN - 1] = '\0';

    LOG_0(TAG_INFO, "장치 ID 설정: %s", device_id.c_str());

    return Ok();
}

VoidResult InfoManager::generateDeviceId()
{
    std::string new_id = generateDeviceIdFromMac();
    if (new_id.empty()) {
        return VoidResult::fail(ESP_FAIL);
    }

    return setDeviceId(new_id);
}

Result<info_system_info_t> InfoManager::getSystemInfo() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return Result<info_system_info_t>::fail(ESP_ERR_INVALID_STATE);
    }

    return Result<info_system_info_t>::ok(cached_info_);
}

VoidResult InfoManager::updateSystemInfo()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return VoidResult::fail(ESP_ERR_INVALID_STATE);
    }

    // 시스템 정보 업데이트
    cached_info_.uptime_sec = 0;  // SystemMonitor에서 관리
    cached_info_.free_heap = esp_get_free_heap_size();
    cached_info_.min_free_heap = esp_get_minimum_free_heap_size();

    // 온도 정보는 SystemMonitor에서 관리하므로 기본값 설정
    // SystemMonitor의 Observer를 통해 업데이트됨
    cached_info_.temperature = 0.0f;  // 초기값

    // WiFi MAC 주소
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        snprintf(cached_info_.wifi_mac, INFO_MAC_ADDR_STR_LEN,
                "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    LOG_1(TAG_INFO, "시스템 정보 업데이트: Heap=%u, MinHeap=%u, Uptime=%u",
          cached_info_.free_heap, cached_info_.min_free_heap, cached_info_.uptime_sec);

    return Ok();
}

Result<info_observer_handle_t> InfoManager::addObserver(info_observer_fn_t callback, void* ctx)
{
    if (!callback) {
        return Result<info_observer_handle_t>::fail(ESP_ERR_INVALID_ARG);
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return Result<info_observer_handle_t>::fail(ESP_ERR_INVALID_STATE);
    }

    // 빈 슬롯 찾기
    for (auto& entry : observers_) {
        if (!entry.active) {
            entry.handle = reinterpret_cast<info_observer_handle_t>(next_observer_id_++);
            entry.callback = callback;
            entry.ctx = ctx;
            entry.active = true;

            LOG_1(TAG_INFO, "Observer 등록: handle=%p", entry.handle);
            return Result<info_observer_handle_t>::ok(entry.handle);
        }
    }

    // 새로운 엔트리 추가
    ObserverEntry entry;
    entry.handle = reinterpret_cast<info_observer_handle_t>(next_observer_id_++);
    entry.callback = callback;
    entry.ctx = ctx;
    entry.active = true;

    observers_.push_back(entry);

    LOG_1(TAG_INFO, "Observer 등록: handle=%p", entry.handle);
    return Result<info_observer_handle_t>::ok(entry.handle);
}

VoidResult InfoManager::removeObserver(info_observer_handle_t handle)
{
    if (!handle) {
        return VoidResult::fail(ESP_ERR_INVALID_ARG);
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return VoidResult::fail(ESP_ERR_INVALID_STATE);
    }

    for (auto& entry : observers_) {
        if (entry.active && entry.handle == handle) {
            entry.active = false;
            LOG_1(TAG_INFO, "Observer 제거: handle=%p", handle);
            return Ok();
        }
    }

    return VoidResult::fail(ESP_ERR_NOT_FOUND);
}

void InfoManager::notifyObservers()
{
    // 콜백 목록 복사 (뮤텍스 보호 하에)
    std::vector<ObserverEntry> observers_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_copy = observers_;
    }

    // 뮤텍스 해제 후 콜백 호출 (데드락 방지)
    for (const auto& entry : observers_copy) {
        if (entry.active && entry.callback) {
            entry.callback(&cached_info_, entry.ctx);
        }
    }
}

VoidResult InfoManager::incrementPacketTx()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return VoidResult::fail(ESP_ERR_INVALID_STATE);
    }

    cached_info_.packet_count_tx++;
    dirty_ = true;  // NVS 저장 필요

    return Ok();
}

VoidResult InfoManager::incrementPacketRx()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return VoidResult::fail(ESP_ERR_INVALID_STATE);
    }

    cached_info_.packet_count_rx++;
    dirty_ = true;  // NVS 저장 필요

    return Ok();
}

VoidResult InfoManager::setLoraRssi(uint32_t rssi)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return VoidResult::fail(ESP_ERR_INVALID_STATE);
    }

    cached_info_.lora_rssi = rssi;
    dirty_ = true;  // 즉시 옵저버에게 알림

    return Ok();
}

VoidResult InfoManager::setLoraSnr(uint32_t snr)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return VoidResult::fail(ESP_ERR_INVALID_STATE);
    }

    cached_info_.lora_snr = snr;
    dirty_ = true;  // 즉시 옵저버에게 알림

    return Ok();
}

VoidResult InfoManager::incrementErrorCount()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return VoidResult::fail(ESP_ERR_INVALID_STATE);
    }

    cached_info_.error_count++;
    dirty_ = true;  // NVS 저장 필요

    return Ok();
}

} // namespace info