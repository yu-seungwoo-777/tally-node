/**
 * @file SystemMonitor.h
 * @brief 시스템 하드웨어 모니터링 Core API
 *
 * Core 역할:
 * - 하드웨어 센서 추상화 (ADC, 온도, CPU, 메모리)
 * - 상태 없음 (Stateless)
 * - 재사용 가능한 독립 컴포넌트
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief 시스템 상태 정보
 */
struct SystemHealth {
    uint64_t uptime_sec;        // 업타임 (초)
    float temperature_celsius;  // 온도 (°C)
    float voltage;              // 전압 (V)
    uint8_t battery_percent;    // 배터리 (%)
};

/**
 * @brief 시스템 모니터링 Core API (정적 클래스)
 */
class SystemMonitor {
public:
    /**
     * @brief 초기화
     *
     * ADC, 온도 센서 등 하드웨어 초기화
     */
    static esp_err_t init();

    /**
     * @brief 전체 시스템 상태 조회
     */
    static SystemHealth getHealth();

    /**
     * @brief 업타임 조회 (초)
     */
    static uint64_t getUptime();

    /**
     * @brief 온도 조회 (°C)
     */
    static float getTemperature();

    /**
     * @brief 전압 조회 (V)
     */
    static float getVoltage();

    /**
     * @brief 배터리 잔량 조회 (%)
     */
    static uint8_t getBatteryPercent();

private:
    // 싱글톤 패턴
    SystemMonitor() = delete;
    ~SystemMonitor() = delete;
    SystemMonitor(const SystemMonitor&) = delete;
    SystemMonitor& operator=(const SystemMonitor&) = delete;
};
