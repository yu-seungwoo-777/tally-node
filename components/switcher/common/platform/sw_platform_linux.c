/**
 * Switcher 플랫폼 구현 - Linux
 *
 * POSIX sockets 및 시스템 함수를 사용한 Linux 플랫폼 구현
 */

#ifdef __linux__

#define _DEFAULT_SOURCE  /* usleep 선언 활성화 */

#include "sw_platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* 디버그 플래그 */
static bool g_debug_enabled = false;

/* 시작 시간 */
static struct timeval g_start_time;
static bool g_initialized = false;

/* ============================================================================
 * 플랫폼 함수 구현
 * ============================================================================ */

int sw_platform_init(void)
{
    if (!g_initialized) {
        gettimeofday(&g_start_time, NULL);
        g_initialized = true;
    }
    return 0;
}

void sw_platform_cleanup(void)
{
    g_initialized = false;
}

uint32_t sw_platform_millis(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    uint32_t sec = (uint32_t)(now.tv_sec - g_start_time.tv_sec);
    int32_t usec = now.tv_usec - g_start_time.tv_usec;

    if (usec < 0) {
        sec--;
        usec += 1000000;
    }

    return sec * 1000 + usec / 1000;
}

void sw_platform_delay(uint32_t ms)
{
    usleep(ms * 1000);
}

/* ============================================================================
 * 소켓 함수 구현
 * ============================================================================ */

sw_socket_t sw_socket_udp_create(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        sw_log("[ERROR] socket(UDP) 실패: %s\n", strerror(errno));
        return SW_INVALID_SOCKET;
    }

    /* SO_REUSEADDR 설정 */
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    return sock;
}

sw_socket_t sw_socket_tcp_create(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        sw_log("[ERROR] socket(TCP) 실패: %s\n", strerror(errno));
        return SW_INVALID_SOCKET;
    }

    /* SO_REUSEADDR 설정 */
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    return sock;
}

void sw_socket_close(sw_socket_t sock)
{
    if (sock != SW_INVALID_SOCKET) {
        close(sock);
    }
}

int sw_socket_bind(sw_socket_t sock, uint16_t port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        sw_log("[ERROR] bind() 실패: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int sw_socket_connect(sw_socket_t sock, const char* ip, uint16_t port, uint32_t timeout_ms)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        sw_log("[ERROR] inet_pton() 실패: 잘못된 IP 주소 '%s'\n", ip);
        return -1;
    }

    /* 논블로킹 모드로 설정 */
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        sw_log("[ERROR] connect() 실패: %s\n", strerror(errno));
        fcntl(sock, F_SETFL, flags);  /* 원래 모드로 복구 */
        return -1;
    }

    /* 연결 완료 대기 */
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    ret = select(sock + 1, NULL, &writefds, NULL, &tv);
    if (ret <= 0) {
        sw_log("[ERROR] connect() 타임아웃\n");
        fcntl(sock, F_SETFL, flags);
        return -1;
    }

    /* 연결 에러 확인 */
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
    if (error != 0) {
        sw_log("[ERROR] connect() 실패: %s\n", strerror(error));
        fcntl(sock, F_SETFL, flags);
        return -1;
    }

    /* 블로킹 모드로 복구 */
    fcntl(sock, F_SETFL, flags);
    return 0;
}

int sw_socket_set_nonblocking(sw_socket_t sock)
{
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }

    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }

    return 0;
}

int sw_socket_set_timeout(sw_socket_t sock, uint32_t timeout_ms)
{
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return -1;
    }

    return 0;
}

int sw_socket_sendto(sw_socket_t sock, const char* ip, uint16_t port,
                     const uint8_t* data, uint16_t length)
{
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &dest.sin_addr) <= 0) {
        sw_log("[ERROR] inet_pton() 실패: 잘못된 IP 주소 '%s'\n", ip);
        return -1;
    }

    ssize_t sent = sendto(sock, data, length, 0,
                          (struct sockaddr*)&dest, sizeof(dest));
    if (sent < 0) {
        sw_log("[ERROR] sendto() 실패: %s\n", strerror(errno));
        return -1;
    }

    return (int)sent;
}

int sw_socket_send(sw_socket_t sock, const uint8_t* data, uint16_t length)
{
    ssize_t sent = send(sock, data, length, 0);
    if (sent < 0) {
        sw_log("[ERROR] send() 실패: %s\n", strerror(errno));
        return -1;
    }

    return (int)sent;
}

int sw_socket_recvfrom(sw_socket_t sock, uint8_t* buffer, uint16_t buffer_size,
                       uint32_t timeout_ms)
{
    /* select()로 타임아웃 처리 */
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    /* timeout_ms=0이면 즉시 반환 (논블로킹), NULL은 무한 대기 */
    int ret = select(sock + 1, &readfds, NULL, NULL, &tv);

    if (ret < 0) {
        if (errno == EINTR) {
            return 0;  /* 인터럽트, 데이터 없음으로 처리 */
        }
        sw_log("[ERROR] select() 실패: %s\n", strerror(errno));
        return -1;
    }

    if (ret == 0) {
        return 0;  /* 타임아웃, 데이터 없음 */
    }

    /* 데이터 수신 */
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);

    ssize_t received = recvfrom(sock, buffer, buffer_size, 0,
                                (struct sockaddr*)&from, &from_len);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  /* 데이터 없음 */
        }
        sw_log("[ERROR] recvfrom() 실패: %s\n", strerror(errno));
        return -1;
    }

    return (int)received;
}

int sw_socket_recv(sw_socket_t sock, uint8_t* buffer, uint16_t buffer_size,
                   uint32_t timeout_ms)
{
    /* select()로 타임아웃 처리 */
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    /* timeout_ms=0일 때도 tv를 전달 (즉시 반환) */
    int ret = select(sock + 1, &readfds, NULL, NULL, &tv);

    if (ret < 0) {
        if (errno == EINTR) {
            return 0;
        }
        sw_log("[ERROR] select() 실패: %s\n", strerror(errno));
        return -1;
    }

    if (ret == 0) {
        return 0;
    }

    ssize_t received = recv(sock, buffer, buffer_size, 0);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        sw_log("[ERROR] recv() 실패: %s\n", strerror(errno));
        return -1;
    }

    if (received == 0) {
        return -1;  /* 연결 종료 */
    }

    return (int)received;
}

/* ============================================================================
 * 디버그 출력 구현
 * ============================================================================ */

void sw_log(const char* fmt, ...)
{
    if (!g_debug_enabled) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

void sw_set_debug(bool enable)
{
    g_debug_enabled = enable;
}

bool sw_is_debug(void)
{
    return g_debug_enabled;
}

#endif /* __linux__ */
