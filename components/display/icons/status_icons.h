/**
 * @file status_icons.h
 * @brief 상태 아이콘 정의 (배터리, 안테나 신호 등)
 */

#ifndef STATUS_ICONS_H
#define STATUS_ICONS_H

#include <stdint.h>

// 아이콘 너비와 높이 정의
#define ICON_WIDTH 16
#define ICON_HEIGHT 12

// 배터리 아이콘 (5단계: 100%, 75%, 50%, 25%, 저전력)
extern const uint8_t icon_battery_100[];
extern const uint8_t icon_battery_75[];
extern const uint8_t icon_battery_50[];
extern const uint8_t icon_battery_25[];
extern const uint8_t icon_battery_low[];

// 안테나 신호 아이콘 (4단계: 강함, 중간, 약함, 없음)
extern const uint8_t icon_signal_strong[];
extern const uint8_t icon_signal_medium[];
extern const uint8_t icon_signal_weak[];
extern const uint8_t icon_signal_none[];

// 아이콘 타입 정의
typedef enum {
    BATTERY_100 = 0,
    BATTERY_75,
    BATTERY_50,
    BATTERY_25,
    BATTERY_LOW,
    SIGNAL_STRONG,
    SIGNAL_MEDIUM,
    SIGNAL_WEAK,
    SIGNAL_NONE,
    ICON_COUNT
} icon_type_t;

#endif // STATUS_ICONS_H