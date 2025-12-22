/**
 * @file icons.c
 * @brief 상태 아이콘 구현
 *
 * 배터리 및 안테나 신호 아이콘 구현
 * 비트맵이 아닌 동적 드로잉 방식 사용
 */

#include "ui/icons.h"
#include "u8g2.h"

/**
 * @brief 배터리 아이콘 그리기
 *
 * @param u8g2 U8G2 디스플레이 포인터
 * @param x X 좌표
 * @param y Y 좌표
 * @param level 배터리 레벨 (0-3)
 */
void drawTallyBatteryIcon(u8g2_t *u8g2, int16_t x, int16_t y, uint8_t level) {
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
 * @param percentage 배터리 퍼센트 (0-100)
 * @return 배터리 레벨 (0-3)
 */
uint8_t getBatteryLevel(uint8_t percentage) {
    if (percentage <= 25) return BATTERY_LEVEL_EMPTY;
    if (percentage <= 50) return BATTERY_LEVEL_LOW;
    if (percentage <= 75) return BATTERY_LEVEL_MEDIUM;
    return BATTERY_LEVEL_FULL;
}

/**
 * @brief RSSI 값을 신호 레벨로 변환 (기존 호환성)
 *
 * @param rssi RSSI 값 (dBm)
 * @return 신호 레벨 (0-3)
 */
uint8_t getSignalLevelFromRSSI(int16_t rssi) {
    if (rssi > -70) return SIGNAL_LEVEL_STRONG;
    if (rssi > -85) return SIGNAL_LEVEL_MEDIUM;
    if (rssi > -100) return SIGNAL_LEVEL_WEAK;
    return SIGNAL_LEVEL_NONE;
}

/**
 * @brief RSSI와 SNR을 고려한 신호 레벨 계산
 *
 * @param rssi RSSI 값 (dBm)
 * @param snr SNR 값 (dB)
 * @return 신호 레벨 (0-3)
 */
uint8_t getSignalLevel(int16_t rssi, float snr) {
    // SNR 정보가 없는 경우 (-999), 기존 RSSI 기반 로직 사용
    if (snr <= -999.0f) {
        return getSignalLevelFromRSSI(rssi);
    }

    // RSSI와 SNR을 함께 고려한 정확한 신호 품질 평가
    if (rssi > -70 && snr > 5.0f) {
        return SIGNAL_LEVEL_STRONG;   // 강한 신호, 양호한 SNR
    } else if (rssi > -85 && snr > 0.0f) {
        return SIGNAL_LEVEL_MEDIUM;   // 보통 신호, 양호한 SNR
    } else if (rssi > -100 && snr > -5.0f) {
        return SIGNAL_LEVEL_WEAK;     // 약한 신호, 낮은 SNR
    } else {
        return SIGNAL_LEVEL_NONE;     // 신호 없음 또는 사용 불가
    }
}

/**
 * @brief 안테나 신호 아이콘 그리기 (SNR 지원)
 *
 * @param u8g2 U8G2 디스플레이 포인터
 * @param x X 좌표
 * @param y Y 좌표
 * @param rssi RSSI 값 (dBm)
 * @param snr SNR 값 (dB), 기본값은 -999로 사용되지 않음을 표시
 */
void drawTallySignalIcon(u8g2_t *u8g2, int16_t x, int16_t y, int16_t rssi, float snr) {
    // RSSI와 SNR을 고려한 신호 레벨 계산
    int signalLevel = getSignalLevel(rssi, snr);

    // 안테나 굵은 T자 모양으로 그리기 (균형 개선)
    const int antennaBaseX = x - 5;  // 안테나 위치
    const int antennaBaseY = y;

    // 안테나 수직 기둥 (3픽셀 너비)
    u8g2_DrawVLine(u8g2, antennaBaseX, antennaBaseY, 8);
    u8g2_DrawVLine(u8g2, antennaBaseX + 1, antennaBaseY, 8);
    u8g2_DrawVLine(u8g2, antennaBaseX + 2, antennaBaseY, 8);

    // 안테나 수평 막대 (3픽셀 높이로 균형 개선)
    u8g2_DrawHLine(u8g2, antennaBaseX - 2, antennaBaseY, 7);  // 중심 맞춤, 7픽셀 길이
    u8g2_DrawHLine(u8g2, antennaBaseX - 2, antennaBaseY + 1, 7);
    u8g2_DrawHLine(u8g2, antennaBaseX - 2, antennaBaseY + 2, 7);

    // 신호 막대 그리기 (점진적 채우기 방식)
    // 막대 너비: 3픽셀, 간격: 1픽셀
    // 총 높이: 8픽셀 (배터리 아이콘과 동일)
    const int barWidth = 3;
    const int barGap = 1;

    // 첫 번째 막대 (가장 낮음) - 높이 3픽셀
    if (signalLevel >= 1) {
        u8g2_DrawBox(u8g2, x, y + 5, barWidth, 3);  // 채워진 막대
    } else {
        u8g2_DrawFrame(u8g2, x, y + 5, barWidth, 3);  // 테두리만
    }

    // 두 번째 막대 (중간) - 높이 6픽셀
    if (signalLevel >= 2) {
        u8g2_DrawBox(u8g2, x + barWidth + barGap, y + 2, barWidth, 6);  // 채워진 막대
    } else {
        u8g2_DrawFrame(u8g2, x + barWidth + barGap, y + 2, barWidth, 6);  // 테두리만
    }

    // 세 번째 막대 (가장 높음) - 높이 8픽셀
    if (signalLevel >= 3) {
        u8g2_DrawBox(u8g2, x + (barWidth + barGap) * 2, y, barWidth, 8);  // 채워진 막대
    } else {
        u8g2_DrawFrame(u8g2, x + (barWidth + barGap) * 2, y, barWidth, 8);  // 테두리만
    }
}