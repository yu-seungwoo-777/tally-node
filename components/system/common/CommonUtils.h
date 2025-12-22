/**
 * @file CommonUtils.h
 * @brief 공통 유틸리티 함수
 */

#pragma once

#include <stdint.h>
#include <string.h>
#include <stdio.h>

namespace Utils {

/**
 * @brief MAC 주소를 문자열로 변환
 */
inline void macToString(const uint8_t* mac, char* str, size_t len) {
    snprintf(str, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief IP 주소를 문자열로 변환
 */
inline void ipToString(uint32_t ip, char* str, size_t len) {
    snprintf(str, len, "%d.%d.%d.%d",
             (int)(ip & 0xFF),
             (int)((ip >> 8) & 0xFF),
             (int)((ip >> 16) & 0xFF),
             (int)((ip >> 24) & 0xFF));
}

/**
 * @brief 채널 배열을 문자열로 변환 (예: "1,4,7")
 */
inline void channelsToString(const uint8_t* channels, uint8_t count, char* buf, size_t buf_size) {
    if (count == 0) {
        snprintf(buf, buf_size, "--");
        return;
    }

    char* p = buf;
    char* end = buf + buf_size - 1;
    for (uint8_t i = 0; i < count && p < end; i++) {
        if (i > 0 && p < end) *p++ = ',';
        p += snprintf(p, end - p, "%d", channels[i]);
    }
}

} // namespace Utils
