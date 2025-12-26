/**
 * @file rx_manager.h
 * @brief RX 디바이스 관리 서비스 (TX)
 *
 * RX 디바이스 목록 관리, 상태 업데이트, UI 제공
 */

#ifndef RX_MANAGER_H
#define RX_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RX_MANAGER_MAX_DEVICES 20
#define RX_MANAGER_MAX_REGISTERED 20  // NVS에 저장할 최대 등록 디바이스 수
#define LORA_DEVICE_ID_LEN 4

/**
 * @brief RX 디바이스 상태
 */
typedef struct {
    uint8_t device_id[LORA_DEVICE_ID_LEN];  // 디바이스 ID
    int16_t last_rssi;                       // 마지막 RSSI
    float last_snr;                          // 마지막 SNR
    uint8_t battery;                         // 배터리 %
    uint8_t camera_id;                       // 카메라 ID
    uint32_t uptime;                         // 업타임 (초)
    uint8_t brightness;                      // 밝기 0-100
    bool is_stopped;                         // 기능 정지 상태
    bool is_online;                          // 온라인 상태
    uint32_t last_seen;                      // 마지막 수신 시간 (tick)
    uint16_t ping_ms;                        // 지연시간 (ms)
} rx_device_t;

/**
 * @brief 상태 변경 콜백
 */
typedef void (*rx_manager_event_callback_t)(void);

/**
 * @brief RX 매니저 초기화
 */
esp_err_t rx_manager_init(void);

/**
 * @brief RX 매니저 시작
 */
esp_err_t rx_manager_start(void);

/**
 * @brief RX 매니저 정지
 */
void rx_manager_stop(void);

/**
 * @brief LoRa 패킷 수신 처리
 * @param data 수신 데이터
 * @param length 데이터 길이
 */
void rx_manager_process_packet(const uint8_t* data, size_t length);

// ========== 디바이스 관리 ==========

/**
 * @brief 디바이스 수 가져오기
 */
uint8_t rx_manager_get_device_count(void);

/**
 * @brief 디바이스 목록 가져오기
 * @param devices 출력 버퍼 (최소 RX_MANAGER_MAX_DEVICES * sizeof(rx_device_t))
 * @return 실제 디바이스 수
 */
uint8_t rx_manager_get_devices(rx_device_t* devices);

/**
 * @brief 디바이스 찾기
 * @param device_id 찾을 디바이스 ID
 * @return 디바이스 인덱스 (0~RX_MANAGER_MAX_DEVICES-1), 못찾으면 -1
 */
int rx_manager_find_device(const uint8_t* device_id);

/**
 * @brief 디바이스 가져오기
 * @param index 디바이스 인덱스
 * @param device 출력 버퍼
 * @return 성공 여부
 */
bool rx_manager_get_device_at(uint8_t index, rx_device_t* device);

/**
 * @brief 오프라인 디바이스 정리 (일정 시간未见인 디바이스 제거)
 * @param timeout_ms 타임아웃 (ms)
 */
void rx_manager_cleanup_offline(uint32_t timeout_ms);

// ========== 등록된 디바이스 관리 (NVS 저장) ==========

/**
 * @brief 디바이스 등록 (NVS에 저장)
 * @param device_id 등록할 디바이스 ID
 * @return 성공 시 ESP_OK, 공간 부족 시 ESP_ERR_NO_MEM
 */
esp_err_t rx_manager_register_device(const uint8_t* device_id);

/**
 * @brief 디바이스 등록 해제 (NVS에서 삭제)
 * @param device_id 삭제할 디바이스 ID
 * @return 성공 시 ESP_OK, 찾지 못하면 ESP_ERR_NOT_FOUND
 */
esp_err_t rx_manager_unregister_device(const uint8_t* device_id);

/**
 * @brief 디바이스 등록 여부 확인
 * @param device_id 확인할 디바이스 ID
 * @return 등록되어 있으면 true
 */
bool rx_manager_is_registered(const uint8_t* device_id);

/**
 * @brief 등록된 디바이스 수 가져오기
 */
uint8_t rx_manager_get_registered_count(void);

/**
 * @brief 등록된 디바이스 목록 가져오기
 * @param device_ids 출력 버퍼 (최소 RX_MANAGER_MAX_REGISTERED * LORA_DEVICE_ID_LEN)
 * @return 실제 등록된 디바이스 수
 */
uint8_t rx_manager_get_registered_devices(uint8_t* device_ids);

/**
 * @brief NVS에서 등록된 디바이스 로드
 * @return 성공 시 ESP_OK
 */
esp_err_t rx_manager_load_registered(void);

/**
 * @brief NVS에 등록된 디바이스 저장
 * @return 성공 시 ESP_OK
 */
esp_err_t rx_manager_save_registered(void);

/**
 * @brief 등록된 모든 디바이스 초기화
 */
void rx_manager_clear_registered(void);

// ========== 이벤트 콜백 ==========

/**
 * @brief 상태 변경 콜백 등록
 */
void rx_manager_set_event_callback(rx_manager_event_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // RX_MANAGER_H
