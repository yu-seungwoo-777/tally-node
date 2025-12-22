/**
 * @file info_manager.h
 * @brief InfoManager C 인터페이스
 *
 * C 코드에서 InfoManager를 사용하기 위한 인터페이스
 */

#pragma once

#include "info/info_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief InfoManager 초기화
 * @return ESP_OK 성공, 그 외 실패
 * @note app_main()에서 다른 컴포넌트보다 먼저 호출
 */
esp_err_t info_manager_init(void);

/**
 * @brief InfoManager 해제
 */
void info_manager_deinit(void);

/**
 * @brief 초기화 여부 확인
 * @return true 초기화됨, false 미초기화
 */
bool info_manager_is_initialized(void);

/**
 * @brief 장치 ID 조회
 * @param[out] buf 결과 저장 버퍼
 * @param[in] buf_len 버퍼 크기 (최소 INFO_DEVICE_ID_MAX_LEN)
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_ARG 잘못된 인자
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_get_device_id(char* buf, size_t buf_len);

/**
 * @brief 장치 ID 설정
 * @param[in] device_id 새 장치 ID (null-terminated, 최대 15자)
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_ARG 잘못된 인자
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_set_device_id(const char* device_id);

/**
 * @brief MAC 주소 기반 장치 ID 자동 생성
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_generate_device_id(void);

/**
 * @brief 시스템 정보 조회
 * @param[out] info 결과 저장 구조체
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_ARG 잘못된 인자
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_get_system_info(info_system_info_t* info);

/**
 * @brief 시스템 정보 캐시 갱신
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_update_system_info(void);

/**
 * @brief Observer 등록
 * @param[in] callback 콜백 함수
 * @param[in] ctx 사용자 컨텍스트 (콜백에 전달됨)
 * @param[out] out_handle Observer 핸들 (제거 시 사용)
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_ARG 잘못된 인자
 *         ESP_ERR_NO_MEM 메모리 부족
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_add_observer(info_observer_fn_t callback,
                                     void* ctx,
                                     info_observer_handle_t* out_handle);

/**
 * @brief Observer 제거
 * @param[in] handle 등록 시 받은 핸들
 * @return ESP_OK 성공
 *         ESP_ERR_NOT_FOUND 핸들 없음
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_remove_observer(info_observer_handle_t handle);

/**
 * @brief 모든 옵저버에게 알림 (내부용)
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_notify_observers(void);

/**
 * @brief 패킷 송신 카운트 증가
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_increment_packet_tx(void);

/**
 * @brief 패킷 수신 카운트 증가
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_increment_packet_rx(void);

/**
 * @brief LoRa RSSI 설정
 * @param[in] rssi RSSI 값 (단위: 0.1dBm, 음수는 보정 필요)
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_set_lora_rssi(uint32_t rssi);

/**
 * @brief LoRa SNR 설정
 * @param[in] snr SNR 값 (단위: 0.1dB)
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_set_lora_snr(uint32_t snr);

/**
 * @brief 에러 카운트 증가
 * @return ESP_OK 성공
 *         ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_increment_error_count(void);

#ifdef __cplusplus
}
#endif