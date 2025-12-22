/**
 * ATEM 버퍼 유틸리티
 *
 * 네트워크 바이트 순서(빅 엔디안)로 데이터를 읽고 쓰는 함수들
 * 순수 C 언어로 작성 (ESP-IDF, Linux 호환)
 */

#ifndef ATEM_BUFFER_H
#define ATEM_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 부호 없는 정수 읽기 (빅 엔디안)
 * ============================================================================ */

/**
 * 8비트 부호 없는 정수 읽기
 */
static inline uint8_t atem_get_u8(const uint8_t* data, uint16_t offset)
{
    return data[offset];
}

/**
 * 16비트 부호 없는 정수 읽기 (빅 엔디안)
 */
static inline uint16_t atem_get_u16(const uint8_t* data, uint16_t offset)
{
    return ((uint16_t)data[offset] << 8) | data[offset + 1];
}

/**
 * 32비트 부호 없는 정수 읽기 (빅 엔디안)
 */
static inline uint32_t atem_get_u32(const uint8_t* data, uint16_t offset)
{
    return ((uint32_t)data[offset] << 24) |
           ((uint32_t)data[offset + 1] << 16) |
           ((uint32_t)data[offset + 2] << 8) |
           data[offset + 3];
}

/* ============================================================================
 * 부호 있는 정수 읽기 (빅 엔디안)
 * ============================================================================ */

/**
 * 8비트 부호 있는 정수 읽기
 */
static inline int8_t atem_get_s8(const uint8_t* data, uint16_t offset)
{
    return (int8_t)data[offset];
}

/**
 * 16비트 부호 있는 정수 읽기 (빅 엔디안)
 */
static inline int16_t atem_get_s16(const uint8_t* data, uint16_t offset)
{
    return (int16_t)(((uint16_t)data[offset] << 8) | data[offset + 1]);
}

/**
 * 32비트 부호 있는 정수 읽기 (빅 엔디안)
 */
static inline int32_t atem_get_s32(const uint8_t* data, uint16_t offset)
{
    return (int32_t)(((uint32_t)data[offset] << 24) |
                     ((uint32_t)data[offset + 1] << 16) |
                     ((uint32_t)data[offset + 2] << 8) |
                     data[offset + 3]);
}

/* ============================================================================
 * 부호 없는 정수 쓰기 (빅 엔디안)
 * ============================================================================ */

/**
 * 8비트 부호 없는 정수 쓰기
 */
static inline void atem_set_u8(uint8_t* data, uint16_t offset, uint8_t value)
{
    data[offset] = value;
}

/**
 * 16비트 부호 없는 정수 쓰기 (빅 엔디안)
 */
static inline void atem_set_u16(uint8_t* data, uint16_t offset, uint16_t value)
{
    data[offset] = (uint8_t)(value >> 8);
    data[offset + 1] = (uint8_t)(value & 0xFF);
}

/**
 * 32비트 부호 없는 정수 쓰기 (빅 엔디안)
 */
static inline void atem_set_u32(uint8_t* data, uint16_t offset, uint32_t value)
{
    data[offset] = (uint8_t)(value >> 24);
    data[offset + 1] = (uint8_t)(value >> 16);
    data[offset + 2] = (uint8_t)(value >> 8);
    data[offset + 3] = (uint8_t)(value & 0xFF);
}

/* ============================================================================
 * 유틸리티 함수
 * ============================================================================ */

/**
 * 특정 비트 플래그 읽기
 *
 * @param data 데이터 포인터
 * @param offset 바이트 위치
 * @param bit 비트 위치 (0-7)
 * @return 플래그 값
 */
static inline bool atem_get_flag(const uint8_t* data, uint16_t offset, uint8_t bit)
{
    return (data[offset] & (1 << bit)) != 0;
}

/**
 * 바이트 시퀀스 복사
 *
 * @param dest 목적지 버퍼
 * @param src 소스 데이터
 * @param offset 소스 시작 위치
 * @param length 복사할 길이
 */
static inline void atem_get_bytes(uint8_t* dest, const uint8_t* src, uint16_t offset, uint16_t length)
{
    memcpy(dest, src + offset, length);
}

/**
 * 바이트 시퀀스 쓰기
 *
 * @param data 데이터 버퍼
 * @param offset 시작 위치
 * @param src 소스 데이터
 * @param length 쓸 길이
 */
static inline void atem_set_bytes(uint8_t* data, uint16_t offset, const uint8_t* src, uint16_t length)
{
    memcpy(data + offset, src, length);
}

/**
 * NULL 종료 문자열 읽기
 *
 * @param dest 목적지 버퍼 (충분한 크기 필요)
 * @param src 소스 데이터
 * @param offset 시작 위치
 * @param max_len 최대 길이 (dest 크기 - 1)
 */
static inline void atem_get_string(char* dest, const uint8_t* src, uint16_t offset, uint16_t max_len)
{
    uint16_t i;
    for (i = 0; i < max_len; i++) {
        dest[i] = (char)src[offset + i];
        if (dest[i] == '\0') {
            return;
        }
    }
    dest[max_len] = '\0';
}

/**
 * 명령 문자열 쓰기 (4글자)
 *
 * @param data 데이터 버퍼
 * @param offset 시작 위치
 * @param cmd 명령 문자열
 */
static inline void atem_set_command(uint8_t* data, uint16_t offset, const char* cmd)
{
    data[offset] = (uint8_t)cmd[0];
    data[offset + 1] = (uint8_t)cmd[1];
    data[offset + 2] = (uint8_t)cmd[2];
    data[offset + 3] = (uint8_t)cmd[3];
}

#ifdef __cplusplus
}
#endif

#endif /* ATEM_BUFFER_H */
