/**
 * @file TxPageTypes.h
 * @brief TX 페이지 내부 데이터 타입 정의
 *
 * 3단계 상태 표시를 위한 ENUM 타입 정의
 */

#ifndef TX_PAGE_TYPES_H
#define TX_PAGE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 네트워크 연결 상태 코드
 *
 * SPEC-STATUS-DASHBOARD-001 REQ-014, REQ-015 참조
 */
typedef enum {
    TX_NET_STATUS_NOT_DETECTED = 0,  // 하드웨어 미감지 [━]
    TX_NET_STATUS_DISCONNECTED = 1,  // 연결 안 됨 [X]
    TX_NET_STATUS_CONNECTED = 2      // 연결됨 [V]
} tx_network_status_t;

/**
 * @brief AP (Access Point) 상태 코드
 *
 * SPEC-STATUS-DASHBOARD-001 REQ-013 참조
 */
typedef enum {
    TX_AP_STATUS_INACTIVE = 0,  // 비활성화 [X]
    TX_AP_STATUS_ACTIVE = 1     // 활성화 [V]
} tx_ap_status_t;

#ifdef __cplusplus
}
#endif

#endif // TX_PAGE_TYPES_H
