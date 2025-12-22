/**
#include "log.h"
#include "log_tags.h"
 * vMix 클라이언트 구현
#include "log.h"
#include "log_tags.h"
 *
#include "log.h"
#include "log_tags.h"
 * vMix TCP API 클라이언트 구현
#include "log.h"
#include "log_tags.h"
 */
#include "log.h"
#include "log_tags.h"

#include "log.h"
#include "log_tags.h"
#include "vmix_client.h"
#include "log.h"
#include "log_tags.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * 디버그 매크로 (ESP-IDF 로거 사용)
 * ============================================================================ */

#define VMIX_TAG "VMIX"

#define VMIX_LOGE(fmt, ...) LOG_0(VMIX_TAG, fmt, ##__VA_ARGS__)
#define VMIX_LOGW(fmt, ...) LOG_0(VMIX_TAG, fmt, ##__VA_ARGS__)
#define VMIX_LOGI(fmt, ...) LOG_0(VMIX_TAG, fmt, ##__VA_ARGS__)
#define VMIX_LOGV(fmt, ...) LOG_1(VMIX_TAG, fmt, ##__VA_ARGS__)

/* ============================================================================
 * 내부 함수 선언
 * ============================================================================ */

static int send_command(vmix_client_t* client, const char* cmd);
static int send_subscribe_tally(vmix_client_t* client);
static void process_line(vmix_client_t* client, const char* line);
static void process_tally_response(vmix_client_t* client, const char* data);

/* ============================================================================
 * 생성/소멸
 * ============================================================================ */

int vmix_client_init(vmix_client_t* client, const char* ip, uint16_t port)
{
    if (!client || !ip) {
        return -1;
    }

    /* 플랫폼 초기화 */
    if (sw_platform_init() < 0) {
        return -1;
    }

    memset(client, 0, sizeof(vmix_client_t));

    /* IP 주소 복사 */
    strncpy(client->ip, ip, sizeof(client->ip) - 1);
    client->port = (port > 0) ? port : VMIX_DEFAULT_PORT;

    /* 상태 초기화 */
    vmix_state_init(&client->state);

    /* 소켓은 connect() 시 생성 */
    client->socket = SW_INVALID_SOCKET;

    return 0;
}

void vmix_client_cleanup(vmix_client_t* client)
{
    if (!client) {
        return;
    }

    vmix_client_disconnect(client);
    sw_platform_cleanup();
}

/* ============================================================================
 * 연결 관리
 * ============================================================================ */

int vmix_client_connect(vmix_client_t* client, uint32_t timeout_ms)
{
    if (!client) {
        return -1;
    }

    /* 이미 연결되어 있으면 종료 */
    if (client->state.connected) {
        vmix_client_disconnect(client);
    }

    /* TCP 소켓 생성 */
    client->socket = sw_socket_tcp_create();
    if (client->socket == SW_INVALID_SOCKET) {
        VMIX_LOGE("소켓 생성 실패\n");
        return -1;
    }

    /* 연결 */
    VMIX_LOGI("연결 중: %s:%d", client->ip, client->port);

    if (sw_socket_connect(client->socket, client->ip, client->port, timeout_ms) < 0) {
        VMIX_LOGI("연결 실패");
        sw_socket_close(client->socket);
        client->socket = SW_INVALID_SOCKET;
        return -1;
    }

    /* 상태 초기화 */
    vmix_state_init(&client->state);
    client->state.connected = true;
    client->state.last_contact_ms = sw_platform_millis();

    VMIX_LOGI("연결 성공");

    /* SUBSCRIBE TALLY 전송 */
    if (send_subscribe_tally(client) < 0) {
        VMIX_LOGW("SUBSCRIBE TALLY 전송 실패");
    }

    /* 연결 콜백 */
    if (client->on_connected) {
        client->on_connected(client->user_data);
    }

    return 0;
}

int vmix_client_connect_start(vmix_client_t* client)
{
    if (!client) {
        return -1;
    }

    /* 기존 소켓 정리 (연결 여부와 관계없이) */
    if (client->socket != SW_INVALID_SOCKET) {
        sw_socket_close(client->socket);
        client->socket = SW_INVALID_SOCKET;
    }

    /* TCP 소켓 생성 */
    client->socket = sw_socket_tcp_create();
    if (client->socket == SW_INVALID_SOCKET) {
        VMIX_LOGE("소켓 생성 실패\n");
        return -1;
    }

    /* 논블로킹 연결 시작 */
    VMIX_LOGI("연결 시작: %s:%d", client->ip, client->port);

    int ret = sw_socket_connect_start(client->socket, client->ip, client->port);
    if (ret < 0) {
        VMIX_LOGI("연결 시작 실패");
        sw_socket_close(client->socket);
        client->socket = SW_INVALID_SOCKET;
        return -1;
    }

    /* 상태 초기화 */
    vmix_state_init(&client->state);
    client->state.last_contact_ms = sw_platform_millis();

    if (ret == 0) {
        /* 즉시 연결 완료 - SUBSCRIBE TALLY 전송 */
        client->state.connected = true;
        VMIX_LOGI("연결 즉시 완료");

        if (send_subscribe_tally(client) < 0) {
            VMIX_LOGW("SUBSCRIBE TALLY 전송 실패");
        }

        if (client->on_connected) {
            client->on_connected(client->user_data);
        }
        return 0;
    }

    /* 연결 진행 중 */
    return 1;
}

int vmix_client_connect_check(vmix_client_t* client)
{
    if (!client || client->socket == SW_INVALID_SOCKET) {
        return -1;
    }

    /* 이미 연결됨 */
    if (client->state.connected) {
        return 0;
    }

    /* TCP 연결 상태 체크 */
    int ret = sw_socket_connect_check(client->socket);
    if (ret < 0) {
        VMIX_LOGI("연결 실패");
        vmix_client_disconnect(client);
        return -1;
    }

    if (ret == 1) {
        /* 연결 진행 중 */
        return 1;
    }

    /* TCP 연결 완료 - SUBSCRIBE TALLY 전송 */
    client->state.connected = true;
    client->state.last_contact_ms = sw_platform_millis();
    VMIX_LOGI("연결 완료");

    if (send_subscribe_tally(client) < 0) {
        VMIX_LOGW("SUBSCRIBE TALLY 전송 실패");
    }

    if (client->on_connected) {
        client->on_connected(client->user_data);
    }

    return 0;
}

void vmix_client_disconnect(vmix_client_t* client)
{
    if (!client) {
        return;
    }

    if (client->socket != SW_INVALID_SOCKET) {
        /* QUIT 전송 시도 */
        send_command(client, VMIX_CMD_QUIT);

        sw_socket_close(client->socket);
        client->socket = SW_INVALID_SOCKET;
    }

    bool was_connected = client->state.connected;
    client->state.connected = false;
    client->state.subscribed = false;

    if (was_connected && client->on_disconnected) {
        client->on_disconnected(client->user_data);
    }

    VMIX_LOGI("연결 종료");
}

bool vmix_client_is_connected(const vmix_client_t* client)
{
    return client && client->state.connected;
}

bool vmix_client_is_initialized(const vmix_client_t* client)
{
    /* vMix는 TALLY 구독 완료 + 첫 TALLY 데이터 수신 후 초기화 완료 */
    return client && client->state.connected && client->state.subscribed && client->state.num_cameras > 0;
}

/* ============================================================================
 * 메인 루프
 * ============================================================================ */

int vmix_client_loop(vmix_client_t* client)
{
    if (!client || !client->state.connected) {
        return -1;
    }

    int processed = 0;
    uint32_t now = sw_platform_millis();

    /* TCP 데이터 수신 (논블로킹) */
    int received = sw_socket_recv(client->socket, client->rx_buffer,
                                  sizeof(client->rx_buffer) - 1, 0);

    if (received > 0) {
        client->state.last_contact_ms = now;
        client->rx_buffer[received] = '\0';

        /* 라인 단위로 파싱 */
        for (int i = 0; i < received; i++) {
            char c = (char)client->rx_buffer[i];

            if (c == '\r') {
                /* 무시 */
                continue;
            }

            if (c == '\n') {
                /* 라인 완료 */
                client->state.line_buffer[client->state.line_pos] = '\0';

                if (client->state.line_pos > 0) {
                    process_line(client, client->state.line_buffer);
                    processed++;
                }

                client->state.line_pos = 0;
            } else {
                /* 버퍼에 추가 */
                if (client->state.line_pos < VMIX_LINE_BUFFER_SIZE - 1) {
                    client->state.line_buffer[client->state.line_pos++] = c;
                }
            }
        }
    } else if (received < 0) {
        /* recv 에러 - 논블로킹 소켓이므로 EAGAIN 가능성 있음
         * 타임아웃으로 연결 끊김 감지하므로 여기서는 로그만 */
        VMIX_LOGV("recv 에러 (errno 체크 필요, 타임아웃으로 감지)\n");
    }
    /* received == 0: 데이터 없음 (논블로킹), 계속 진행 */

    /* 타임아웃 체크 */
    if (client->state.connected &&
        now - client->state.last_contact_ms > VMIX_MAX_SILENCE_TIME_MS) {
        VMIX_LOGE("연결 타임아웃 (무응답 %dms)\n",
                  (int)(now - client->state.last_contact_ms));
        vmix_client_disconnect(client);
        return -1;
    }

    /* Keepalive 전송 (TALLY 요청으로 연결 유지) */
    if (client->state.subscribed &&
        now - client->state.last_keepalive_ms > VMIX_KEEPALIVE_INTERVAL_MS) {
        send_command(client, VMIX_CMD_TALLY);
        client->state.last_keepalive_ms = now;
        VMIX_LOGV("Keepalive 전송 (TALLY)\n");
    }

    return processed;
}

/* ============================================================================
 * Program/Preview 조회
 * ============================================================================ */

uint16_t vmix_client_get_program_input(const vmix_client_t* client)
{
    if (!client) return 0;
    return client->state.program_input;
}

uint16_t vmix_client_get_preview_input(const vmix_client_t* client)
{
    if (!client) return 0;
    return client->state.preview_input;
}

/* ============================================================================
 * Tally 조회
 * ============================================================================ */

uint8_t vmix_client_get_tally_by_index(const vmix_client_t* client, uint8_t index)
{
    if (!client || index >= VMIX_MAX_CHANNELS) return 0;
    return vmix_tally_get(client->state.tally_packed, index);
}

uint64_t vmix_client_get_tally_packed(const vmix_client_t* client)
{
    if (!client) return 0;
    return client->state.tally_packed;
}

uint8_t vmix_client_get_tally_count(const vmix_client_t* client)
{
    if (!client) return 0;
    return client->state.num_cameras;
}

/* ============================================================================
 * 제어 명령
 * ============================================================================ */

int vmix_client_cut(vmix_client_t* client)
{
    return vmix_client_function(client, VMIX_FUNC_CUT, NULL);
}

int vmix_client_fade(vmix_client_t* client)
{
    return vmix_client_function(client, VMIX_FUNC_FADE, NULL);
}

int vmix_client_set_preview_input(vmix_client_t* client, uint16_t input)
{
    char params[32];
    snprintf(params, sizeof(params), "Input=%d", input);
    return vmix_client_function(client, VMIX_FUNC_PREVIEW_INPUT, params);
}

int vmix_client_set_program_input(vmix_client_t* client, uint16_t input)
{
    char params[32];
    snprintf(params, sizeof(params), "Input=%d", input);
    return vmix_client_function(client, VMIX_FUNC_ACTIVE_INPUT, params);
}

int vmix_client_quick_play(vmix_client_t* client, uint16_t input)
{
    char params[32];
    snprintf(params, sizeof(params), "Input=%d", input);
    return vmix_client_function(client, VMIX_FUNC_QUICK_PLAY, params);
}

int vmix_client_overlay_in(vmix_client_t* client, uint8_t overlay_index, uint16_t input)
{
    if (overlay_index < 1 || overlay_index > 4) return -1;

    const char* funcs[] = {
        VMIX_FUNC_OVERLAY1_IN,
        VMIX_FUNC_OVERLAY2_IN,
        VMIX_FUNC_OVERLAY3_IN,
        VMIX_FUNC_OVERLAY4_IN
    };

    char params[32];
    snprintf(params, sizeof(params), "Input=%d", input);
    return vmix_client_function(client, funcs[overlay_index - 1], params);
}

int vmix_client_overlay_out(vmix_client_t* client, uint8_t overlay_index)
{
    if (overlay_index < 1 || overlay_index > 4) return -1;

    const char* funcs[] = {
        VMIX_FUNC_OVERLAY1_OUT,
        VMIX_FUNC_OVERLAY2_OUT,
        VMIX_FUNC_OVERLAY3_OUT,
        VMIX_FUNC_OVERLAY4_OUT
    };

    return vmix_client_function(client, funcs[overlay_index - 1], NULL);
}

int vmix_client_function(vmix_client_t* client, const char* function, const char* params)
{
    if (!client || !client->state.connected || !function) {
        return -1;
    }

    char cmd[VMIX_TX_BUFFER_SIZE];
    if (params && strlen(params) > 0) {
        snprintf(cmd, sizeof(cmd), "%s %s %s", VMIX_CMD_FUNCTION, function, params);
    } else {
        snprintf(cmd, sizeof(cmd), "%s %s", VMIX_CMD_FUNCTION, function);
    }

    VMIX_LOGV("명령 전송: %s\n", cmd);
    return send_command(client, cmd);
}

/* ============================================================================
 * 콜백 설정
 * ============================================================================ */

void vmix_client_set_on_connected(vmix_client_t* client, void (*callback)(void*), void* user_data)
{
    if (client) {
        client->on_connected = callback;
        client->user_data = user_data;
    }
}

void vmix_client_set_on_disconnected(vmix_client_t* client, void (*callback)(void*), void* user_data)
{
    if (client) {
        client->on_disconnected = callback;
        client->user_data = user_data;
    }
}

void vmix_client_set_on_tally_changed(vmix_client_t* client, void (*callback)(void*), void* user_data)
{
    if (client) {
        client->on_tally_changed = callback;
        client->user_data = user_data;
    }
}

/* ============================================================================
 * 디버그
 * ============================================================================ */

void vmix_client_set_debug(vmix_client_t* client, bool enable)
{
    if (client) {
        client->debug = enable;
        sw_set_debug(enable);
    }
}

/* ============================================================================
 * 내부 함수 구현
 * ============================================================================ */

static int send_command(vmix_client_t* client, const char* cmd)
{
    if (!client || !cmd || client->socket == SW_INVALID_SOCKET) {
        return -1;
    }

    /* 명령어 + CRLF */
    int len = snprintf((char*)client->tx_buffer, sizeof(client->tx_buffer),
                       "%s\r\n", cmd);

    if (len <= 0 || len >= (int)sizeof(client->tx_buffer)) {
        return -1;
    }

    int sent = sw_socket_send(client->socket, client->tx_buffer, (uint16_t)len);
    if (sent < 0) {
        VMIX_LOGE("전송 실패\n");
        return -1;
    }

    return 0;
}

static int send_subscribe_tally(vmix_client_t* client)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%s %s", VMIX_CMD_SUBSCRIBE, VMIX_CMD_TALLY);
    return send_command(client, cmd);
}

static void process_line(vmix_client_t* client, const char* line)
{
    VMIX_LOGV("수신: %s\n", line);

    /* TALLY 응답 처리 */
    if (strncmp(line, "TALLY OK ", 9) == 0) {
        process_tally_response(client, line + 9);
        return;
    }

    /* SUBSCRIBE 응답 */
    if (strncmp(line, "SUBSCRIBE OK TALLY", 18) == 0) {
        client->state.subscribed = true;
        VMIX_LOGI("TALLY 구독 완료");
        return;
    }

    /* 에러 응답 */
    if (strstr(line, " ER ") != NULL) {
        VMIX_LOGW("에러 응답: %s\n", line);
        return;
    }
}

static void process_tally_response(vmix_client_t* client, const char* data)
{
    uint64_t prev_tally = client->state.tally_packed;

    /* Tally 데이터 업데이트 */
    uint16_t len = (uint16_t)strlen(data);
    vmix_state_update_tally(&client->state, data, len);

    VMIX_LOGV("Tally 업데이트: count=%d, pgm=%d, pvw=%d, packed=0x%016llX",
              client->state.num_cameras,
              client->state.program_input,
              client->state.preview_input,
              (unsigned long long)client->state.tally_packed);

    /* 변경 감지 및 콜백 */
    if (client->state.tally_packed != prev_tally) {
        if (client->on_tally_changed) {
            client->on_tally_changed(client->user_data);
        }
    }
}
