/**
 * @file LoRaManager.cpp
 * @brief LoRa 통신 관리 Manager 구현
 */

#include "LoRaManager.h"
#include "LoRaPacket.h"
#include "log.h"
#include "log_tags.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

// ConfigCore를 통해 LoRa 설정 관리
extern "C" {
#include "ConfigCore.h"
}

static const char* TAG = TAG_LORA;

// 정적 멤버 초기화
bool LoRaManager::s_initialized = false;

// 임시 설정 저장 변수
static float s_pending_frequency = 0.0f;
static uint8_t s_pending_sync_word = 0x00;
static bool s_has_pending_config = false;

esp_err_t LoRaManager::init(const LoRaConfig* config)
{
    if (s_initialized) {
        LOG_0(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    // LoRaCore 초기화 (칩 감지 및 기본 설정)
    esp_err_t err = LoRaCore::init(config);
    if (err != ESP_OK) {
        LOG_0(TAG, "LoRaCore 초기화 실패");
        return ESP_FAIL;
    }

    // ConfigCore에서 LoRa 설정 가져오기
    ConfigLoRa lora_config = ConfigCore::getLoRa();

    // 칩 타입 확인 및 기본값 보정
    lora_status_t status = LoRaCore::getStatus();
    float config_freq = lora_config.frequency;

    // 칩 타입과 설정이 맞지 않으면 경고하고 적절한 값 사용
    if (status.chip_type == LORA_CHIP_SX1268 && config_freq > 500) {
        LOG_0(TAG, "경고: SX1268(433MHz) 칩이지만 %.1f MHz로 설정됨. 433MHz로 변경합니다.", config_freq);
        config_freq = 433.0f;
        lora_config.frequency = config_freq;
        // ConfigCore에 보정된 값 저장
        ConfigCore::setLoRa(lora_config);
    } else if (status.chip_type == LORA_CHIP_SX1262 && config_freq < 500) {
        LOG_0(TAG, "경고: SX1262(868MHz) 칩이지만 %.1f MHz로 설정됨. 868MHz로 변경합니다.", config_freq);
        config_freq = 868.0f;
        lora_config.frequency = config_freq;
        // ConfigCore에 보정된 값 저장
        ConfigCore::setLoRa(lora_config);
    }

    // 설정 적용
    if (config_freq != status.frequency) {
        err = LoRaCore::setFrequency(config_freq);
        if (err != ESP_OK) {
            LOG_0(TAG, "주파수 설정 실패: %.1f MHz", config_freq);
            return err;
        }
    }

    err = LoRaCore::setSyncWord(lora_config.sync_word);
    if (err != ESP_OK) {
        LOG_0(TAG, "Sync Word 설정 실패: 0x%02X", lora_config.sync_word);
        return err;
    }

    LoRaCore::startReceive();

    s_initialized = true;
    LOG_0(TAG, "초기화 완료: %.1f MHz, Sync Word: 0x%02X", config_freq, lora_config.sync_word);
    return ESP_OK;
}

esp_err_t LoRaManager::transmit(const uint8_t* data, size_t length)
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return ESP_FAIL;
    }

    return LoRaCore::transmit(data, length);
}

esp_err_t LoRaManager::transmitAsync(const uint8_t* data, size_t length)
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return ESP_FAIL;
    }

    return LoRaCore::transmitAsync(data, length);
}

esp_err_t LoRaManager::startReceive()
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return ESP_FAIL;
    }

    return LoRaCore::startReceive();
}

void LoRaManager::setReceiveCallback(LoRaReceiveCallback callback)
{
    LoRaCore::setReceiveCallback(callback);
}

void LoRaManager::checkReceived()
{
    LoRaCore::checkReceived();
}

void LoRaManager::checkTransmitted()
{
    LoRaCore::checkTransmitted();
}

bool LoRaManager::isTransmitting()
{
    return LoRaCore::isTransmitting();
}

lora_status_t LoRaManager::getStatus()
{
    return LoRaCore::getStatus();
}

bool LoRaManager::isInitialized()
{
    return s_initialized;
}

esp_err_t LoRaManager::scanChannels(float start_freq, float end_freq, float step,
                                     channel_info_t* results, size_t max_results,
                                     size_t* result_count)
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return ESP_FAIL;
    }

    return LoRaCore::scanChannels(start_freq, end_freq, step, results, max_results, result_count);
}

esp_err_t LoRaManager::setFrequency(float freq)
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return ESP_FAIL;
    }

    return LoRaCore::setFrequency(freq);
}

esp_err_t LoRaManager::setSyncWord(uint8_t sync_word)
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return ESP_FAIL;
    }

    return LoRaCore::setSyncWord(sync_word);
}

void LoRaManager::setPendingFrequency(float freq)
{
    s_pending_frequency = freq;
    s_has_pending_config = true;
    LOG_0(TAG, "주파수 임시 저장: %.1f MHz", freq);
}

void LoRaManager::setPendingSyncWord(uint8_t sync_word)
{
    s_pending_sync_word = sync_word;
    s_has_pending_config = true;
    LOG_0(TAG, "Sync Word 임시 저장: 0x%02X", sync_word);
}

bool LoRaManager::hasPendingConfig()
{
    return s_has_pending_config;
}

esp_err_t LoRaManager::applyPendingConfig()
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return ESP_FAIL;
    }

    if (!s_has_pending_config) {
        LOG_0(TAG, "임시 저장된 설정 없음");
        return ESP_OK;
    }

    // 1. Config Change 패킷 생성
    uint8_t packet_buffer[CONFIG_CHANGE_PACKET_SIZE];
    size_t packet_size = LoRaPacket::createConfigChangePacket(
        s_pending_frequency, s_pending_sync_word, packet_buffer, sizeof(packet_buffer)
    );

    if (packet_size == 0) {
        LOG_0(TAG, "Config Change 패킷 생성 실패");
        return ESP_FAIL;
    }

    LOG_0(TAG, "설정 적용 시작: %.1f MHz, 0x%02X", s_pending_frequency, s_pending_sync_word);

    // 2. 1초 간격으로 3회 전송
    for (int i = 0; i < 3; i++) {
        esp_err_t err = LoRaCore::transmit(packet_buffer, packet_size);
        if (err != ESP_OK) {
            LOG_0(TAG, "Config Change 패킷 전송 실패 (%d/3)", i+1);
        } else {
            LOG_0(TAG, "Config Change 패킷 전송 (%d/3)", i+1);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1초 대기
    }

    // 3. 마지막 전송 후 1초 대기 (RX가 설정 변경할 시간)
    LOG_0(TAG, "RX 설정 변경 대기 중...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 4. TX 설정 변경
    if (s_pending_frequency > 0) {
        esp_err_t err = LoRaCore::setFrequency(s_pending_frequency);
        if (err != ESP_OK) {
            LOG_0(TAG, "주파수 변경 실패");
            return ESP_FAIL;
        }
    }

    if (s_pending_sync_word > 0) {
        esp_err_t err = LoRaCore::setSyncWord(s_pending_sync_word);
        if (err != ESP_OK) {
            LOG_0(TAG, "Sync Word 변경 실패");
            return ESP_FAIL;
        }
    }

    // 수신 모드 재시작
    LoRaCore::startReceive();

    // 5. ConfigCore에 저장
    ConfigLoRa lora_config = ConfigCore::getLoRa();

    if (s_pending_frequency > 0) {
        lora_config.frequency = s_pending_frequency;
    }
    if (s_pending_sync_word > 0) {
        lora_config.sync_word = s_pending_sync_word;
    }

    esp_err_t nvs_err = ConfigCore::setLoRa(lora_config);
    if (nvs_err == ESP_OK) {
        LOG_0(TAG, "ConfigCore에 LoRa 설정 저장 완료");
    } else {
        LOG_0(TAG, "ConfigCore 저장 실패: %s", esp_err_to_name(nvs_err));
    }

    // 6. 임시 저장 클리어
    s_has_pending_config = false;
    s_pending_frequency = 0.0f;
    s_pending_sync_word = 0x00;

    LOG_0(TAG, "설정 적용 완료");
    return ESP_OK;
}

// ============================================================================
// C 래퍼 함수 (DisplayManager 등 C 코드에서 사용)
// ============================================================================

extern "C" {

bool LoRaManager_isInitialized(void)
{
    return LoRaManager::isInitialized();
}

lora_status_t LoRaManager_getStatus(void)
{
    return LoRaManager::getStatus();
}

} // extern "C"
