# Battery Management System

## Overview

배터리 관리 시스템은 ESP32-S3 내부 ADC를 사용하여 배터리 전압을 측정하고, 18650 리튬이온 배터리 기준으로 백분율로 변환하여 OLED 디스플레이와 웹 인터페이스에 표시합니다.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Hardware Layer                               │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  Battery HAL (05_hal/battery_hal)                          │   │
│  │  - ADC1 Channel 0 (GPIO1)                                  │   │
│  │  - 12-bit resolution (0~4095)                              │   │
│  │  - 12dB attenuation (0~3300mV input)                       │   │
│  │  - Voltage divider ratio: 2:1                              │   │
│  └───────────────────────┬─────────────────────────────────────┘   │
└──────────────────────────┼───────────────────────────────────────────┘
                           │
┌──────────────────────────┼───────────────────────────────────────────┐
│                    Driver Layer                                    │
│  ┌───────────────────────┼─────────────────────────────────────┐   │
│  │  Battery Driver (04_driver/battery_driver)                 │   │
│  │  - voltageToPercent(): 전압 → 백분율 변환                  │   │
│  │  - 18650 Li-ion 기준 전압 임계값                           │   │
│  └───────────────────────┼─────────────────────────────────────┘   │
└──────────────────────────┼───────────────────────────────────────────┘
                           │
┌──────────────────────────┼───────────────────────────────────────────┐
│                   Service Layer                                    │
│  ┌───────────────────────┼─────────────────────────────────────┐   │
│  │  Hardware Service (03_service/hardware_service)             │   │
│  │  - 1초마다 배터리 갱신                                     │   │
│  │  - EVT_INFO_UPDATED 이벤트 발행                            │   │
│  └───────────────────────┼─────────────────────────────────────┘   │
└──────────────────────────┼───────────────────────────────────────────┘
                           │
┌──────────────────────────┼───────────────────────────────────────────┐
│                  Presentation Layer                                │
│  ┌───────────────────────┼─────────────────────────────────────┐   │
│  │  Display Manager (02_presentation/display)                  │   │
│  │  - OLED 배터리 아이콘 표시                                 │   │
│  │  - Web Server (API: /api/status)                           │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

## Voltage to Percent Conversion

### Constants (battery_driver.cpp:27-30)

| Constant | Value | Description |
|----------|-------|-------------|
| `BATTERY_VOLTAGE_FULL` | 4.0V | 100% 충전 |
| `BATTERY_VOLTAGE_HIGH` | 3.9V | 90% 충전 |
| `BATTERY_VOLTAGE_NOMINAL` | 3.5V | 50% 충전 |
| `BATTERY_VOLTAGE_LOW` | 3.0V | 0% 충전 |

### Conversion Table

| Voltage Range | Percent Range | Formula |
|---------------|---------------|---------|
| ≥ 4.0V | 100% | Fixed |
| 3.9V ~ 4.0V | 90% ~ 100% | `90 + (v - 3.9) / 0.1 * 10` |
| 3.8V ~ 3.9V | 80% ~ 90% | `80 + (v - 3.8) / 0.1 * 10` |
| 3.7V ~ 3.8V | 70% ~ 80% | `70 + (v - 3.7) / 0.1 * 10` |
| 3.6V ~ 3.7V | 60% ~ 70% | `60 + (v - 3.6) / 0.1 * 10` |
| 3.5V ~ 3.6V | 50% ~ 60% | `50 + (v - 3.5) / 0.1 * 10` |
| 3.4V ~ 3.5V | 40% ~ 50% | `40 + (v - 3.4) / 0.1 * 10` |
| 3.3V ~ 3.4V | 30% ~ 40% | `30 + (v - 3.3) / 0.1 * 10` |
| 3.0V ~ 3.3V | 0% ~ 30% | `(v - 3.0) / 0.3 * 30` |
| < 3.0V | 0% | Fixed |

### Conversion Code (battery_driver.cpp:171-196)

```cpp
uint8_t BatteryDriver::voltageToPercent(float voltage)
{
    if (voltage >= BATTERY_VOLTAGE_FULL) {
        return 100;
    } else if (voltage >= BATTERY_VOLTAGE_HIGH) {
        return (uint8_t)(90.0f + (voltage - BATTERY_VOLTAGE_HIGH) / 0.1f * 10.0f);
    } else if (voltage >= 3.8f) {
        return (uint8_t)(80.0f + (voltage - 3.8f) / 0.1f * 10.0f);
    } else if (voltage >= 3.7f) {
        return (uint8_t)(70.0f + (voltage - 3.7f) / 0.1f * 10.0f);
    } else if (voltage >= 3.6f) {
        return (uint8_t)(60.0f + (voltage - 3.6f) / 0.1f * 10.0f);
    } else if (voltage >= BATTERY_VOLTAGE_NOMINAL) {
        return (uint8_t)(50.0f + (voltage - BATTERY_VOLTAGE_NOMINAL) / 0.1f * 10.0f);
    } else if (voltage >= 3.4f) {
        return (uint8_t)(40.0f + (voltage - 3.4f) / 0.1f * 10.0f);
    } else if (voltage >= 3.3f) {
        return (uint8_t)(30.0f + (voltage - 3.3f) / 0.1f * 10.0f);
    } else if (voltage >= BATTERY_VOLTAGE_LOW) {
        return (uint8_t)((voltage - BATTERY_VOLTAGE_LOW) / 0.3f * 30.0f);
    }
    return 0;
}
```

## API Reference

### HAL Layer (battery_hal.h)

```c
esp_err_t battery_hal_init(void);
esp_err_t battery_hal_read_voltage(float* voltage);
esp_err_t battery_hal_read_voltage_mV(int* voltage_mv);
bool battery_hal_is_initialized(void);
```

### Driver Layer (battery_driver.h)

```c
esp_err_t battery_driver_init(void);
esp_err_t battery_driver_get_voltage(float* voltage);
uint8_t battery_driver_get_percent(void);
uint8_t battery_driver_update_percent(void);
uint8_t battery_driver_voltage_to_percent(float voltage);
bool battery_driver_is_initialized(void);
```

### Service Layer (hardware_service.h)

```c
uint8_t hardware_service_update_battery(void);
uint8_t hardware_service_get_battery(void);
float hardware_service_get_voltage(void);
```

## Update Flow

### 1-Second Monitoring Loop (hardware_service.cpp:239-284)

```cpp
void HardwareService::hw_monitor_task(void* arg)
{
    while (s_running) {
        // 배터리 업데이트 (ADC 읽기)
        updateBattery();

        // 온도 업데이트
        updateTemperature();

        // uptime 증가
        s_uptime++;

        // 하드웨어 정보 이벤트 발행
        system_info_event_t info;
        info.battery = s_battery;
        info.voltage = s_voltage;
        // ...
        event_bus_publish(EVT_INFO_UPDATED, &info, sizeof(info));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Known Issues

### 1. ADC 중복 읽기 (hardware_service.cpp:359-371)

```cpp
uint8_t HardwareService::updateBattery(void)
{
    uint8_t percent = battery_driver_update_percent();  // ADC 읽기 1
    s_battery = percent;

    float voltage = 3.7f;
    if (battery_driver_get_voltage(&voltage) == ESP_OK) {  // ADC 읽기 2 (중복)
        s_voltage = voltage;
    }
    return percent;
}
```

**문제점**: `battery_driver_update_percent()` 내부에서 이미 ADC를 읽지만, `battery_driver_get_voltage()`가 다시 ADC를 읽습니다.

**해결 방안**: 전압과 퍼센트를 한 번의 ADC 읽기로 가져오는 함수 추가 필요.

### 2. 측정 실패 시 기본값 100% (battery_driver.cpp:157-158)

```cpp
uint8_t BatteryDriver::getPercent(void)
{
    float voltage;
    if (getVoltage(&voltage) == ESP_OK && voltage >= BATTERY_VOLTAGE_MIN_VALID) {
        return voltageToPercent(voltage);
    }
    return 100;  // 측정 실패 시 100% 반환
}
```

**문제점**: 배터리가 연결되지 않았거나 ADC 실패 시 100%로 표시되어 오해의 소지가 있습니다.

**해결 방안**: 실패 시 0% 또는 특정 에러 코드 반환 고려.

### 3. 전압 범위 설정

- 현재: 3.0V ~ 4.0V (18650 기준)
- 일반적인 18650: 2.5V(완전방전) ~ 4.2V(완전충전)
- 4.2V 입력 시 100%로 표시되지만, 실제로는 완전충전 상태

## File Structure

```
components/
├── 05_hal/battery_hal/
│   ├── battery_hal.c
│   └── include/battery_hal.h
├── 04_driver/battery_driver/
│   ├── battery_driver.cpp
│   └── include/battery_driver.h
└── 03_service/hardware_service/
    ├── hardware_service.cpp
    └── include/hardware_service.h
```

## References

- ESP32 ADC Calibration: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/adc.html
- 18650 Li-ion Discharge Curve: 일반적으로 4.2V ~ 3.0V 범위에서 사용

---
*문서 버전: 1.0.0*
*마지막 수정: 2026-01-19*
