/**
 * @file lora_core_c.cpp
 * @brief LoRaCore C 래퍼 구현
 *
 * C 파일에서 C++ LoRaCore 클래스를 사용하기 위한 래퍼 함수
 * - 이전 버전과의 호환성 유지
 */

#include "LoRaCore.h"
#include "LoRaTypes.h"

extern "C" {

// C 래퍼 함수 - lora_status_t 사용
lora_status_t getLoRaStatus(void) {
    return LoRaCore::getStatus();
}

} // extern "C"