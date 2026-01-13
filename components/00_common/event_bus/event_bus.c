/**
 * @file event_bus.c
 * @brief 이벤트 버스 구현
 */

#include "event_bus.h"
#include "t_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

static const char* TAG = "00_EventBus";

// 최대 대기열 크기
#define EVENT_QUEUE_SIZE 32
#define MAX_SUBSCRIBERS_PER_EVENT 8

// 구독자 정보
typedef struct {
    event_callback_t callback;
    bool active;
} subscriber_t;

// 이벤트 버스 상태
typedef struct {
    QueueHandle_t queue;
    SemaphoreHandle_t mutex;
    TaskHandle_t handler_task;
    subscriber_t subscribers[_EVT_MAX][MAX_SUBSCRIBERS_PER_EVENT];
    bool initialized;
} event_bus_t;

static event_bus_t g_event_bus = {0};

/**
 * @brief 이벤트 타입 이름 (디버깅용)
 */
static const char* EVENT_NAMES[] = {
    "EVT_SYSTEM_READY",
    "EVT_CONFIG_CHANGED",
    "EVT_CONFIG_DATA_CHANGED",
    "EVT_CONFIG_DATA_REQUEST",
    "EVT_BRIGHTNESS_CHANGED",
    "EVT_CAMERA_ID_CHANGED",
    "EVT_RF_CHANGED",
    "EVT_RF_SAVED",
    "EVT_STOP_CHANGED",
    "EVT_BUTTON_SINGLE_CLICK",
    "EVT_BUTTON_LONG_PRESS",
    "EVT_BUTTON_LONG_RELEASE",
    "EVT_INFO_UPDATED",
    "EVT_LORA_STATUS_CHANGED",
    "EVT_LORA_RSSI_CHANGED",
    "EVT_LORA_TX_COMMAND",
    "EVT_LORA_RX_RESPONSE",
    "EVT_LORA_PACKET_RECEIVED",
    "EVT_LORA_PACKET_SENT",
    "EVT_LORA_SEND_REQUEST",
    "EVT_LORA_SCAN_START",
    "EVT_LORA_SCAN_PROGRESS",
    "EVT_LORA_SCAN_COMPLETE",
    "EVT_LORA_SCAN_STOP",
    "EVT_NETWORK_STATUS_CHANGED",
    "EVT_NETWORK_CONNECTED",
    "EVT_NETWORK_DISCONNECTED",
    "EVT_NETWORK_RESTART_REQUEST",
    "EVT_NETWORK_RESTARTED",
    "EVT_SWITCHER_CONNECTED",
    "EVT_SWITCHER_DISCONNECTED",
    "EVT_SWITCHER_STATUS_CHANGED",
    "EVT_TALLY_STATE_CHANGED",
    "EVT_DISPLAY_UPDATE_REQUEST",
    "EVT_LED_STATE_CHANGED",
    "EVT_DEVICE_REGISTER",
    "EVT_DEVICE_UNREGISTER",
    "EVT_DEVICE_LIST_CHANGED",
    "EVT_DEVICE_BRIGHTNESS_REQUEST",
    "EVT_DEVICE_CAMERA_ID_REQUEST",
    "EVT_DEVICE_PING_REQUEST",
    "EVT_DEVICE_STOP_REQUEST",
    "EVT_DEVICE_REBOOT_REQUEST",
    "EVT_STATUS_REQUEST",
    "EVT_DEVICE_CAM_MAP_RECEIVE",
    "EVT_DEVICE_CAM_MAP_LOAD",
    "EVT_LICENSE_STATE_CHANGED",
    "EVT_LICENSE_VALIDATE",
    "EVT_LICENSE_DATA_REQUEST",
    "EVT_LICENSE_DATA_SAVE",
};

const char* event_type_to_string(event_type_t type) {
    if (type >= 0 && type < sizeof(EVENT_NAMES) / sizeof(EVENT_NAMES[0])) {
        return EVENT_NAMES[type];
    }
    return "UNKNOWN";
}

/**
 * @brief 이벤트 처리 태스크
 */
static void event_handler_task(void* arg) {
    event_data_t event;

    while (1) {
        if (xQueueReceive(g_event_bus.queue, &event, portMAX_DELAY) == pdTRUE) {
            // 구독자들에게 이벤트 전달
            if (event.type < _EVT_MAX) {
                xSemaphoreTake(g_event_bus.mutex, portMAX_DELAY);

                for (int i = 0; i < MAX_SUBSCRIBERS_PER_EVENT; i++) {
                    subscriber_t* sub = &g_event_bus.subscribers[event.type][i];
                    if (sub->active && sub->callback != NULL) {
                        xSemaphoreGive(g_event_bus.mutex);
                        // 콜백 호출 (mutex 해제 후)
                        sub->callback(&event);
                        xSemaphoreTake(g_event_bus.mutex, portMAX_DELAY);
                    }
                }

                xSemaphoreGive(g_event_bus.mutex);
            }
        }
    }
}

esp_err_t event_bus_init(void) {
    if (g_event_bus.initialized) {
        return ESP_OK;
    }

    // 큐 생성
    g_event_bus.queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(event_data_t));
    if (g_event_bus.queue == NULL) {
        T_LOGE(TAG, "Failed to create event queue");
        return ESP_FAIL;
    }

    // 뮤텍스 생성
    g_event_bus.mutex = xSemaphoreCreateMutex();
    if (g_event_bus.mutex == NULL) {
        T_LOGE(TAG, "Failed to create mutex");
        vQueueDelete(g_event_bus.queue);
        return ESP_FAIL;
    }

    // 구독자 테이블 초기화
    memset(g_event_bus.subscribers, 0, sizeof(g_event_bus.subscribers));

    // 이벤트 처리 태스크 생성 (스택 크기 증가: 4096 → 12288)
    BaseType_t ret = xTaskCreate(
        event_handler_task,
        "event_bus",
        12288,  // 12KB (HTTPS 응답 처리 등을 위한 충분한 공간)
        NULL,
        5,  // 우선순위 (중간)
        &g_event_bus.handler_task
    );

    if (ret != pdPASS) {
        T_LOGE(TAG, "Failed to create event handler task");
        vSemaphoreDelete(g_event_bus.mutex);
        vQueueDelete(g_event_bus.queue);
        return ESP_FAIL;
    }

    g_event_bus.initialized = true;
    return ESP_OK;
}

esp_err_t event_bus_publish(event_type_t type, const void* data, size_t data_size) {
    if (!g_event_bus.initialized) {
        T_LOGE(TAG, "Event bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (type >= _EVT_MAX) {
        T_LOGE(TAG, "Invalid event type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    // 데이터 크기 검증
    if (data_size > EVENT_DATA_BUFFER_SIZE) {
        T_LOGE(TAG, "Data size %zu exceeds buffer size %d", data_size, EVENT_DATA_BUFFER_SIZE);
        return ESP_ERR_INVALID_ARG;
    }

    // 큐 유효성 검사
    if (g_event_bus.queue == NULL) {
        T_LOGE(TAG, "Event queue is NULL!");
        return ESP_ERR_INVALID_STATE;
    }

    // 스택 할당 (구조체 크기 약 2064바이트)
    // 필드 순서: type, data[], data_size, timestamp
    event_data_t event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.data_size = data_size;
    event.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 데이터 복사 (발행자의 데이터를 내부 버퍼로)
    if (data != NULL && data_size > 0) {
        memcpy(event.data, data, data_size);
    }

    // 큐에 전송 (대기 없음)
    if (xQueueSend(g_event_bus.queue, &event, 0) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t event_bus_subscribe(event_type_t type, event_callback_t callback) {
    if (!g_event_bus.initialized) {
        T_LOGE(TAG, "Event bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (type >= _EVT_MAX || callback == NULL) {
        T_LOGE(TAG, "Invalid arguments for subscribe");
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_event_bus.mutex, portMAX_DELAY);

    // 빈 슬롯 찾기
    bool found = false;
    for (int i = 0; i < MAX_SUBSCRIBERS_PER_EVENT; i++) {
        if (!g_event_bus.subscribers[type][i].active) {
            g_event_bus.subscribers[type][i].callback = callback;
            g_event_bus.subscribers[type][i].active = true;
            found = true;
            break;
        }
    }

    xSemaphoreGive(g_event_bus.mutex);

    if (!found) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t event_bus_unsubscribe(event_type_t type, event_callback_t callback) {
    if (!g_event_bus.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (type >= _EVT_MAX || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_event_bus.mutex, portMAX_DELAY);

    for (int i = 0; i < MAX_SUBSCRIBERS_PER_EVENT; i++) {
        if (g_event_bus.subscribers[type][i].active &&
            g_event_bus.subscribers[type][i].callback == callback) {
            g_event_bus.subscribers[type][i].active = false;
            g_event_bus.subscribers[type][i].callback = NULL;
            break;
        }
    }

    xSemaphoreGive(g_event_bus.mutex);
    return ESP_OK;
}
