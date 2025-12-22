/**
 * @file DisplayBuffer.h
 * @brief U8g2 정적 버퍼 관리
 *
 * 동적 메모리 할당을 제거하고 정적 버퍼를 사용하여 메모리 안정성 확보
 */

#ifndef DISPLAY_BUFFER_H
#define DISPLAY_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// SSD1306 128x64 버퍼 크기 계산
// tile_width = 16 (128/8), tile_height = 8, 각 타일은 8 bytes
#define DISPLAY_BUFFER_SIZE (16 * 8 * 8)  // 1024 bytes

// 듀얼 버퍼링을 위한 2개 버퍼
#define DISPLAY_BUFFER_COUNT 2

// 버퍼 상태
typedef enum {
    BUFFER_STATE_FREE = 0,
    BUFFER_STATE_DRAWING,
    BUFFER_STATE_SENDING
} buffer_state_t;

// 정적 버퍼 관리 구조체
typedef struct {
    uint8_t buffers[DISPLAY_BUFFER_COUNT][DISPLAY_BUFFER_SIZE];
    buffer_state_t states[DISPLAY_BUFFER_COUNT];
    uint8_t current_draw;   // 현재 드로잉 버퍼 인덱스
    uint8_t current_send;   // 현재 전송 버퍼 인덱스
    bool sending;           // I2C 전송 중 여부
} DisplayBufferManager_t;

// 버퍼 관리자 함수
esp_err_t DisplayBuffer_init(void);
uint8_t* DisplayBuffer_getDrawBuffer(void);
uint8_t* DisplayBuffer_getSendBuffer(void);
bool DisplayBuffer_isReadyToSend(void);
void DisplayBuffer_swapBuffers(void);
void DisplayBuffer_markSendingComplete(void);

// 버퍼 내용 비교
bool DisplayBuffer_isChanged(uint8_t* buf1, uint8_t* buf2);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_BUFFER_H