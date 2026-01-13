/**
 * @file api_devices.h
 * @brief API Devices 핸들러 (TX 전용)
 */

#ifndef TALLY_API_DEVICES_H
#define TALLY_API_DEVICES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief GET /api/devices - 디바이스 리스트 반환 (TX 전용)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_devices_handler(httpd_req_t* req);

/**
 * @brief DELETE /api/devices - 디바이스 삭제 (TX 전용)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_delete_device_handler(httpd_req_t* req);

/**
 * @brief POST /api/device/brightness - 디바이스 밝기 설정 (LoRa 전송)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_device_brightness_handler(httpd_req_t* req);

/**
 * @brief POST /api/device/camera-id - 디바이스 카메라 ID 설정 (LoRa 전송)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_device_camera_id_handler(httpd_req_t* req);

#ifdef DEVICE_MODE_TX

/**
 * @brief POST /api/brightness/broadcast - 일괄 밝기 제어 (TX → all RX Broadcast)
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_brightness_broadcast_handler(httpd_req_t* req);

/**
 * @brief POST /api/device/ping - 디바이스 PING 핸들러
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_device_ping_handler(httpd_req_t* req);

/**
 * @brief POST /api/device/stop - 디바이스 STOP 핸들러
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_device_stop_handler(httpd_req_t* req);

/**
 * @brief POST /api/device/reboot - 디바이스 REBOOT 핸들러
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_device_reboot_handler(httpd_req_t* req);

/**
 * @brief POST /api/device/status-request - 상태 요청 브로드캐스트 핸들러
 * @param req HTTP 요청
 * @return ESP_OK 성공
 */
esp_err_t api_status_request_handler(httpd_req_t* req);

#endif // DEVICE_MODE_TX

#ifdef __cplusplus
}
#endif

#endif // TALLY_API_DEVICES_H
