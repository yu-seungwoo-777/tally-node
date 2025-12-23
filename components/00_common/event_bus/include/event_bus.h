/**
 * @file event_bus.h
 * @brief 이벤트 버스 - 레이어 간 결합도 제거를 위한 비동기 이벤트 시스템
 *
 * 레이어 아키텍처 준수를 위한 통신 메커니즘:
 * - 상위 레이어는 이벤트를 발행(Publish)만 하고, 누가 구독하는지 모름
 * - 하위 레이어는 이벤트를 구독(Subscribe)하고 반응
 * - 단방향 통신: 01_app → 02 → 03 → 04 → 05
 */

#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 이벤트 타입 정의
 */
typedef enum {
    // 시스템 이벤트 (01_app)
    EVT_SYSTEM_READY = 0,
    EVT_CONFIG_CHANGED,

    // 정보/상태 이벤트
    EVT_INFO_UPDATED,

    // LoRa 이벤트 (03_service)
    EVT_LORA_STATUS_CHANGED,
    EVT_LORA_PACKET_RECEIVED,
    EVT_LORA_PACKET_SENT,

    // 네트워크 이벤트 (03_service)
    EVT_NETWORK_STATUS_CHANGED,
    EVT_NETWORK_CONNECTED,
    EVT_NETWORK_DISCONNECTED,

    // 스위처 이벤트 (03_service)
    EVT_SWITCHER_CONNECTED,
    EVT_SWITCHER_DISCONNECTED,
    EVT_TALLY_STATE_CHANGED,      // 핵심: Tally 상태 변경

    // UI 이벤트 (02_presentation)
    EVT_BUTTON_PRESSED,
    EVT_DISPLAY_UPDATE_REQUEST,

    // LED 이벤트 (02_presentation)
    EVT_LED_STATE_CHANGED,

    // 최대 이벤트 수
   _EVT_MAX
} event_type_t;

/**
 * @brief 이벤트 데이터 구조체
 */
typedef struct {
    event_type_t type;        ///< 이벤트 타입
    const void* data;         ///< 이벤트 데이터 (상수 포인터 - 수명 주의)
    size_t data_size;         ///< 데이터 크기
    uint32_t timestamp;       ///< 타임스탬프 (ms)
} event_data_t;

/**
 * @brief 이벤트 콜백 함수 타입
 *
 * @param event 이벤트 데이터
 * @return esp_err_t ESP_OK 또는 에러 코드
 */
typedef esp_err_t (*event_callback_t)(const event_data_t* event);

/**
 * @brief 이벤트 버스 초기화
 *
 * @return esp_err_t ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t event_bus_init(void);

/**
 * @brief 이벤트 발행 (Publish)
 *
 * @param type 이벤트 타입
 * @param data 이벤트 데이터 (NULL 가능)
 * @param data_size 데이터 크기
 * @return esp_err_t ESP_OK 성공
 *
 * @note data 포인터는 발행 직후에 해제되어도 됨 (내부 복사)
 */
esp_err_t event_bus_publish(event_type_t type, const void* data, size_t data_size);

/**
 * @brief 이벤트 구독 (Subscribe)
 *
 * @param type 이벤트 타입
 * @param callback 콜백 함수
 * @return esp_err_t ESP_OK 성공
 *
 * @note 콜백은 컨텍스트가 보장됨 (FreeRTOS task 내부)
 */
esp_err_t event_bus_subscribe(event_type_t type, event_callback_t callback);

/**
 * @brief 이벤트 구독 취소
 *
 * @param type 이벤트 타입
 * @param callback 콜백 함수
 * @return esp_err_t ESP_OK 성공
 */
esp_err_t event_bus_unsubscribe(event_type_t type, event_callback_t callback);

/**
 * @brief 이벤트 타입 이름 가져오기 (디버깅용)
 *
 * @param type 이벤트 타입
 * @return const char* 이벤트 이름 문자열
 */
const char* event_type_to_string(event_type_t type);

#ifdef __cplusplus
}
#endif

#endif // EVENT_BUS_H
