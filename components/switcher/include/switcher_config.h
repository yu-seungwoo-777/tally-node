/**
 * 스위처 공통 설정
 *
 * 모든 스위처 프로토콜에서 사용하는 타임아웃, 재시도 등의 설정값
 * 이 파일을 수정하면 모든 프로토콜에 일괄 적용됨
 */

#ifndef SWITCHER_CONFIG_H
#define SWITCHER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 연결 타임아웃 설정 (ms)
 * ============================================================================ */

/**
 * 연결 시도 타임아웃
 *
 * 스위처에 TCP 연결을 시도할 때 최대 대기 시간
 * 기본값: 5000ms (5초)
 */
#ifndef SWITCHER_CONNECT_TIMEOUT_MS
#define SWITCHER_CONNECT_TIMEOUT_MS      5000
#endif

/**
 * 응답 타임아웃
 *
 * 명령을 보낸 후 응답을 기다리는 최대 시간
 * 기본값: 2000ms (2초)
 */
#ifndef SWITCHER_RESPONSE_TIMEOUT_MS
#define SWITCHER_RESPONSE_TIMEOUT_MS     2000
#endif

/**
 * 최대 무응답 시간
 *
 * 스위처로부터 데이터를 받지 못한 채로 이 시간이 지나면 연결 끊김으로 간주
 * 기본값: 5000ms (5초) - ATEMbase, PyATEMMax와 동일
 */
#ifndef SWITCHER_MAX_SILENCE_TIME_MS
#define SWITCHER_MAX_SILENCE_TIME_MS     5000
#endif

/* ============================================================================
 * Keepalive 설정 (ms)
 * ============================================================================ */

/**
 * Keepalive 전송 간격
 *
 * 주기적으로 keepalive 패킷을 보내는 간격
 * - ATEM: ACK 패킷
 * - vMix: TALLY 명령
 * - OBS: Heartbeat 메시지
 *
 * 기본값: 5000ms (5초)
 */
#ifndef SWITCHER_KEEPALIVE_INTERVAL_MS
#define SWITCHER_KEEPALIVE_INTERVAL_MS   5000
#endif

/* ============================================================================
 * 재시도 설정
 * ============================================================================ */

/**
 * 최대 재시도 횟수
 *
 * 연결 실패 시 최대 재시도 횟수
 * 기본값: 3회
 */
#ifndef SWITCHER_MAX_RETRIES
#define SWITCHER_MAX_RETRIES             3
#endif

/**
 * 재시도 지연 시간
 *
 * 재시도 사이의 대기 시간
 * 기본값: 500ms
 */
#ifndef SWITCHER_RETRY_DELAY_MS
#define SWITCHER_RETRY_DELAY_MS          500
#endif

/* ============================================================================
 * 재연결 설정
 * ============================================================================ */

/**
 * 재연결 간격
 *
 * 연결이 끊어진 후 재연결을 시도하는 간격
 * 기본값: 5000ms (5초)
 */
#ifndef SWITCHER_RECONNECT_INTERVAL_MS
#define SWITCHER_RECONNECT_INTERVAL_MS   5000
#endif

/* ============================================================================
 * 프로토콜별 기본값 오버라이드
 *
 * 필요 시 각 프로토콜에서 다른 값을 사용할 수 있음
 * ============================================================================ */

/**
 * ATEM Keepalive 간격 (1초)
 * 참고: ATEMbase, PyATEMMax와 동일
 * - 1초마다 ACK 패킷 전송
 * - 5초 타임아웃 동안 5번의 재시도 가능
 */
#define ATEM_KEEPALIVE_INTERVAL_MS       1000

/**
 * vMix Keepalive 간격 (3초로 설정)
 * vMix는 5초 타임아웃보다 짧은 간격으로 keepalive 전송 필요
 */
#define VMIX_KEEPALIVE_INTERVAL_MS       3000

/**
 * OBS Keepalive 간격 (10초로 설정)
 * OBS는 WebSocket Heartbeat 사용
 */
#define OBS_KEEPALIVE_INTERVAL_MS        10000

#ifdef __cplusplus
}
#endif

#endif /* SWITCHER_CONFIG_H */
