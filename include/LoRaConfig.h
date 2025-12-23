/**
 * @file LoRaConfig.h
 * @brief LoRa 기본 설정값
 *
 * E22 모듈 사양:
 * - E22-400MM22S: SX1268, 410~493 MHz (433MHz 대역)
 * - E22-900MM22S: SX1262, 850~930 MHz (868/915MHz 대역)
 */

#pragma once

#include <stdint.h>

// ============================================================================
// LoRa 기본 설정
// ============================================================================

// 주파수 (MHz)
#define LORA_DEFAULT_FREQ_400   433.0f    // 433MHz (E22-400MM22S)
#define LORA_DEFAULT_FREQ_900   868.0f    // 868MHz (E22-900MM22S)
#define LORA_DEFAULT_FREQ       LORA_DEFAULT_FREQ_900  // 기본값

// 칩 타입 이름 (표시용)
#define LORA_CHIP_400_NAME      "SX1268 (433MHz)"
#define LORA_CHIP_900_NAME      "SX1262 (868MHz)"

// Spreading Factor (SF)
// SF7=빠름/짧은거리, SF12=느림/긴거리
#define LORA_DEFAULT_SF         7         // SF7 (가장 일반적)

// Coding Rate (CR)
// 5=4/5, 6=4/6, 7=4/7, 8=4/8
#define LORA_DEFAULT_CR         7         // CR 4/7

// Bandwidth (kHz)
// 125, 250, 500 중 선택
// 125kHz: 감도 좋음, 250kHz: 속도 빠름
#define LORA_DEFAULT_BW         125.0f    // 125kHz (장거리용)

// 송신 전력 (dBm)
// 최대 +22dBm (E22 모듈)
#define LORA_DEFAULT_TX_POWER   22

// Sync Word
// 0x12: 공용, 0x14: LoRaWAN
#define LORA_DEFAULT_SYNC_WORD  0x12

// 프리앰블 길이
#define LORA_DEFAULT_PREAMBLE   8

// ============================================================================
// SF별 전송 시간 (단위: ms, 125kHz 기준)
// ============================================================================
// SF7:  ~44ms (11 bytes)
// SF8:  ~79ms
// SF9:  ~142ms
// SF10: ~253ms
// SF11: ~455ms
// SF12: ~818ms
