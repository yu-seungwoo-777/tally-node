/**
 * @file icon_renderer.h
 * @brief 아이콘 렌더링 함수 정의
 */

#ifndef ICON_RENDERER_H
#define ICON_RENDERER_H

#include "u8g2.h"
#include "status_icons.h"

// 아이콘 렌더링 함수 (U8g2용)
void icon_draw_u8g2(u8g2_t *u8g2, icon_type_t icon_type, uint8_t x, uint8_t y);
void icon_draw_battery_u8g2(u8g2_t *u8g2, uint8_t percentage, uint8_t x, uint8_t y);
void icon_draw_signal_u8g2(u8g2_t *u8g2, int8_t rssi, uint8_t x, uint8_t y);

#endif // ICON_RENDERER_H