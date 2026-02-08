/**
 * @file icons.c
 * @brief 상태 아이콘 구현
 *
 * 배터리 및 안테나 신호 아이콘
 * 비트맵이 아닌 동적 드로잉 방식 사용
 */

#include "icons.h"
#include "u8g2.h"

/**
 * @brief 배터리 아이콘 그리기
 */
void drawTallyBatteryIcon(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t level)
{
    // 배터리 외곽선 (20x8)
    u8g2_DrawFrame(u8g2, x, y, 20, 8);

    // 양극 (+)
    u8g2_DrawBox(u8g2, x + 20, y + 2, 2, 4);

    // 배터리 레벨 (3칸)
    // 각 칸: 4x4 픽셀, 간격: 1픽셀
    if (level >= 1) u8g2_DrawBox(u8g2, x + 3, y + 2, 4, 4);
    if (level >= 2) u8g2_DrawBox(u8g2, x + 8, y + 2, 4, 4);
    if (level >= 3) u8g2_DrawBox(u8g2, x + 13, y + 2, 4, 4);
}

/**
 * @brief 배터리 퍼센트를 레벨로 변환
 *
 * 레벨 기준 (보수적 설정, 사용자에게 넉넉하게 표시):
 * - 0%  ~ 10%:  EMPTY (0칸) - 거의 소진
 * - 11% ~ 40%:  LOW   (1칸) - 3.5V~3.7V
 * - 41% ~ 75%:  MEDIUM(2칸) - 3.7V~3.95V
 * - 76% ~ 100%: FULL  (3칸) - 3.95V 이상
 *
 * @note 딥슬립은 3.2V 미만 (0%)에서 트리거됨
 */
uint8_t getBatteryLevel(uint8_t percentage)
{
    if (percentage <= 10) return BATTERY_LEVEL_EMPTY;
    if (percentage <= 40) return BATTERY_LEVEL_LOW;
    if (percentage <= 75) return BATTERY_LEVEL_MEDIUM;
    return BATTERY_LEVEL_FULL;
}

/**
 * @brief RSSI 값을 신호 레벨로 변환
 */
uint8_t getSignalLevelFromRSSI(int16_t rssi)
{
    if (rssi > -70) return SIGNAL_LEVEL_STRONG;
    if (rssi > -85) return SIGNAL_LEVEL_MEDIUM;
    if (rssi > -100) return SIGNAL_LEVEL_WEAK;
    return SIGNAL_LEVEL_NONE;
}

/**
 * @brief RSSI와 SNR을 고려한 신호 레벨 계산
 */
uint8_t getSignalLevel(int16_t rssi, float snr)
{
    // SNR 정보가 없는 경우 (-999), 기존 RSSI 기반 로직 사용
    if (snr <= -999.0f) {
        return getSignalLevelFromRSSI(rssi);
    }

    // RSSI와 SNR을 함께 고려한 신호 품질 평가
    if (rssi > -70 && snr > 5.0f) {
        return SIGNAL_LEVEL_STRONG;
    } else if (rssi > -85 && snr > 0.0f) {
        return SIGNAL_LEVEL_MEDIUM;
    } else if (rssi > -100 && snr > -5.0f) {
        return SIGNAL_LEVEL_WEAK;
    } else {
        return SIGNAL_LEVEL_NONE;
    }
}

/**
 * @brief 안테나 신호 아이콘 그리기
 */
void drawTallySignalIcon(u8g2_t *u8g2, int16_t x, int16_t y, int16_t rssi, float snr)
{
    // RSSI와 SNR을 고려한 신호 레벨 계산
    int signalLevel = getSignalLevel(rssi, snr);

    // 디버그: 신호 레벨 로그
    // printf("Signal: RSSI=%d SNR=%.1f Level=%d\n", rssi, snr, signalLevel);

    // 안테나 굵은 T자 모양
    const int antennaBaseX = x - 5;
    const int antennaBaseY = y;

    // 안테나 수직 기둥 (3픽셀 너비)
    u8g2_DrawVLine(u8g2, antennaBaseX, antennaBaseY, 8);
    u8g2_DrawVLine(u8g2, antennaBaseX + 1, antennaBaseY, 8);
    u8g2_DrawVLine(u8g2, antennaBaseX + 2, antennaBaseY, 8);

    // 안테나 수평 막대 (3픽셀 높이)
    u8g2_DrawHLine(u8g2, antennaBaseX - 2, antennaBaseY, 7);
    u8g2_DrawHLine(u8g2, antennaBaseX - 2, antennaBaseY + 1, 7);
    u8g2_DrawHLine(u8g2, antennaBaseX - 2, antennaBaseY + 2, 7);

    // 신호 막대 그리기
    const int barWidth = 3;
    const int barGap = 1;

    // 첫 번째 막대 (높이 3픽셀)
    if (signalLevel >= 1) {
        u8g2_DrawBox(u8g2, x, y + 5, barWidth, 3);
    } else {
        u8g2_DrawFrame(u8g2, x, y + 5, barWidth, 3);
    }

    // 두 번째 막대 (높이 6픽셀)
    if (signalLevel >= 2) {
        u8g2_DrawBox(u8g2, x + barWidth + barGap, y + 2, barWidth, 6);
    } else {
        u8g2_DrawFrame(u8g2, x + barWidth + barGap, y + 2, barWidth, 6);
    }

    // 세 번째 막대 (높이 8픽셀)
    if (signalLevel >= 3) {
        u8g2_DrawBox(u8g2, x + (barWidth + barGap) * 2, y, barWidth, 8);
    } else {
        u8g2_DrawFrame(u8g2, x + (barWidth + barGap) * 2, y, barWidth, 8);
    }
}

/**
 * @brief 체크마크 그리기 (8x8 픽셀 영역)
 *
 *     ✓
 *    /
 *   /
 *  /
 * /
 *
 * @param u8g2 U8G2 디스플레이 포인터
 * @param x X 좌표 (왼쪽 상단)
 * @param y Y 좌표 (왼쪽 상단)
 */
void drawCheckMark(u8g2_t *u8g2, int16_t x, int16_t y)
{
    // 체크마크: 왼쪽 아래에서 오른쪽 위로 대각선
    // (x+1, y+7) -> (x+3, y+5) -> (x+7, y+1)

    // 첫 번째 선: 왼쪽 아래에서 중간까지
    u8g2_DrawLine(u8g2, x + 1, y + 7, x + 3, y + 5);

    // 두 번째 선: 중간에서 오른쪽 위까지
    u8g2_DrawLine(u8g2, x + 3, y + 5, x + 7, y + 1);
}

/**
 * @brief 엑스마크 그리기 (8x8 픽셀 영역)
 *
 *     ✗
 *    \ /
 *     X
 *    / \
 *
 * @param u8g2 U8G2 디스플레이 포인터
 * @param x X 좌표 (왼쪽 상단)
 * @param y Y 좌표 (왼쪽 상단)
 */
void drawXMark(u8g2_t *u8g2, int16_t x, int16_t y)
{
    // 첫 번째 대각선: 왼쪽 위에서 오른쪽 아래로
    u8g2_DrawLine(u8g2, x + 1, y + 1, x + 7, y + 7);

    // 두 번째 대각선: 오른쪽 위에서 왼쪽 아래로
    u8g2_DrawLine(u8g2, x + 7, y + 1, x + 1, y + 7);
}
