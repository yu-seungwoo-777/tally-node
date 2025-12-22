/**
 * @file system_monitor_c.cpp
 * @brief SystemMonitor C 래퍼 구현
 *
 * C 파일에서 C++ SystemMonitor 클래스를 사용하기 위한 래퍼 함수
 */

#include "SystemMonitor.h"

extern "C" {

// SystemHealth 구조체 정의 (C와 호환)
struct SystemHealth {
    uint64_t uptime_sec;
    float temperature_celsius;
    float voltage;
    uint8_t battery_percent;
    char wifi_mac[18];  // WiFi MAC 주소 (XX:XX:XX:XX:XX:XX)
};

// C 래퍼 함수
struct SystemHealth getSystemHealth(void) {
    SystemHealth cpp_health = SystemMonitor::getHealth();

    // C++ 구조체를 C 구조체로 변환
    struct SystemHealth c_health = {
        .uptime_sec = cpp_health.uptime_sec,
        .temperature_celsius = cpp_health.temperature_celsius,
        .voltage = cpp_health.voltage,
        .battery_percent = cpp_health.battery_percent
    };

    // WiFi MAC 주소 복사
    if (cpp_health.wifi_mac) {
        strncpy(c_health.wifi_mac, cpp_health.wifi_mac, sizeof(c_health.wifi_mac) - 1);
        c_health.wifi_mac[sizeof(c_health.wifi_mac) - 1] = '\0';
    } else {
        strcpy(c_health.wifi_mac, "00:00:00:00:00:00");
    }

    return c_health;
}

} // extern "C"