/**
 * @file icons.h
 * @brief TALLY-NODE 스타일 아이콘 함수 선언
 */

#ifndef TALLY_ICONS_H
#define TALLY_ICONS_H

#include <stdint.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 아이콘 레벨 상수 */
#define BATTERY_LEVEL_EMPTY    0
#define BATTERY_LEVEL_LOW      1
#define BATTERY_LEVEL_MEDIUM   2
#define BATTERY_LEVEL_FULL     3

#define SIGNAL_LEVEL_NONE      0
#define SIGNAL_LEVEL_WEAK      1
#define SIGNAL_LEVEL_MEDIUM    2
#define SIGNAL_LEVEL_STRONG    3

/**
 * @brief 배터리 아이콘 그리기
 * @param u8g2 U8G2 디스플레이 포인터
 * @param x X 좌표
 * @param y Y 좌표
 * @param level 배터리 레벨 (0-3)
 */
void drawTallyBatteryIcon(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t level);

/**
 * @brief 배터리 퍼센트를 레벨로 변환
 * @param percentage 배터리 퍼센트 (0-100)
 * @return 배터리 레벨 (0-3)
 */
uint8_t getBatteryLevel(uint8_t percentage);

/**
 * @brief RSSI 값을 신호 레벨로 변환
 * @param rssi RSSI 값 (dBm)
 * @return 신호 레벨 (0-3)
 */
uint8_t getSignalLevelFromRSSI(int16_t rssi);

/**
 * @brief RSSI와 SNR을 고려한 신호 레벨 계산
 * @param rssi RSSI 값 (dBm)
 * @param snr SNR 값 (dB)
 * @return 신호 레벨 (0-3)
 */
uint8_t getSignalLevel(int16_t rssi, float snr);

/**
 * @brief 안테나 신호 아이콘 그리기
 * @param u8g2 U8G2 디스플레이 포인터
 * @param x X 좌표
 * @param y Y 좌표
 * @param rssi RSSI 값 (dBm)
 * @param snr SNR 값 (dB)
 */
void drawTallySignalIcon(u8g2_t *u8g2, int16_t x, int16_t y, int16_t rssi, float snr);

/**
 * @brief 체크마크 그리기 (8x8 픽셀)
 * @param u8g2 U8G2 디스플레이 포인터
 * @param x X 좌표
 * @param y Y 좌표
 */
void drawCheckMark(u8g2_t *u8g2, int16_t x, int16_t y);

/**
 * @brief 엑스마크 그리기 (8x8 픽셀)
 * @param u8g2 U8G2 디스플레이 포인터
 * @param x X 좌표
 * @param y Y 좌표
 */
void drawXMark(u8g2_t *u8g2, int16_t x, int16_t y);

#ifdef __cplusplus
}
#endif

#endif // TALLY_ICONS_H
