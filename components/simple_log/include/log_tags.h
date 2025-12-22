/**
 * @file log_tags.h
 * @brief 로그 태그 중앙 관리
 *
 * 모든 컴포넌트의 로그 태그를 중앙에서 정의하고 관리합니다.
 * 태그는 최대 10자 이내로 유지합니다.
 */

#ifndef LOG_TAGS_H
#define LOG_TAGS_H

// ========================================
// System
// ========================================
#define TAG_MAIN          "MAIN"          // 메인 시스템
#define TAG_CONFIG        "CONFIG"        // 설정 관리
#define TAG_MONITOR       "MONITOR"       // 시스템 모니터
#define TAG_PLATFORM      "PLATFORM"      // 플랫폼
#define TAG_BUTTON        "BUTTON"        // 버튼 폴링
#define TAG_INFO          "INFO"          // 정보 관리

// ========================================
// Network
// ========================================
#define TAG_NETWORK       "NETWORK"       // 네트워크 전체
#define TAG_WIFI          "WIFI"          // WiFi
#define TAG_ETHERNET      "ETHERNET"      // 이더넷

// ========================================
// Switcher
// ========================================
#define TAG_SWITCHER      "SWITCHER"      // 스위처 시스템
#define TAG_OBS           "OBS"           // OBS 프로토콜
#define TAG_ATEM          "ATEM"          // ATEM 프로토콜
#define TAG_VMIX          "VMIX"          // vMix 프로토콜

// ========================================
// LoRa
// ========================================
#define TAG_LORA          "LORA"          // LoRa 전체
#define TAG_COMM          "COMM"          // 통신 관리

// ========================================
// Interface
// ========================================
#define TAG_CLI           "CLI"           // CLI 인터페이스
#define TAG_WEB           "WEB"           // 웹서버
#define TAG_API           "API"           // API 핸들러

// ========================================
// Display
// ========================================
#define TAG_DISPLAY       "DISPLAY"       // 디스플레이

// ========================================
// Debug & Test
// ========================================
#define TAG_TEST          "TEST"          // 테스트 코드
#define TAG_ERROR         "ERROR"         // 에러 메시지

#endif // LOG_TAGS_H
