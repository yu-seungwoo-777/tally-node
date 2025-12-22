/**
 * @file DisplayBuffer.c
 * @brief U8g2 정적 버퍼 관리 구현
 */

#include "buffer/DisplayBuffer.h"
#include "esp_err.h"
#include "string.h"
#include "log.h"
#include "log_tags.h"

static const char* TAG = "DBUF";

// 정적 버퍼 관리자 인스턴스
static DisplayBufferManager_t s_buffer_manager = {0};

esp_err_t DisplayBuffer_init(void)
{
    // 모든 버퍼 초기화
    memset(s_buffer_manager.buffers, 0, sizeof(s_buffer_manager.buffers));

    // 버퍼 상태 초기화
    for (int i = 0; i < DISPLAY_BUFFER_COUNT; i++) {
        s_buffer_manager.states[i] = BUFFER_STATE_FREE;
    }

    s_buffer_manager.current_draw = 0;
    s_buffer_manager.current_send = 1;
    s_buffer_manager.sending = false;

    LOG_0(TAG, "Display buffer initialized: %d x %d bytes",
           DISPLAY_BUFFER_COUNT, DISPLAY_BUFFER_SIZE);

    return ESP_OK;
}

uint8_t* DisplayBuffer_getDrawBuffer(void)
{
    uint8_t idx = s_buffer_manager.current_draw;
    s_buffer_manager.states[idx] = BUFFER_STATE_DRAWING;
    return s_buffer_manager.buffers[idx];
}

uint8_t* DisplayBuffer_getSendBuffer(void)
{
    if (!s_buffer_manager.sending) {
        uint8_t idx = s_buffer_manager.current_send;
        s_buffer_manager.states[idx] = BUFFER_STATE_SENDING;
        s_buffer_manager.sending = true;
        return s_buffer_manager.buffers[idx];
    }
    return NULL;
}

bool DisplayBuffer_isReadyToSend(void)
{
    // 전송 중이 아니고 버퍼 내용이 변경된 경우
    uint8_t* draw_buf = s_buffer_manager.buffers[s_buffer_manager.current_draw];
    uint8_t* send_buf = s_buffer_manager.buffers[s_buffer_manager.current_send];

    return !s_buffer_manager.sending &&
           DisplayBuffer_isChanged(draw_buf, send_buf);
}

void DisplayBuffer_swapBuffers(void)
{
    if (!s_buffer_manager.sending) {
        // 버퍼 인덱스 교환
        uint8_t temp = s_buffer_manager.current_draw;
        s_buffer_manager.current_draw = s_buffer_manager.current_send;
        s_buffer_manager.current_send = temp;

        // 상태 업데이트
        s_buffer_manager.states[s_buffer_manager.current_draw] = BUFFER_STATE_DRAWING;
        s_buffer_manager.states[s_buffer_manager.current_send] = BUFFER_STATE_FREE;

        LOG_1(TAG, "Buffers swapped: draw=%d, send=%d",
              s_buffer_manager.current_draw, s_buffer_manager.current_send);
    }
}

void DisplayBuffer_markSendingComplete(void)
{
    s_buffer_manager.sending = false;
    if (s_buffer_manager.current_send < DISPLAY_BUFFER_COUNT) {
        s_buffer_manager.states[s_buffer_manager.current_send] = BUFFER_STATE_FREE;
    }
}

bool DisplayBuffer_isChanged(uint8_t* buf1, uint8_t* buf2)
{
    // 버퍼 내용 비교
    return memcmp(buf1, buf2, DISPLAY_BUFFER_SIZE) != 0;
}