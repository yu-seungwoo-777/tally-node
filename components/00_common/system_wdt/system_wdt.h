/**
 * @file system_wdt.h
 * @brief 시스템 워치독 타이머 (TWDT) 관리자
 *
 * ESP32 태스크 워치독 타이머(TWDT)를 중앙에서 관리하는 컴포넌트입니다.
 * 여러 태스크를 WDT에 등록하고 주기적으로 리셋하여 태스크 hang을 감지합니다.
 */

#ifndef SYSTEM_WDT_H
#define SYSTEM_WDT_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_task_wdt.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// 상수 정의
// =============================================================================

// 기본 WDT 타임아웃 (5초)
#define SYSTEM_WDT_TIMEOUT_MS_DEFAULT    5000

// WDT 초기화 여부 확인을 위한 핸들 (내부용)
#define SYSTEM_WDT_INIT_MAGIC            0x57445431  // "WDT1"

// =============================================================================
// 태스크 그룹 정의
// =============================================================================

/**
 * @brief WDT 태스크 그룹
 *
 * TX/RX 모드에 따라 태스크를 그룹화하여 관리합니다.
 */
typedef enum {
    SYSTEM_WDT_GROUP_COMMON = 0,  ///< 공통 태스크 (TX/RX 모두 사용)
    SYSTEM_WDT_GROUP_TX = 1,      ///< TX 전용 태스크
    SYSTEM_WDT_GROUP_RX = 2       ///< RX 전용 태스크
} system_wdt_group_t;

// =============================================================================
// 타입 정의
// =============================================================================

/**
 * @brief WDT 등록 태스크 정보
 */
typedef struct {
    TaskHandle_t handle;      // 태스크 핸들
    const char* name;         // 태스크 이름 (디버그용)
    uint32_t reset_count;     // 리셋 횟수 (통계)
    bool registered;          // 등록 여부
    system_wdt_group_t group; // 태스크 그룹
} system_wdt_task_t;

/**
 * @brief 시스템 WDT 통계 정보
 */
typedef struct {
    uint32_t total_tasks;           // 등록된 태스크 수
    uint32_t total_resets;          // 총 리셋 횟수
    uint32_t init_count;            // 초기화 횟수
    uint32_t tx_tasks;              // TX 그룹 태스크 수
    uint32_t rx_tasks;              // RX 그룹 태스크 수
    uint32_t common_tasks;          // 공통 그룹 태스크 수
} system_wdt_stats_t;

/**
 * @brief WDT 타임아웃 콜백 함수 타입
 */
typedef void (*system_wdt_callback_t)(void* arg);

// =============================================================================
// 공개 API
// =============================================================================

/**
 * @brief 시스템 WDT를 초기화합니다
 *
 * 태스크 워치독 타이머(TWDT)를 초기화합니다. init_config.timeout_ms가 0이면
 * 기본값(SYSTEM_WDT_TIMEOUT_MS_DEFAULT)을 사용합니다.
 *
 * @param init_config WDT 설정 (nullptr 가능)
 * @return ESP_OK 성공, ESP_FAIL 초기화 실패
 * @note init_config.trigger_panic이 true면 타임아웃 시 시스템 패닉 발생
 */
esp_err_t system_wdt_init(const esp_task_wdt_config_t* init_config);

/**
 * @brief 시스템 WDT를 정리합니다
 *
 * 모든 태스크를 WDT에서 제거하고 TWDT를 중지합니다.
 *
 * @return ESP_OK 성공
 */
esp_err_t system_wdt_deinit(void);

/**
 * @brief 현재 태스크를 WDT에 등록합니다
 *
 * 호출한 태스크를 WDT에 등록합니다. 등록된 태스크은 주기적으로
 * system_wdt_reset()을 호출해야 합니다.
 *
 * @param task_name 태스크 이름 (nullptr 가능)
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE WDT 초기화 안 됨, ESP_ERR_NO_MEM 태스크 초과
 */
esp_err_t system_wdt_register_task(const char* task_name);

/**
 * @brief 현재 태스크를 WDT에 그룹 지정하여 등록합니다
 *
 * 호출한 태스크를 WDT에 특정 그룹(TX/RX/COMMON)으로 등록합니다.
 * TX/RX 모드에 따라 태스크를 그룹화하여 관리할 수 있습니다.
 *
 * @param task_name 태스크 이름 (nullptr 가능)
 * @param group 태스크 그룹 (SYSTEM_WDT_GROUP_TX/RX/COMMON)
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE WDT 초기화 안 됨, ESP_ERR_NO_MEM 태스크 초과
 */
esp_err_t system_wdt_register_task_ex(const char* task_name, system_wdt_group_t group);

/**
 * @brief 현재 태스크를 WDT에서 제거합니다
 *
 * 호출한 태스크를 WDT에서 제거합니다.
 *
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 태스크가 등록되지 않음
 */
esp_err_t system_wdt_unregister_task(void);

/**
 * @brief 현재 태스크의 WDT를 리셋합니다
 *
 * 호출한 태스크의 워치독 타이머를 리셋(피드)합니다.
 * 등록된 태스크은 주기적으로 호출해야 합니다.
 *
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 태스크가 등록되지 않음
 */
esp_err_t system_wdt_reset(void);

/**
 * @brief 타임아웃 시 콜백을 등록합니다
 *
 * WDT 타임아웃 발생 시 호출될 콜백을 등록합니다.
 * 콜백에서 시스템 복구 로직을 구현할 수 있습니다.
 *
 * @param callback 타임아웃 콜백 함수
 * @return ESP_OK 성공
 */
esp_err_t system_wdt_set_timeout_callback(system_wdt_callback_t callback);

/**
 * @brief 시스템 WDT 통계를 가져옵니다
 *
 * @param stats 통계 정보를 저장할 구조체 포인터
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG stats가 nullptr
 */
esp_err_t system_wdt_get_stats(system_wdt_stats_t* stats);

/**
 * @brief 시스템 WDT가 초기화되었는지 확인합니다
 *
 * @return true 초기화됨, false 초기화되지 않음
 */
bool system_wdt_is_initialized(void);

/**
 * @brief 지정된 태스크가 WDT에 등록되었는지 확인합니다
 *
 * @param handle 확인할 태스크 핸들 (nullptr인 경우 현재 태스크)
 * @return true 등록됨, false 등록되지 않음
 */
bool system_wdt_is_task_registered(TaskHandle_t handle);

/**
 * @brief WDT 타임아웃을 가져옵니다 (밀리초)
 *
 * @return 타임아웃 시간 (ms), 초기화되지 않은 경우 0
 */
uint32_t system_wdt_get_timeout_ms(void);

// =============================================================================
// 편의 매크로 (컴파일 타임 디바이스 모드 기반 자동 그룹 선택)
// =============================================================================

/**
 * @brief 현재 디바이스 모드에 맞는 그룹으로 태스크 등록
 *
 * platformio.ini에서 정의된 DEVICE_MODE_TX 또는 DEVICE_MODE_RX 매크로에
 * 따라 자동으로 TX 또는 RX 그룹을 선택합니다. 매크로가 정의되지 않은 경우
 * COMMON 그룹을 사용합니다.
 *
 * 사용 예:
 *   system_wdt_register_task_auto("lora_task");  // TX/RX 자동 선택
 */
#if defined(DEVICE_MODE_TX)
#define SYSTEM_WDT_CURRENT_GROUP  SYSTEM_WDT_GROUP_TX
#elif defined(DEVICE_MODE_RX)
#define SYSTEM_WDT_CURRENT_GROUP  SYSTEM_WDT_GROUP_RX
#else
#define SYSTEM_WDT_CURRENT_GROUP  SYSTEM_WDT_GROUP_COMMON
#endif

/**
 * @brief 현재 디바이스 모드 그룹으로 태스크 등록 편의 매크로
 */
#define system_wdt_register_task_auto(name) \
    system_wdt_register_task_ex((name), SYSTEM_WDT_CURRENT_GROUP)

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_WDT_H
