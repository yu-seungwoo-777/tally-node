/**
 * @file obs_websocket.c
 * @brief WebSocket 클라이언트 (순수 C)
 *
 * RFC 6455 기반 WebSocket 클라이언트
 */

#include "obs_websocket.h"
#include "obs_base64.h"
#include "obs_sha256.h"
#include "sw_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* WebSocket GUID (RFC 6455) */
static const char* WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

/*===========================================================================
 * 내부 함수
 *===========================================================================*/

/* 랜덤 WebSocket 키 생성 */
static void generate_websocket_key(char* key, size_t len)
{
    (void)len;  /* unused - key buffer assumed to be at least 25 bytes */
    uint8_t random[16];

    /* 간단한 랜덤 생성 (실제로는 더 좋은 랜덤 필요) */
    srand((unsigned int)time(NULL) ^ (unsigned int)sw_platform_millis());
    for (int i = 0; i < 16; i++) {
        random[i] = (uint8_t)(rand() & 0xFF);
    }

    base64_encode(random, 16, key);
}

/* SHA-1 구현 (WebSocket Accept 검증용) */
#define SHA1_DIGEST_SIZE 20

static void sha1(const uint8_t* data, size_t len, uint8_t digest[SHA1_DIGEST_SIZE])
{
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    /* WebSocket key + GUID 는 최대 ~100바이트, 2블록(128바이트)이면 충분 */
    uint8_t msg[128];
    memset(msg, 0, sizeof(msg));

    if (len > 100) len = 100;  /* 안전장치 */
    memcpy(msg, data, len);
    msg[len] = 0x80;

    /* 메시지 길이 결정 (64 또는 128) */
    size_t new_len = (len + 8 < 64) ? 64 : 128;

    uint64_t bits_len = len * 8;
    msg[new_len - 1] = (uint8_t)(bits_len);
    msg[new_len - 2] = (uint8_t)(bits_len >> 8);
    msg[new_len - 3] = (uint8_t)(bits_len >> 16);
    msg[new_len - 4] = (uint8_t)(bits_len >> 24);

    /* 블록 처리 */
    for (size_t offset = 0; offset < new_len; offset += 64) {
        uint32_t w[80];

        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)msg[offset + i*4] << 24) |
                   ((uint32_t)msg[offset + i*4 + 1] << 16) |
                   ((uint32_t)msg[offset + i*4 + 2] << 8) |
                   ((uint32_t)msg[offset + i*4 + 3]);
        }

        for (int i = 16; i < 80; i++) {
            uint32_t temp = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = (temp << 1) | (temp >> 31);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    /* 결과 저장 (big-endian) */
    digest[0] = (uint8_t)(h0 >> 24); digest[1] = (uint8_t)(h0 >> 16);
    digest[2] = (uint8_t)(h0 >> 8);  digest[3] = (uint8_t)h0;
    digest[4] = (uint8_t)(h1 >> 24); digest[5] = (uint8_t)(h1 >> 16);
    digest[6] = (uint8_t)(h1 >> 8);  digest[7] = (uint8_t)h1;
    digest[8] = (uint8_t)(h2 >> 24); digest[9] = (uint8_t)(h2 >> 16);
    digest[10] = (uint8_t)(h2 >> 8); digest[11] = (uint8_t)h2;
    digest[12] = (uint8_t)(h3 >> 24); digest[13] = (uint8_t)(h3 >> 16);
    digest[14] = (uint8_t)(h3 >> 8); digest[15] = (uint8_t)h3;
    digest[16] = (uint8_t)(h4 >> 24); digest[17] = (uint8_t)(h4 >> 16);
    digest[18] = (uint8_t)(h4 >> 8); digest[19] = (uint8_t)h4;
}

/* Sec-WebSocket-Accept 검증 (RFC 6455: SHA-1) */
static bool verify_websocket_accept(const char* key, const char* accept)
{
    char combined[128];
    snprintf(combined, sizeof(combined), "%s%s", key, WS_GUID);

    uint8_t hash[SHA1_DIGEST_SIZE];
    sha1((const uint8_t*)combined, strlen(combined), hash);

    char expected[64];
    base64_encode(hash, SHA1_DIGEST_SIZE, expected);

    return strcmp(expected, accept) == 0;
}

/* HTTP 핸드셰이크 요청 전송 */
static int send_handshake(ws_client_t* ws)
{
    char request[512];
    int len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        ws->path, ws->host, ws->port, ws->websocket_key);

    return sw_socket_send(ws->socket_fd, (const uint8_t*)request, len);
}

/* HTTP 핸드셰이크 응답 파싱 */
static int parse_handshake_response(ws_client_t* ws)
{
    /* Null terminate for strstr */
    if (ws->recv_len < WS_RECV_BUFFER_SIZE) {
        ws->recv_buffer[ws->recv_len] = '\0';
    }

    /* HTTP 응답 끝 찾기 */
    char* end = strstr((char*)ws->recv_buffer, "\r\n\r\n");
    if (!end) return 0;  /* 아직 완료되지 않음 */

    /* HTTP 상태 코드 확인 */
    if (strncmp((char*)ws->recv_buffer, "HTTP/1.1 101", 12) != 0) {
        return -1;  /* 업그레이드 실패 */
    }

    /* Sec-WebSocket-Accept 찾기 */
    char* accept_header = strstr((char*)ws->recv_buffer, "Sec-WebSocket-Accept: ");
    if (!accept_header) {
        return -1;
    }

    accept_header += strlen("Sec-WebSocket-Accept: ");
    char accept_value[64] = {0};
    char* accept_end = strstr(accept_header, "\r\n");
    if (accept_end && (accept_end - accept_header) < 64) {
        strncpy(accept_value, accept_header, accept_end - accept_header);
    }

    /* Accept 값 검증 */
    if (!verify_websocket_accept(ws->websocket_key, accept_value)) {
        return -1;
    }

    /* 남은 데이터 처리 */
    size_t header_len = (end + 4) - (char*)ws->recv_buffer;
    if (ws->recv_len > header_len) {
        memmove(ws->recv_buffer, end + 4, ws->recv_len - header_len);
        ws->recv_len -= header_len;
    } else {
        ws->recv_len = 0;
    }

    ws->handshake_complete = true;
    return 1;  /* 성공 */
}

/* WebSocket 프레임 전송 */
static int send_frame(ws_client_t* ws, uint8_t opcode, const uint8_t* data, size_t len)
{
    if (!ws || ws->socket_fd < 0) return -1;

    uint8_t* buf = ws->send_buffer;
    size_t pos = 0;

    /* FIN + opcode */
    buf[pos++] = 0x80 | opcode;

    /* 마스킹 비트 + 길이 */
    if (len < 126) {
        buf[pos++] = 0x80 | (uint8_t)len;  /* 클라이언트는 마스킹 필수 */
    } else if (len < 65536) {
        buf[pos++] = 0x80 | 126;
        buf[pos++] = (uint8_t)(len >> 8);
        buf[pos++] = (uint8_t)len;
    } else {
        buf[pos++] = 0x80 | 127;
        for (int i = 7; i >= 0; i--) {
            buf[pos++] = (uint8_t)(len >> (i * 8));
        }
    }

    /* 마스킹 키 생성 */
    uint8_t mask[4];
    for (int i = 0; i < 4; i++) {
        mask[i] = (uint8_t)(rand() & 0xFF);
    }
    memcpy(&buf[pos], mask, 4);
    pos += 4;

    /* 버퍼 크기 확인 */
    if (pos + len > WS_SEND_BUFFER_SIZE) {
        return -1;
    }

    /* 데이터 마스킹 및 복사 */
    for (size_t i = 0; i < len; i++) {
        buf[pos++] = data[i] ^ mask[i & 3];
    }

    return sw_socket_send(ws->socket_fd, buf, pos);
}

/* WebSocket 프레임 수신 처리 */
static int process_frames(ws_client_t* ws)
{
    while (ws->recv_len >= 2) {
        uint8_t* buf = ws->recv_buffer;
        size_t pos = 0;

        /* FIN, opcode */
        bool fin = (buf[pos] & 0x80) != 0;
        uint8_t opcode = buf[pos] & 0x0F;
        pos++;

        /* 마스크, 길이 */
        bool masked = (buf[pos] & 0x80) != 0;
        uint64_t payload_len = buf[pos] & 0x7F;
        pos++;

        if (payload_len == 126) {
            if (ws->recv_len < pos + 2) return 0;  /* 더 많은 데이터 필요 */
            payload_len = ((uint64_t)buf[pos] << 8) | buf[pos + 1];
            pos += 2;
        } else if (payload_len == 127) {
            if (ws->recv_len < pos + 8) return 0;
            payload_len = 0;
            for (int i = 0; i < 8; i++) {
                payload_len = (payload_len << 8) | buf[pos + i];
            }
            pos += 8;
        }

        /* 마스킹 키 */
        uint8_t mask[4] = {0};
        if (masked) {
            if (ws->recv_len < pos + 4) return 0;
            memcpy(mask, &buf[pos], 4);
            pos += 4;
        }

        /* 페이로드 */
        if (ws->recv_len < pos + payload_len) return 0;  /* 더 많은 데이터 필요 */

        /* 마스크 해제 (서버 응답은 보통 마스킹 안됨) */
        if (masked) {
            for (uint64_t i = 0; i < payload_len; i++) {
                buf[pos + i] ^= mask[i & 3];
            }
        }

        /* opcode 처리 */
        switch (opcode) {
            case WS_OPCODE_TEXT:
            case WS_OPCODE_BINARY:
                if (fin) {
                    /* 완전한 메시지 */
                    if (ws->on_message) {
                        ws->on_message((char*)&buf[pos], (size_t)payload_len, ws->user_data);
                    }
                } else {
                    /* 분할 메시지 시작 */
                    if (payload_len < WS_FRAME_BUFFER_SIZE) {
                        memcpy(ws->frame_buffer, &buf[pos], (size_t)payload_len);
                        ws->frame_pos = (size_t)payload_len;
                        ws->frame_started = true;
                    }
                }
                break;

            case WS_OPCODE_CONTINUATION:
                if (ws->frame_started) {
                    size_t space = WS_FRAME_BUFFER_SIZE - ws->frame_pos;
                    size_t copy = (payload_len < space) ? (size_t)payload_len : space;
                    memcpy(&ws->frame_buffer[ws->frame_pos], &buf[pos], copy);
                    ws->frame_pos += copy;

                    if (fin) {
                        if (ws->on_message) {
                            ws->on_message((char*)ws->frame_buffer, ws->frame_pos, ws->user_data);
                        }
                        ws->frame_started = false;
                        ws->frame_pos = 0;
                    }
                }
                break;

            case WS_OPCODE_CLOSE:
                ws->state = WS_STATE_CLOSING;
                /* Close 프레임 응답 */
                send_frame(ws, WS_OPCODE_CLOSE, NULL, 0);
                return -1;

            case WS_OPCODE_PING:
                /* Pong 응답 */
                send_frame(ws, WS_OPCODE_PONG, &buf[pos], (size_t)payload_len);
                break;

            case WS_OPCODE_PONG:
                /* Pong 응답 수신 - on_message 콜백 호출하여 last_contact_ms 업데이트 */
                if (ws->on_message) {
                    ws->on_message("", 0, ws->user_data);  /* 빈 메시지로 콜백 호출 */
                }
                break;
        }

        /* 처리된 데이터 제거 */
        size_t frame_len = pos + (size_t)payload_len;
        if (ws->recv_len > frame_len) {
            memmove(ws->recv_buffer, &ws->recv_buffer[frame_len], ws->recv_len - frame_len);
            ws->recv_len -= frame_len;
        } else {
            ws->recv_len = 0;
        }
    }

    return 0;
}

/*===========================================================================
 * 공개 API
 *===========================================================================*/

int ws_client_init(ws_client_t* ws, const char* host, uint16_t port, const char* path)
{
    if (!ws || !host) return -1;

    memset(ws, 0, sizeof(ws_client_t));

    strncpy(ws->host, host, sizeof(ws->host) - 1);
    ws->port = port;
    strncpy(ws->path, path ? path : "/", sizeof(ws->path) - 1);

    ws->socket_fd = -1;
    ws->state = WS_STATE_DISCONNECTED;

    return 0;
}

void ws_client_cleanup(ws_client_t* ws)
{
    if (!ws) return;
    ws_client_disconnect(ws);
    memset(ws, 0, sizeof(ws_client_t));
    ws->socket_fd = -1;
}

int ws_client_connect(ws_client_t* ws, uint32_t timeout_ms)
{
    if (!ws) return -1;

    /* TCP 연결 */
    ws->socket_fd = sw_socket_tcp_create();
    if (ws->socket_fd < 0) {
        return -1;
    }

    if (sw_socket_connect(ws->socket_fd, ws->host, ws->port, timeout_ms) < 0) {
        sw_socket_close(ws->socket_fd);
        ws->socket_fd = -1;
        return -1;
    }

    ws->state = WS_STATE_CONNECTING;

    /* WebSocket 키 생성 */
    generate_websocket_key(ws->websocket_key, sizeof(ws->websocket_key));

    /* 핸드셰이크 전송 */
    if (send_handshake(ws) < 0) {
        sw_socket_close(ws->socket_fd);
        ws->socket_fd = -1;
        ws->state = WS_STATE_DISCONNECTED;
        return -1;
    }

    /* 핸드셰이크 응답 대기 */
    uint32_t start = sw_platform_millis();
    while (sw_platform_millis() - start < timeout_ms) {
        /* 데이터 수신 (논블로킹) */
        size_t space = WS_RECV_BUFFER_SIZE - ws->recv_len;
        if (space > 0) {
            int received = sw_socket_recv(ws->socket_fd, &ws->recv_buffer[ws->recv_len],
                                          (uint16_t)space, 10);  /* timeout=10ms */
            if (received > 0) {
                ws->recv_len += received;
            }
        }

        /* 핸드셰이크 파싱 */
        int result = parse_handshake_response(ws);
        if (result > 0) {
            /* 성공 */
            ws->state = WS_STATE_CONNECTED;
            if (ws->on_connected) {
                ws->on_connected(ws->user_data);
            }
            return 0;
        } else if (result < 0) {
            /* 실패 */
            ws_client_disconnect(ws);
            return -1;
        }

        sw_platform_delay(10);
    }

    /* 타임아웃 */
    ws_client_disconnect(ws);
    return -1;
}

int ws_client_connect_start(ws_client_t* ws)
{
    if (!ws) return -1;

    /* 기존 소켓 정리 */
    if (ws->socket_fd >= 0) {
        sw_socket_close(ws->socket_fd);
        ws->socket_fd = -1;
    }

    /* TCP 연결 시작 */
    ws->socket_fd = sw_socket_tcp_create();
    if (ws->socket_fd < 0) {
        return -1;
    }

    int ret = sw_socket_connect_start(ws->socket_fd, ws->host, ws->port);
    if (ret < 0) {
        sw_socket_close(ws->socket_fd);
        ws->socket_fd = -1;
        return -1;
    }

    if (ret == 0) {
        /* 즉시 연결 완료 - 핸드셰이크 전송 */
        ws->state = WS_STATE_CONNECTING;
        generate_websocket_key(ws->websocket_key, sizeof(ws->websocket_key));

        if (send_handshake(ws) < 0) {
            sw_socket_close(ws->socket_fd);
            ws->socket_fd = -1;
            ws->state = WS_STATE_DISCONNECTED;
            return -1;
        }
        return 0;  /* 핸드셰이크 응답 대기 중 */
    }

    /* TCP 연결 진행 중 */
    ws->state = WS_STATE_CONNECTING;
    return 1;
}

int ws_client_connect_check(ws_client_t* ws)
{
    if (!ws || ws->socket_fd < 0) return -1;

    if (ws->state == WS_STATE_DISCONNECTED) {
        return -1;
    }

    /* 핸드셰이크가 아직 전송되지 않았으면 TCP 연결 체크 */
    if (ws->websocket_key[0] == '\0') {
        int ret = sw_socket_connect_check(ws->socket_fd);
        if (ret < 0) {
            /* TCP 연결 실패 */
            ws_client_disconnect(ws);
            return -1;
        }
        if (ret == 1) {
            /* TCP 연결 진행 중 */
            return 1;
        }

        /* TCP 연결 완료 - 핸드셰이크 전송 */
        generate_websocket_key(ws->websocket_key, sizeof(ws->websocket_key));
        if (send_handshake(ws) < 0) {
            ws_client_disconnect(ws);
            return -1;
        }
        return 1;  /* 핸드셰이크 응답 대기 */
    }

    /* 핸드셰이크 응답 수신 시도 */
    size_t space = WS_RECV_BUFFER_SIZE - ws->recv_len;
    if (space > 0) {
        int received = sw_socket_recv(ws->socket_fd, &ws->recv_buffer[ws->recv_len],
                                      (uint16_t)space, 10);  /* timeout=10ms */
        if (received > 0) {
            ws->recv_len += received;
        }
    }

    /* 핸드셰이크 파싱 */
    int result = parse_handshake_response(ws);
    if (result > 0) {
        /* 성공 */
        printf("[WS] 핸드셰이크 성공\n");
        ws->state = WS_STATE_CONNECTED;
        if (ws->on_connected) {
            ws->on_connected(ws->user_data);
        }
        return 0;
    } else if (result < 0) {
        /* 실패 */
        printf("[WS] 핸드셰이크 실패\n");
        ws_client_disconnect(ws);
        return -1;
    }

    /* 아직 응답 대기 중 */
    return 1;
}

void ws_client_disconnect(ws_client_t* ws)
{
    if (!ws) return;

    bool was_connected = (ws->state == WS_STATE_CONNECTED);

    if (ws->socket_fd >= 0) {
        /* Close 프레임 전송 시도 */
        if (ws->state == WS_STATE_CONNECTED) {
            send_frame(ws, WS_OPCODE_CLOSE, NULL, 0);
        }
        sw_socket_close(ws->socket_fd);
        ws->socket_fd = -1;
    }

    ws->state = WS_STATE_DISCONNECTED;
    ws->handshake_complete = false;
    ws->recv_len = 0;
    ws->frame_pos = 0;
    ws->frame_started = false;

    if (was_connected && ws->on_disconnected) {
        ws->on_disconnected(ws->user_data);
    }
}

bool ws_client_is_connected(ws_client_t* ws)
{
    return ws && ws->state == WS_STATE_CONNECTED;
}

int ws_client_loop(ws_client_t* ws)
{
    if (!ws || ws->socket_fd < 0) return -1;

    if (ws->state != WS_STATE_CONNECTED) return 0;

    /* 데이터 수신 (논블로킹) */
    size_t space = WS_RECV_BUFFER_SIZE - ws->recv_len;
    if (space > 0) {
        int received = sw_socket_recv(ws->socket_fd, &ws->recv_buffer[ws->recv_len],
                                      (uint16_t)space, 0);  /* timeout=0 for non-blocking */
        if (received > 0) {
            ws->recv_len += received;
        }
        /* received == 0 또는 received < 0:
         * 논블로킹 소켓에서는 데이터 없음을 의미할 수 있음
         * 실제 연결 끊김은 OBS 클라이언트의 타임아웃으로 감지
         * (타임아웃: 30초 무응답)
         */
    }

    /* 프레임 처리 */
    if (ws->recv_len > 0) {
        if (process_frames(ws) < 0) {
            printf("[WS] process_frames 실패\n");
            ws_client_disconnect(ws);
            return -1;
        }
    }

    return 0;
}

int ws_client_send_text(ws_client_t* ws, const char* data, size_t len)
{
    if (!ws || ws->state != WS_STATE_CONNECTED) return -1;
    return send_frame(ws, WS_OPCODE_TEXT, (const uint8_t*)data, len);
}

int ws_client_send_ping(ws_client_t* ws)
{
    if (!ws || ws->state != WS_STATE_CONNECTED) return -1;
    /* Ping 프레임 (빈 페이로드) */
    return send_frame(ws, WS_OPCODE_PING, NULL, 0);
}

void ws_client_set_callbacks(ws_client_t* ws,
                             void (*on_connected)(void*),
                             void (*on_disconnected)(void*),
                             void (*on_message)(const char*, size_t, void*),
                             void* user_data)
{
    if (!ws) return;
    ws->on_connected = on_connected;
    ws->on_disconnected = on_disconnected;
    ws->on_message = on_message;
    ws->user_data = user_data;
}
