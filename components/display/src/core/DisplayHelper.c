/**
 * @file DisplayHelper.c
 * @brief Display Helper Functions Implementation
 *
 * 페이지들에서 공통으로 사용하는 디스플레이 헬퍼 함수 구현
 */

#include "core/DisplayHelper.h"
#include "core/DisplayManager.h"

u8g2_t* DisplayHelper_getU8g2(void)
{
    return DisplayManager_getU8g2();
}

void DisplayHelper_clearBuffer(void)
{
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (u8g2) {
        u8g2_ClearBuffer(u8g2);
    }
}

void DisplayHelper_sendBuffer(void)
{
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (u8g2) {
        u8g2_SendBuffer(u8g2);
    }
}

void DisplayHelper_setPower(bool on)
{
    u8g2_t* u8g2 = DisplayHelper_getU8g2();
    if (u8g2) {
        u8g2_SetPowerSave(u8g2, on ? 0 : 1);
    }
}