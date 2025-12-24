/**
 * @file SwitcherConfig.h
 * @brief Switcher (ATEM/OBS/vMix) 기본 설정값
 *
 * @note 이 파일은 스위처 프로토콜 및 Tally 설정입니다.
 *
 * @section 설정 항목
 *   - 스위처 타입/역할 (ATEM/OBS/vMix, Primary/Secondary)
 *   - ATEM/OBS/vMix 프로토콜 타임아웃 및 포트
 *   - Tally 채널/상태 설정
 *   - 듀얼모드 오프셋 설정
 *   - Primary/Secondary 스위처 기본값
 *   - NVS 키 이름 (설정 저장용)
 */

#pragma once

#include <stdint.h>

// ============================================================================
// 스위처 타입
// ============================================================================

#define SWITCHER_TYPE_ATEM      0    ///< Blackmagic ATEM
#define SWITCHER_TYPE_OBS       1    ///< OBS Studio
#define SWITCHER_TYPE_VMIX      2    ///< vMix

// ============================================================================
// ATEM 프로토콜 기본 설정
// ============================================================================

// 포트 번호
#define ATEM_DEFAULT_PORT       9910     ///< ATEM UDP 포트

// 패킷 크기
#define ATEM_HELLO_PACKET_SIZE  20       ///< Hello 패킷 크기 (정확히 20바이트)
#define ATEM_ACK_PACKET_SIZE    12       ///< ACK 패킷 크기 (정확히 12바이트)
#define ATEM_MAX_PACKET_SIZE    1500     ///< 최대 패킷 크기 (MTU)

// 타임아웃 설정
#define ATEM_CONNECT_TIMEOUT_MS         5000    ///< 연결 타임아웃 (5초)
#define ATEM_HELLO_RESPONSE_TIMEOUT_MS  10000   ///< Hello 응답 타임아웃 (10초)
#define ATEM_MAX_SILENCE_TIME_MS        5000    ///< 최대 무응답 시간 (5초)
#define ATEM_KEEPALIVE_INTERVAL_MS      1000    ///< Keepalive 간격 (1초)

// 패킷 플래그
#define ATEM_FLAG_ACK_REQUEST       0x01    ///< 수신 확인 요청
#define ATEM_FLAG_HELLO             0x02    ///< Hello 패킷
#define ATEM_FLAG_RESEND            0x04    ///< 재전송 패킷
#define ATEM_FLAG_REQUEST_RESEND    0x08    ///< 재전송 요청
#define ATEM_FLAG_ACK               0x10    ///< 수신 확인

// ============================================================================
// OBS 프로토콜 기본 설정
// ============================================================================

#define OBS_DEFAULT_PORT        4455     ///< OBS WebSocket 포트
#define OBS_CONNECT_TIMEOUT_MS  5000     ///< 연결 타임아웃 (5초)

// ============================================================================
// vMix 프로토콜 기본 설정
// ============================================================================

#define VMIX_DEFAULT_PORT       8099     ///< vMix TCP 포트
#define VMIX_CONNECT_TIMEOUT_MS  5000     ///< 연결 타임아웃 (5초)

// ============================================================================
// Tally 공통 설정
// ============================================================================

#define TALLY_MAX_CHANNELS      20       ///< 최대 Tally 채널 수
// Tally 상태 (tally_status_t enum)는 TallyTypes.h를 참고하세요

// ============================================================================
// 듀얼모드 설정
// ============================================================================

// 오프셋 범위
#define SWITCHER_MIN_OFFSET    0    ///< 최소 오프셋
#define SWITCHER_MAX_OFFSET    19   ///< 최대 오프셋 (20채널 - 1)
#define SWITCHER_DEFAULT_OFFSET 1    ///< Secondary 기본 오프셋

// ============================================================================
// 스위처 공통 설정
// ============================================================================

#define SWITCHER_RETRY_INTERVAL_MS   5000    ///< 스위처 재연결 시도 간격 (5초)

// ============================================================================
// 기본 설정값
// ============================================================================

// Primary 스위처 기본값
#define SWITCHER_PRIMARY_TYPE           SWITCHER_TYPE_ATEM  ///< Primary 기본 타입
#define SWITCHER_PRIMARY_IP             "192.168.0.240"    ///< Primary 기본 IP
#define SWITCHER_PRIMARY_PORT           0                  ///< Primary 기본 포트 (0=기본값 사용)
#define SWITCHER_PRIMARY_PASSWORD       ""                 ///< Primary 기본 비밀번호
#define SWITCHER_PRIMARY_CAMERA_LIMIT   0                  ///< Primary 카메라 제한 (0=자동)
#define SWITCHER_PRIMARY_INTERFACE      2                  ///< Primary 네트워크 인터페이스 (1=WiFi, 2=Ethernet)

// Secondary 스위처 기본값
#define SWITCHER_SECONDARY_TYPE         SWITCHER_TYPE_ATEM  ///< Secondary 기본 타입
#define SWITCHER_SECONDARY_IP           "192.168.0.244"    ///< Secondary 기본 IP
#define SWITCHER_SECONDARY_PORT         0                  ///< Secondary 기본 포트 (0=기본값 사용)
#define SWITCHER_SECONDARY_PASSWORD     ""                 ///< Secondary 기본 비밀번호
#define SWITCHER_SECONDARY_CAMERA_LIMIT 0                  ///< Secondary 카메라 제한 (0=자동)
#define SWITCHER_SECONDARY_INTERFACE    1                  ///< Secondary 네트워크 인터페이스 (1=WiFi, 2=Ethernet)

// 듀얼모드 기본값
#define SWITCHER_DUAL_MODE_ENABLED       false              ///< 듀얼모드 기본값
#define SWITCHER_DUAL_MODE_OFFSET        4                  ///< Secondary 오프셋 기본값 (5번 채널부터)

// ============================================================================
// NVS 키 이름 (설정 저장용) - Primary 스위처
// ============================================================================

#define NVS_PRIMARY_TYPE         "pri_type"       ///< Primary 스위처 타입
#define NVS_PRIMARY_IP           "pri_ip"         ///< Primary IP 주소
#define NVS_PRIMARY_PORT         "pri_port"       ///< Primary 포트
#define NVS_PRIMARY_PASSWORD     "pri_password"   ///< Primary 비밀번호 (OBS)
#define NVS_PRIMARY_CAMERA_LIMIT "pri_cam_limit" ///< Primary 카메라 제한
#define NVS_PRIMARY_INTERFACE    "pri_interface"  ///< Primary 네트워크 인터페이스 (1=WiFi, 2=Ethernet)

// ============================================================================
// NVS 키 이름 (설정 저장용) - Secondary 스위처
// ============================================================================

#define NVS_SECONDARY_TYPE       "sec_type"       ///< Secondary 스위처 타입
#define NVS_SECONDARY_IP         "sec_ip"         ///< Secondary IP 주소
#define NVS_SECONDARY_PORT       "sec_port"       ///< Secondary 포트
#define NVS_SECONDARY_PASSWORD   "sec_password"   ///< Secondary 비밀번호 (OBS)
#define NVS_SECONDARY_CAMERA_LIMIT "sec_cam_limit" ///< Secondary 카메라 제한
#define NVS_SECONDARY_INTERFACE  "sec_interface"  ///< Secondary 네트워크 인터페이스 (1=WiFi, 2=Ethernet)

// ============================================================================
// NVS 키 이름 (듀얼모드 설정)
// ============================================================================

#define NVS_DUAL_MODE_ENABLED     "dual_enabled"   ///< 듀얼모드 사용 여부
#define NVS_SECONDARY_OFFSET      "sec_offset"     ///< Secondary 채널 오프셋
