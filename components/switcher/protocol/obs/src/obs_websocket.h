/**
 * @file obs_websocket.h
 * @brief WebSocket 클라이언트 (순수 C)
 *
 * RFC 6455 기반 WebSocket 클라이언트
 * TCP 소켓 위에서 동작
 */

#ifndef OBS_WEBSOCKET_H
#define OBS_WEBSOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WebSocket 상태 */
typedef enum {
    WS_STATE_DISCONNECTED,
    WS_STATE_CONNECTING,
    WS_STATE_CONNECTED,
    WS_STATE_CLOSING
} ws_state_t;

/* WebSocket 프레임 opcode */
#define WS_OPCODE_CONTINUATION  0x0
#define WS_OPCODE_TEXT          0x1
#define WS_OPCODE_BINARY        0x2
#define WS_OPCODE_CLOSE         0x8
#define WS_OPCODE_PING          0x9
#define WS_OPCODE_PONG          0xA

/* 버퍼 크기 (ESP32 최적화) */
#define WS_RECV_BUFFER_SIZE     2048    /* 수신 버퍼: 2KB (OBS JSON 충분) */
#define WS_FRAME_BUFFER_SIZE    1024    /* 분할 메시지용: 1KB (거의 사용 안됨) */
#define WS_SEND_BUFFER_SIZE     512     /* 송신 버퍼: 512B (명령은 작음) */
#define WS_MAX_HEADER_SIZE      256

/* WebSocket 클라이언트 구조체 */
typedef struct {
    int socket_fd;
    ws_state_t state;

    char host[64];
    uint16_t port;
    char path[64];

    /* 수신 버퍼 */
    uint8_t recv_buffer[WS_RECV_BUFFER_SIZE];
    size_t recv_pos;
    size_t recv_len;

    /* 프레임 조립 (분할 메시지용) */
    uint8_t frame_buffer[WS_FRAME_BUFFER_SIZE];
    size_t frame_pos;
    bool frame_started;

    /* 전송 버퍼 */
    uint8_t send_buffer[WS_SEND_BUFFER_SIZE];

    /* HTTP 핸드셰이크 */
    char websocket_key[32];
    bool handshake_complete;

    /* 콜백 */
    void (*on_connected)(void* user_data);
    void (*on_disconnected)(void* user_data);
    void (*on_message)(const char* data, size_t len, void* user_data);
    void* user_data;

} ws_client_t;

/**
 * @brief WebSocket 클라이언트 초기화
 */
int ws_client_init(ws_client_t* ws, const char* host, uint16_t port, const char* path);

/**
 * @brief WebSocket 연결 정리
 */
void ws_client_cleanup(ws_client_t* ws);

/**
 * @brief WebSocket 연결 시도
 * @param timeout_ms 타임아웃 (ms)
 * @return 0 성공, -1 실패
 */
int ws_client_connect(ws_client_t* ws, uint32_t timeout_ms);

/**
 * @brief WebSocket 연결 시작 (논블로킹)
 * @return 0 성공, 1 진행중, -1 실패
 */
int ws_client_connect_start(ws_client_t* ws);

/**
 * @brief WebSocket 연결 상태 확인 (논블로킹)
 * @return 0 연결 완료, 1 진행중, -1 실패
 */
int ws_client_connect_check(ws_client_t* ws);

/**
 * @brief WebSocket 연결 해제
 */
void ws_client_disconnect(ws_client_t* ws);

/**
 * @brief 연결 상태 확인
 */
bool ws_client_is_connected(ws_client_t* ws);

/**
 * @brief 메인 루프 (수신 처리)
 * @return 0 정상, -1 오류
 */
int ws_client_loop(ws_client_t* ws);

/**
 * @brief 텍스트 메시지 전송
 */
int ws_client_send_text(ws_client_t* ws, const char* data, size_t len);

/**
 * @brief WebSocket Ping 프레임 전송 (연결 유지용)
 * @return 0 성공, -1 실패
 */
int ws_client_send_ping(ws_client_t* ws);

/**
 * @brief 콜백 설정
 */
void ws_client_set_callbacks(ws_client_t* ws,
                             void (*on_connected)(void*),
                             void (*on_disconnected)(void*),
                             void (*on_message)(const char*, size_t, void*),
                             void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* OBS_WEBSOCKET_H */
