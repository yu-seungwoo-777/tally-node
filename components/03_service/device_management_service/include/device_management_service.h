/**
 * @file device_management_service.h
 * @brief 디바이스 관리 서비스 (TX/RX 통합)
 *
 * TX: 명령 송신 + 디바이스 목록 관리
 * RX: 명령 수신 및 실행
 */

#ifndef DEVICE_MANAGEMENT_SERVICE_H
#define DEVICE_MANAGEMENT_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LORA_DEVICE_ID_LEN 4
#define DEVICE_MGMT_MAX_DEVICES 20
#define DEVICE_MGMT_MAX_REGISTERED 20

// ============================================================================
// 공통 정의
// ============================================================================

/**
 * @brief RX 디바이스 상태 (TX용)
 */
typedef struct {
    uint8_t device_id[LORA_DEVICE_ID_LEN];  ///< 디바이스 ID
    int16_t last_rssi;                       ///< 마지막 RSSI
    float last_snr;                          ///< 마지막 SNR
    uint8_t battery;                         ///< 배터리 %
    uint8_t camera_id;                       ///< 카메라 ID
    uint32_t uptime;                         ///< 업타임 (초)
    uint8_t brightness;                      ///< 밝기 0-100
    bool is_stopped;                         ///< 기능 정지 상태
    bool is_online;                          ///< 온라인 상태
    uint32_t last_seen;                      ///< 마지막 수신 시간 (tick)
    uint16_t ping_ms;                        ///< 지연시간 (ms)
    float frequency;                         ///< 현재 주파수 (MHz)
    uint8_t sync_word;                       ///< 현재 sync word
} device_mgmt_device_t;

/**
 * @brief RX 상태 콜백 (RX용)
 */
typedef struct {
    uint8_t battery;       ///< 배터리 0-100%
    uint8_t camera_id;     ///< 카메라 ID
    uint32_t uptime;       ///< 업타임 (초)
    uint8_t brightness;    ///< 밝기 0-100
    bool is_stopped;      ///< 기능 정지 상태
} device_mgmt_status_t;

typedef void (*device_mgmt_status_callback_t)(device_mgmt_status_t* status);
typedef void (*device_mgmt_event_callback_t)(void);

// ============================================================================
// 공개 API
// ============================================================================

/**
 * @brief 디바이스 관리 서비스 초기화
 * @param status_cb 상태 요청 시 호출할 콜백 (RX 전용, NULL 가능)
 */
esp_err_t device_management_service_init(device_mgmt_status_callback_t status_cb);

/**
 * @brief 디바이스 관리 서비스 시작
 */
esp_err_t device_management_service_start(void);

/**
 * @brief 디바이스 관리 서비스 정지
 */
void device_management_service_stop(void);

// ============================================================================
// TX 전용 API: 명령 송신
// ============================================================================

#ifdef DEVICE_MODE_TX

/**
 * @brief 상태 요청 (Broadcast)
 * 모든 RX 디바이스가 응답
 */
esp_err_t device_mgmt_send_status_req(void);

/**
 * @brief 밝기 설정 (Unicast)
 * @param device_id 타겟 RX device_id (4바이트)
 * @param brightness 밝기 0-100
 */
esp_err_t device_mgmt_set_brightness(const uint8_t* device_id, uint8_t brightness);

/**
 * @brief 카메라 ID 설정 (Unicast)
 * @param device_id 타겟 RX device_id (4바이트)
 * @param camera_id 카메라 ID
 */
esp_err_t device_mgmt_set_camera_id(const uint8_t* device_id, uint8_t camera_id);

/**
 * @brief 주파수+SyncWord 설정 (Unicast)
 * @param device_id 타겟 RX device_id (4바이트)
 * @param frequency 주파수 (MHz)
 * @param sync_word sync word
 */
esp_err_t device_mgmt_set_rf(const uint8_t* device_id, float frequency, uint8_t sync_word);

/**
 * @brief 기능 정지 명령 전송 (Uni/Broadcast)
 * @param device_id 타겟 RX device_id (4바이트), nullptr이면 broadcast
 */
esp_err_t device_mgmt_send_stop(const uint8_t* device_id);

/**
 * @brief 재부팅 (Unicast)
 * @param device_id 타겟 RX device_id (4바이트)
 */
esp_err_t device_mgmt_reboot(const uint8_t* device_id);

/**
 * @brief 지연시간 테스트 (Unicast)
 * @param device_id 타겟 RX device_id (4바이트)
 * @param timestamp 송신 시간 (ms)
 */
esp_err_t device_mgmt_ping(const uint8_t* device_id, uint32_t timestamp);

// ============================================================================
// TX 전용 API: 디바이스 관리
// ============================================================================

/**
 * @brief 디바이스 수 가져오기
 */
uint8_t device_mgmt_get_device_count(void);

/**
 * @brief 디바이스 목록 가져오기
 * @param devices 출력 버퍼 (최소 DEVICE_MGMT_MAX_DEVICES * sizeof(device_mgmt_device_t))
 * @return 실제 디바이스 수
 */
uint8_t device_mgmt_get_devices(device_mgmt_device_t* devices);

/**
 * @brief 디바이스 찾기
 * @param device_id 찾을 디바이스 ID
 * @return 디바이스 인덱스 (0~DEVICE_MGMT_MAX_DEVICES-1), 못찾으면 -1
 */
int device_mgmt_find_device(const uint8_t* device_id);

/**
 * @brief 디바이스 가져오기
 * @param index 디바이스 인덱스
 * @param device 출력 버퍼
 * @return 성공 여부
 */
bool device_mgmt_get_device_at(uint8_t index, device_mgmt_device_t* device);

/**
 * @brief 오프라인 디바이스 정리 (일정 시간未见인 디바이스 제거)
 * @param timeout_ms 타임아웃 (ms)
 */
void device_mgmt_cleanup_offline(uint32_t timeout_ms);

/**
 * @brief 상태 변경 콜백 등록
 */
void device_mgmt_set_event_callback(device_mgmt_event_callback_t callback);

// ============================================================================
// TX 전용 API: 등록된 디바이스 관리 (NVS 저장)
// ============================================================================

/**
 * @brief 디바이스 등록 (NVS에 저장)
 * @param device_id 등록할 디바이스 ID
 * @return 성공 시 ESP_OK, 공간 부족 시 ESP_ERR_NO_MEM
 */
esp_err_t device_mgmt_register_device(const uint8_t* device_id);

/**
 * @brief 디바이스 등록 해제 (NVS에서 삭제)
 * @param device_id 삭제할 디바이스 ID
 * @return 성공 시 ESP_OK, 찾지 못하면 ESP_ERR_NOT_FOUND
 */
esp_err_t device_mgmt_unregister_device(const uint8_t* device_id);

/**
 * @brief 디바이스 등록 여부 확인
 * @param device_id 확인할 디바이스 ID
 * @return 등록되어 있으면 true
 */
bool device_mgmt_is_registered(const uint8_t* device_id);

/**
 * @brief 등록된 디바이스 수 가져오기
 */
uint8_t device_mgmt_get_registered_count(void);

/**
 * @brief 등록된 디바이스 목록 가져오기
 * @param device_ids 출력 버퍼 (최소 DEVICE_MGMT_MAX_REGISTERED * LORA_DEVICE_ID_LEN)
 * @return 실제 등록된 디바이스 수
 */
uint8_t device_mgmt_get_registered_devices(uint8_t* device_ids);

/**
 * @brief NVS에서 등록된 디바이스 로드
 * @return 성공 시 ESP_OK
 */
esp_err_t device_mgmt_load_registered(void);

/**
 * @brief NVS에 등록된 디바이스 저장
 * @return 성공 시 ESP_OK
 */
esp_err_t device_mgmt_save_registered(void);

/**
 * @brief 등록된 모든 디바이스 초기화
 */
void device_mgmt_clear_registered(void);

#endif // DEVICE_MODE_TX

// ============================================================================
// RX 전용 API: Device ID 관리
// ============================================================================

#ifdef DEVICE_MODE_RX

/**
 * @brief Device ID 설정 (MAC 주소 뒤 4자리)
 * @param device_id Device ID (4바이트)
 */
void device_mgmt_set_device_id(const uint8_t* device_id);

/**
 * @brief Device ID 가져오기
 * @return Device ID 포인터 (4바이트)
 */
const uint8_t* device_mgmt_get_device_id(void);

#endif // DEVICE_MODE_RX

#ifdef __cplusplus
}
#endif

#endif // DEVICE_MANAGEMENT_SERVICE_H
