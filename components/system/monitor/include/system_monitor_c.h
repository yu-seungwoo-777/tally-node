/**
 * @file system_monitor_c.h
 * @brief SystemMonitor C 인터페이스
 *
 * C 코드에서 SystemMonitor C++ 클래스를 사용하기 위한 인터페이스
 * InfoManager와 중복 기능 제거
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 시스템 하드웨어 상태 정보 (InfoManager에서 관리하는 정보 제외)
 */
struct system_health {
    uint64_t uptime_sec;        // 업타임 (초)
    float temperature_celsius;  // 온도 (°C)
    float voltage;              // 전압 (V)
    float battery_percent;      // 배터리 (%)
    uint32_t free_heap;         // 사용 가능한 힙 메모리
    uint32_t min_free_heap;     // 최소 힙 메모리
};

/**
 * @brief 초기화
 *
 * ADC, 온도 센서 등 하드웨어 초기화
 */
esp_err_t system_monitor_init(void);

/**
 * @brief 종료
 */
void system_monitor_deinit(void);

/**
 * @brief 전체 시스템 상태 조회
 * @return system_health 현재 시스템 상태
 */
struct system_health system_monitor_get_health(void);

/**
 * @brief 모니터링 시작
 * @return 성공 여부
 */
esp_err_t system_monitor_start_monitoring(void);

/**
 * @brief 모니터링 중지
 * @return 성공 여부
 */
esp_err_t system_monitor_stop_monitoring(void);

/**
 * @brief 초기화 여부 확인
 * @return 초기화 완료 여부
 */
bool system_monitor_is_initialized(void);

#ifdef __cplusplus
}
#endif