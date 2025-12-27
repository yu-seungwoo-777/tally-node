/**
 * @file WiFiHal.h
 * @brief WiFi HAL (esp_wifi 하드웨어 제어 캡슐화)
 *
 * 역할: ESP-IDF esp_wifi 드라이버 캡슐화
 * - WiFi 하드웨어 초기화/제어
 * - netif 생성/관리
 * - 이벤트 핸들링
 */

#ifndef WIFI_HAL_H
#define WIFI_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

// WiFi 상태
typedef enum {
    WIFI_HAL_STATE_IDLE = 0,
    WIFI_HAL_STATE_STARTED,
    WIFI_HAL_STATE_STOPPED,
} wifi_hal_state_t;

// 이벤트 콜백 타입
typedef void (*wifi_hal_event_callback_t)(void* arg, esp_event_base_t event_base,
                                           int32_t event_id, void* event_data);

// ============================================================================
// 초기화/정리
// ============================================================================

/**
 * @brief WiFi HAL 초기화
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_init(void);

/**
 * @brief WiFi HAL 정리
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_deinit(void);

// ============================================================================
// netif 생성
// ============================================================================

/**
 * @brief WiFi AP netif 생성
 * @return netif 핸들, 실패 시 NULL
 */
void* wifi_hal_create_ap_netif(void);

/**
 * @brief WiFi STA netif 생성
 * @return netif 핸들, 실패 시 NULL
 */
void* wifi_hal_create_sta_netif(void);

// ============================================================================
// 이벤트 핸들러
// ============================================================================

/**
 * @brief 이벤트 핸들러 등록
 * @param callback 이벤트 콜백 함수
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_register_event_handler(wifi_hal_event_callback_t callback);

// ============================================================================
// WiFi 제어
// ============================================================================

/**
 * @brief WiFi 시작
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_start(void);

/**
 * @brief WiFi 정지
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_stop(void);

/**
 * @brief STA 연결
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_connect(void);

/**
 * @brief STA 연결 해제
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_disconnect(void);

// ============================================================================
// 설정
// ============================================================================

/**
 * @brief WiFi 설정 적용
 * @param iface WiFi 인터페이스 (WIFI_IF_AP 또는 WIFI_IF_STA)
 * @param config WiFi 설정
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_set_config(wifi_interface_t iface, const void* config);

/**
 * @brief WiFi 설정 조회
 * @param iface WiFi 인터페이스 (WIFI_IF_AP 또는 WIFI_IF_STA)
 * @param config WiFi 설정을 받을 구조체 포인터
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_get_config(wifi_interface_t iface, void* config);

// ============================================================================
// 스캔
// ============================================================================

/**
 * @brief WiFi 스캔 시작
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_scan_start(void);

/**
 * @brief WiFi 스캔 결과 조회
 * @param ap_records AP 레코드를 받을 배열
 * @param max_count 배열 최대 크기
 * @param out_count 실제 발견된 AP 개수
 * @return ESP_OK 성공, ESP_ERR_* 실패
 */
esp_err_t wifi_hal_scan_get_results(void* ap_records, uint16_t max_count, uint16_t* out_count);

// ============================================================================
// 상태 조회
// ============================================================================

/**
 * @brief WiFi HAL 상태 조회
 * @return WiFi HAL 상태
 */
wifi_hal_state_t wifi_hal_get_state(void);

/**
 * @brief WiFi 초기화 여부
 * @return true 초기화됨, false 초기화 안됨
 */
bool wifi_hal_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_HAL_H
