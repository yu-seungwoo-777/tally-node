/**
 * @file web_server_events.h
 * @brief Web Server 이벤트 핸들러 모듈
 */

#ifndef TALLY_WEB_SERVER_EVENTS_H
#define TALLY_WEB_SERVER_EVENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "event_bus.h"

// ============================================================================
// 이벤트 핸들러 함수
// ============================================================================

/**
 * @brief 시스템 정보 이벤트 핸들러 (EVT_INFO_UPDATED)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 실패
 */
esp_err_t web_server_on_system_info_event(const event_data_t* event);

/**
 * @brief 스위처 상태 이벤트 핸들러 (EVT_SWITCHER_STATUS_CHANGED)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 실패
 */
esp_err_t web_server_on_switcher_status_event(const event_data_t* event);

/**
 * @brief 네트워크 상태 이벤트 핸들러 (EVT_NETWORK_STATUS_CHANGED)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 실패
 */
esp_err_t web_server_on_network_status_event(const event_data_t* event);

/**
 * @brief 설정 데이터 이벤트 핸들러 (EVT_CONFIG_DATA_CHANGED)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 실패
 */
esp_err_t web_server_on_config_data_event(const event_data_t* event);

/**
 * @brief LoRa 스캔 시작 이벤트 핸들러 (EVT_LORA_SCAN_START)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공
 */
esp_err_t web_server_on_lora_scan_start_event(const event_data_t* event);

/**
 * @brief LoRa 스캔 진행 이벤트 핸들러 (EVT_LORA_SCAN_PROGRESS)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 실패
 */
esp_err_t web_server_on_lora_scan_progress_event(const event_data_t* event);

/**
 * @brief LoRa 스캔 완료 이벤트 핸들러 (EVT_LORA_SCAN_COMPLETE)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 실패
 */
esp_err_t web_server_on_lora_scan_complete_event(const event_data_t* event);

/**
 * @brief 디바이스 리스트 이벤트 핸들러 (EVT_DEVICE_LIST_CHANGED)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 실패
 */
esp_err_t web_server_on_device_list_event(const event_data_t* event);

/**
 * @brief 라이센스 상태 이벤트 핸들러 (EVT_LICENSE_STATE_CHANGED)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 실패
 */
esp_err_t web_server_on_license_state_event(const event_data_t* event);

/**
 * @brief 네트워크 재시작 완료 이벤트 핸들러 (EVT_NETWORK_RESTARTED)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공, ESP_FAIL 실패
 */
esp_err_t web_server_on_network_restarted_event(const event_data_t* event);

/**
 * @brief LED 색상 변경 이벤트 핸들러 (EVT_LED_COLORS_CHANGED)
 * @param event 이벤트 데이터
 * @return ESP_OK 성공
 */
esp_err_t web_server_on_led_colors_event(const event_data_t* event);

#ifdef __cplusplus
}
#endif

#endif // TALLY_WEB_SERVER_EVENTS_H
