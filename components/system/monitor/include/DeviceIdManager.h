/**
 * @file DeviceIdManager.h
 * @brief Device ID 관리자 (NVS 기반)
 *
 * 기능:
 * - WiFi MAC 마지막 4자리를 Device ID로 사용
 * - NVS에 영구 저장
 * - 자동 생성 및 조회
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief Device ID 관리자
 */
class DeviceIdManager {
public:
    // Device ID 최대 길이
    static const uint8_t DEVICE_ID_MAX_LEN = 8;  // "XXXX" + null

    /**
     * @brief 초기화
     * @return ESP_OK 성공, 그 외 에러
     */
    static esp_err_t init();

    /**
     * @brief Device ID 조회
     * @param deviceId Device ID를 저장할 버퍼 (최소 DEVICE_ID_MAX_LEN 바이트)
     * @return ESP_OK 성공, 그 외 에러
     */
    static esp_err_t getDeviceId(char* deviceId);

    /**
     * @brief Device ID 강제 설정 (테스트용)
     * @param deviceId 설정할 Device ID
     * @return ESP_OK 성공, 그 외 에러
     */
    static esp_err_t setDeviceId(const char* deviceId);

private:
    // NVS 네임스페이스
    static const char* NAMESPACE;

    // NVS 키
    static const char* KEY_DEVICE_ID;

    // 싱글톤 패턴
    DeviceIdManager() = delete;
    ~DeviceIdManager() = delete;
    DeviceIdManager(const DeviceIdManager&) = delete;
    DeviceIdManager& operator=(const DeviceIdManager&) = delete;

    /**
     * @brief WiFi MAC에서 Device ID 생성
     * @param deviceId 생성된 Device ID를 저장할 버퍼
     * @return ESP_OK 성공, 그 외 에러
     */
    static esp_err_t generateDeviceIdFromMac(char* deviceId);
};