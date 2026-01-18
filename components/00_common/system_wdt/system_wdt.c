/**
 * @file system_wdt.c
 * @brief 시스템 워치독 타이머 (TWDT) 관리자 구현
 */

#include "system_wdt.h"
#include "t_log.h"
#include <string.h>

// =============================================================================
// 매크로
// =============================================================================

#define TAG "00_WDT"

// 최대 등록 가능한 태스크 수
#define MAX_WDT_TASKS     16

// =============================================================================
// 정적 변수
// =============================================================================

// WDT 초기화 상태
static bool s_initialized = false;
static uint32_t s_init_magic = 0;

// WDT 설정
static esp_task_wdt_config_t s_wdt_config = {
    .timeout_ms = SYSTEM_WDT_TIMEOUT_MS_DEFAULT,
    .idle_core_mask = 0,       // 모든 코어 감시
    .trigger_panic = true      // 패닉 모드 (크리티컬 태스크 재시작)
};

// 등록된 태스크 정보
static system_wdt_task_t s_tasks[MAX_WDT_TASKS];
static uint32_t s_task_count = 0;

// 통계
static system_wdt_stats_t s_stats = {0};

// 타임아웃 콜백
static system_wdt_callback_t s_timeout_callback = NULL;

// 뮤텍스 (태스크 등록/제거 보호)
static SemaphoreHandle_t s_mutex = NULL;

// =============================================================================
// 내부 함수
// =============================================================================

/**
 * @brief 태스크 핸들로 등록된 태스크 인덱스 찾기
 */
static int find_task_index(TaskHandle_t handle) {
    if (handle == NULL) {
        handle = xTaskGetCurrentTaskHandle();
    }

    for (int i = 0; i < MAX_WDT_TASKS; i++) {
        if (s_tasks[i].registered && s_tasks[i].handle == handle) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 빈 태스크 슬롯 찾기
 */
static int find_free_slot(void) {
    for (int i = 0; i < MAX_WDT_TASKS; i++) {
        if (!s_tasks[i].registered) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 그룹 이름 문자열 반환
 */
static const char* group_name_to_string(system_wdt_group_t group) {
    switch (group) {
        case SYSTEM_WDT_GROUP_TX:     return "TX";
        case SYSTEM_WDT_GROUP_RX:     return "RX";
        case SYSTEM_WDT_GROUP_COMMON:
        default:                      return "COMMON";
    }
}

/**
 * @brief 기본 타임아웃 콜백 (로그만 남김)
 */
static void default_timeout_callback(void* arg) {
    TaskHandle_t hung_task = (TaskHandle_t)arg;

    T_LOGE(TAG, "timeout:task=%p", hung_task);

    // 등록된 태스크 중 응답 없는 태스크 찾기
    for (int i = 0; i < MAX_WDT_TASKS; i++) {
        if (s_tasks[i].registered && s_tasks[i].handle == hung_task) {
            T_LOGE(TAG, "timeout:task_name=%s,group=%s,resets=%u",
                   s_tasks[i].name ? s_tasks[i].name : "unknown",
                   group_name_to_string(s_tasks[i].group),
                   s_tasks[i].reset_count);
        }
    }

    // 사용자 콜백 호출
    if (s_timeout_callback) {
        s_timeout_callback(arg);
    }
}

// =============================================================================
// 공개 API 구현
// =============================================================================

esp_err_t system_wdt_init(const esp_task_wdt_config_t* init_config) {
    if (s_initialized) {
        T_LOGW(TAG, "init:already");
        return ESP_OK;
    }

    // 뮤텍스 생성
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            T_LOGE(TAG, "init:fail:mutex");
            return ESP_FAIL;
        }
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // 설정 복사
    if (init_config != NULL) {
        memcpy(&s_wdt_config, init_config, sizeof(esp_task_wdt_config_t));
    }

    // 타임아웃이 0이면 기본값 사용
    if (s_wdt_config.timeout_ms == 0) {
        s_wdt_config.timeout_ms = SYSTEM_WDT_TIMEOUT_MS_DEFAULT;
    }

    // 태스크 배열 초기화
    memset(s_tasks, 0, sizeof(s_tasks));

    // 통계 초기화
    memset(&s_stats, 0, sizeof(s_stats));

    // TWDT 초기화
    esp_err_t ret = esp_task_wdt_init(&s_wdt_config);
    if (ret != ESP_OK) {
        // 이미 초기화된 경우 (ESP-IDF에서 TWDT가 이미 init됨)
        if (ret == ESP_ERR_INVALID_STATE) {
            T_LOGW(TAG, "init:already_initialized_by_idf");
            ret = ESP_OK; // 계속 진행
        } else {
            T_LOGE(TAG, "init:fail:wdt:0x%x", ret);
            xSemaphoreGive(s_mutex);
            return ESP_FAIL;
        }
    }

    // 기본 콜백 등록 (panic이 아닐 때만)
    if (!s_wdt_config.trigger_panic) {
        // esp_task_wdt_init에서 콜백 등록은 ESP-IDF 버전에 따라 다름
        // 여기서는 사용자가 등록하도록 함
    }

    s_initialized = true;
    s_init_magic = SYSTEM_WDT_INIT_MAGIC;
    s_stats.init_count++;

    T_LOGI(TAG, "init:ok:timeout=%u,panic=%d",
           s_wdt_config.timeout_ms, s_wdt_config.trigger_panic);

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t system_wdt_deinit(void) {
    if (!s_initialized) {
        return ESP_OK;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // 모든 태스크 제거
    for (int i = 0; i < MAX_WDT_TASKS; i++) {
        if (s_tasks[i].registered) {
            esp_task_wdt_delete(s_tasks[i].handle);
            s_tasks[i].registered = false;
        }
    }
    s_task_count = 0;

    // TWDT 정리
    esp_task_wdt_deinit();

    // 그룹별 통계 초기화
    s_stats.tx_tasks = 0;
    s_stats.rx_tasks = 0;
    s_stats.common_tasks = 0;

    s_initialized = false;
    s_init_magic = 0;

    T_LOGI(TAG, "deinit:ok");

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t system_wdt_register_task(const char* task_name) {
    // 기본 등록은 COMMON 그룹 사용
    return system_wdt_register_task_ex(task_name, SYSTEM_WDT_GROUP_COMMON);
}

esp_err_t system_wdt_register_task_ex(const char* task_name, system_wdt_group_t group) {
    if (!s_initialized) {
        T_LOGE(TAG, "register:not_init");
        return ESP_ERR_INVALID_STATE;
    }

    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // 이미 등록되었는지 확인
    int idx = find_task_index(current_task);
    if (idx >= 0) {
        T_LOGW(TAG, "register:already:%s[%s]",
               task_name ? task_name : "unknown",
               group_name_to_string(group));
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }

    // 빈 슬롯 찾기
    idx = find_free_slot();
    if (idx < 0) {
        T_LOGE(TAG, "register:full");
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    // WDT에 태스크 추가
    esp_err_t ret = esp_task_wdt_add(current_task);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "register:fail:add:0x%x", ret);
        xSemaphoreGive(s_mutex);
        return ret;
    }

    // 태스크 정보 저장
    s_tasks[idx].handle = current_task;
    s_tasks[idx].name = task_name;
    s_tasks[idx].reset_count = 0;
    s_tasks[idx].registered = true;
    s_tasks[idx].group = group;
    s_task_count++;
    s_stats.total_tasks = s_task_count;

    // 그룹별 카운터 업데이트
    switch (group) {
        case SYSTEM_WDT_GROUP_TX:
            s_stats.tx_tasks++;
            break;
        case SYSTEM_WDT_GROUP_RX:
            s_stats.rx_tasks++;
            break;
        case SYSTEM_WDT_GROUP_COMMON:
        default:
            s_stats.common_tasks++;
            break;
    }

    T_LOGI(TAG, "register:ok:%s[%s],total=%u",
           task_name ? task_name : "unknown",
           group_name_to_string(group),
           s_task_count);

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t system_wdt_unregister_task(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    int idx = find_task_index(current_task);
    if (idx < 0) {
        T_LOGW(TAG, "unregister:not_found");
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_ARG;
    }

    // WDT에서 태스크 제거
    esp_err_t ret = esp_task_wdt_delete(current_task);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "unregister:fail:delete:0x%x", ret);
    }

    const char* name = s_tasks[idx].name;
    system_wdt_group_t group = s_tasks[idx].group;

    s_tasks[idx].registered = false;
    s_task_count--;
    s_stats.total_tasks = s_task_count;

    // 그룹별 카운터 업데이트
    switch (group) {
        case SYSTEM_WDT_GROUP_TX:
            if (s_stats.tx_tasks > 0) s_stats.tx_tasks--;
            break;
        case SYSTEM_WDT_GROUP_RX:
            if (s_stats.rx_tasks > 0) s_stats.rx_tasks--;
            break;
        case SYSTEM_WDT_GROUP_COMMON:
        default:
            if (s_stats.common_tasks > 0) s_stats.common_tasks--;
            break;
    }

    T_LOGI(TAG, "unregister:ok:%s[%s],remaining=%u",
           name ? name : "unknown",
           group_name_to_string(group),
           s_task_count);

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t system_wdt_reset(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    // 뮤텍스 없이 빠르게 처리 (성능 중요)
    int idx = find_task_index(current_task);
    if (idx < 0) {
        // 등록되지 않은 태스크가 리셋 시도 시 무시 (경계 조건)
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_task_wdt_reset();
    if (ret == ESP_OK) {
        s_tasks[idx].reset_count++;
        s_stats.total_resets++;
    }

    return ret;
}

esp_err_t system_wdt_set_timeout_callback(system_wdt_callback_t callback) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_timeout_callback = callback;
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t system_wdt_get_stats(system_wdt_stats_t* stats) {
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(stats, &s_stats, sizeof(system_wdt_stats_t));
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

bool system_wdt_is_initialized(void) {
    return s_initialized && (s_init_magic == SYSTEM_WDT_INIT_MAGIC);
}

bool system_wdt_is_task_registered(TaskHandle_t handle) {
    if (!s_initialized) {
        return false;
    }

    if (handle == NULL) {
        handle = xTaskGetCurrentTaskHandle();
    }

    return find_task_index(handle) >= 0;
}

uint32_t system_wdt_get_timeout_ms(void) {
    if (!s_initialized) {
        return 0;
    }
    return s_wdt_config.timeout_ms;
}
