/**
 * Switcher 플랫폼 추상화 레이어
 *
 * 플랫폼별 함수를 추상화하여 이식성을 제공
 * - Linux: POSIX sockets, gettimeofday
 * - ESP-IDF: lwIP sockets, esp_timer
 *
 * 순수 C 언어로 작성
 */

#ifndef SW_PLATFORM_H
#define SW_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 플랫폼 설정
 * ============================================================================ */

/* 플랫폼 자동 감지 */
#if defined(ESP_PLATFORM)
    #define SW_PLATFORM_ESP_IDF
#elif defined(__linux__) || defined(__unix__)
    #define SW_PLATFORM_LINUX
#else
    #error "Unsupported platform"
#endif

/* ============================================================================
 * 소켓 핸들 타입
 * ============================================================================ */

typedef int sw_socket_t;
#define SW_INVALID_SOCKET (-1)

/* ============================================================================
 * 플랫폼 함수 선언
 * ============================================================================ */

/**
 * 플랫폼 초기화
 *
 * @return 성공 시 0, 실패 시 -1
 */
int sw_platform_init(void);

/**
 * 플랫폼 정리
 */
void sw_platform_cleanup(void);

/**
 * 현재 시간 (밀리초)
 *
 * @return 밀리초 단위 현재 시간
 */
uint32_t sw_platform_millis(void);

/**
 * 지연 (밀리초)
 *
 * @param ms 지연 시간 (밀리초)
 */
void sw_platform_delay(uint32_t ms);

/* ============================================================================
 * UDP 소켓 함수
 * ============================================================================ */

/**
 * UDP 소켓 생성
 *
 * @return 소켓 핸들, 실패 시 SW_INVALID_SOCKET
 */
sw_socket_t sw_socket_udp_create(void);

/**
 * TCP 소켓 생성
 *
 * @return 소켓 핸들, 실패 시 SW_INVALID_SOCKET
 */
sw_socket_t sw_socket_tcp_create(void);

/**
 * 소켓 닫기
 *
 * @param sock 소켓 핸들
 */
void sw_socket_close(sw_socket_t sock);

/**
 * 소켓을 로컬 포트에 바인드
 *
 * @param sock 소켓 핸들
 * @param port 로컬 포트 (0이면 자동 할당)
 * @return 성공 시 0, 실패 시 -1
 */
int sw_socket_bind(sw_socket_t sock, uint16_t port);

/**
 * TCP 연결
 *
 * @param sock 소켓 핸들
 * @param ip 목적지 IP 주소 (문자열)
 * @param port 목적지 포트
 * @param timeout_ms 타임아웃 (밀리초)
 * @return 성공 시 0, 실패 시 -1
 */
int sw_socket_connect(sw_socket_t sock, const char* ip, uint16_t port, uint32_t timeout_ms);

/**
 * TCP 연결 시작 (논블로킹)
 *
 * 논블로킹 방식으로 연결을 시작합니다.
 * 연결 진행 상태는 sw_socket_connect_check()로 확인해야 합니다.
 *
 * @param sock 소켓 핸들
 * @param ip 목적지 IP 주소 (문자열)
 * @param port 목적지 포트
 * @return 성공 시 0, 진행중 1, 실패 시 -1
 */
int sw_socket_connect_start(sw_socket_t sock, const char* ip, uint16_t port);

/**
 * TCP 연결 상태 확인 (논블로킹)
 *
 * sw_socket_connect_start() 호출 후 연결 완료 여부를 확인합니다.
 *
 * @param sock 소켓 핸들
 * @return 연결 완료 0, 진행중 1, 실패 시 -1
 */
int sw_socket_connect_check(sw_socket_t sock);

/**
 * 소켓을 논블로킹 모드로 설정
 *
 * @param sock 소켓 핸들
 * @return 성공 시 0, 실패 시 -1
 */
int sw_socket_set_nonblocking(sw_socket_t sock);

/**
 * 소켓 타임아웃 설정
 *
 * @param sock 소켓 핸들
 * @param timeout_ms 타임아웃 (밀리초)
 * @return 성공 시 0, 실패 시 -1
 */
int sw_socket_set_timeout(sw_socket_t sock, uint32_t timeout_ms);

/**
 * UDP 패킷 전송
 *
 * @param sock 소켓 핸들
 * @param ip 목적지 IP 주소 (문자열)
 * @param port 목적지 포트
 * @param data 데이터
 * @param length 데이터 길이
 * @return 전송된 바이트 수, 실패 시 -1
 */
int sw_socket_sendto(sw_socket_t sock, const char* ip, uint16_t port,
                     const uint8_t* data, uint16_t length);

/**
 * TCP 데이터 전송
 *
 * @param sock 소켓 핸들
 * @param data 데이터
 * @param length 데이터 길이
 * @return 전송된 바이트 수, 실패 시 -1
 */
int sw_socket_send(sw_socket_t sock, const uint8_t* data, uint16_t length);

/**
 * UDP 패킷 수신 (논블로킹)
 *
 * @param sock 소켓 핸들
 * @param buffer 수신 버퍼
 * @param buffer_size 버퍼 크기
 * @param timeout_ms 타임아웃 (0이면 즉시 반환)
 * @return 수신된 바이트 수, 데이터 없으면 0, 에러 시 -1
 */
int sw_socket_recvfrom(sw_socket_t sock, uint8_t* buffer, uint16_t buffer_size,
                       uint32_t timeout_ms);

/**
 * UDP 패킷 수신 (논블로킹, select 미사용)
 *
 * 소켓이 이미 논블로킹 모드로 설정되어 있어야 함
 * select() 오버헤드 없이 직접 recvfrom 호출
 *
 * @param sock 소켓 핸들 (논블로킹 모드)
 * @param buffer 수신 버퍼
 * @param buffer_size 버퍼 크기
 * @return 수신된 바이트 수, 데이터 없으면 0, 에러 시 -1
 */
int sw_socket_recvfrom_nb(sw_socket_t sock, uint8_t* buffer, uint16_t buffer_size);

/**
 * TCP 데이터 수신 (논블로킹)
 *
 * @param sock 소켓 핸들
 * @param buffer 수신 버퍼
 * @param buffer_size 버퍼 크기
 * @param timeout_ms 타임아웃 (0이면 즉시 반환)
 * @return 수신된 바이트 수, 데이터 없으면 0, 에러 시 -1
 */
int sw_socket_recv(sw_socket_t sock, uint8_t* buffer, uint16_t buffer_size,
                   uint32_t timeout_ms);

/* ============================================================================
 * 디버그 출력
 * ============================================================================ */

/**
 * 디버그 메시지 출력
 *
 * @param fmt printf 형식 문자열
 * @param ... 가변 인수
 */
void sw_log(const char* fmt, ...);

/**
 * 디버그 활성화/비활성화
 *
 * @param enable true면 활성화
 */
void sw_set_debug(bool enable);

/**
 * 디버그 활성화 여부
 *
 * @return true면 디버그 활성화됨
 */
bool sw_is_debug(void);

#ifdef __cplusplus
}
#endif

#endif /* SW_PLATFORM_H */
