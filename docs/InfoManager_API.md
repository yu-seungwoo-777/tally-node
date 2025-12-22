# InfoManager API 문서

## 개요

InfoManager는 TALLY-NODE 시스템의 모든 정보를 중앙에서 관리하는 컴포넌트입니다. 장치 ID, 시스템 상태, 패킷 통계 등을 스레드 안전하게 관리하며 Observer 패턴을 통해 실시간 변경 알림을 제공합니다.

## 특징

- **중앙 정보 관리**: 장치 ID, 배터리, 온도, 업타임, 패킷 통계 등
- **스레드 안전**: std::mutex를 사용한 동기화
- **영속성**: NVS를 통한 데이터 저장
- **Observer 패턴**: 실시간 정보 업데이트 알림
- **C/C++ 하이브리드**: C와 C++에서 모두 사용 가능한 인터페이스

## 초기화

```c
// C 인터페이스
esp_err_t ret = info_manager_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "InfoManager 초기화 실패: %s", esp_err_to_name(ret));
    return ret;
}
```

```cpp
// C++ 인터페이스
esp_err_t ret = info::InfoManager::init();
if (!ret) {
    ESP_LOGE(TAG, "InfoManager 초기화 실패: %s", esp_err_to_name(ret));
    return ret.error();
}
```

## 주요 API

### 1. 장치 ID 관리

#### C 인터페이스

```c
/**
 * @brief 장치 ID 조회
 * @param buf ID를 저장할 버퍼 (최소 INFO_DEVICE_ID_MAX_LEN)
 * @param buf_len 버퍼 크기
 * @return ESP_OK 성공, 그 외 오류 코드
 */
esp_err_t info_manager_get_device_id(char* buf, size_t buf_len);

/**
 * @brief 장치 ID 설정
 * @param device_id 새 장치 ID (최대 15자)
 * @return ESP_OK 성공, 그 외 오류 코드
 */
esp_err_t info_manager_set_device_id(const char* device_id);

/**
 * @brief WiFi MAC 주소 기반으로 장치 ID 자동 생성
 * @return ESP_OK 성공, 그 외 오류 코드
 */
esp_err_t info_manager_generate_device_id(void);
```

#### C++ 인터페이스

```cpp
/**
 * @brief 장치 ID 조회
 * @return Result<string> 성공 시 장치 ID, 실패 시 오류
 */
Result<std::string> InfoManager::getDeviceId() const;

/**
 * @brief 장치 ID 설정
 * @param device_id 새 장치 ID (최대 15자)
 * @return VoidResult 성공/실패 정보
 */
VoidResult InfoManager::setDeviceId(const std::string& device_id);

/**
 * @brief MAC 주소 기반 장치 ID 자동 생성
 * @return VoidResult 성공/실패 정보
 */
VoidResult InfoManager::generateDeviceId();
```

### 2. 시스템 정보 관리

#### C 인터페이스

```c
/**
 * @brief 시스템 정보 조회
 * @param info 정보를 저장할 구조체 포인터
 * @return ESP_OK 성공, 그 외 오류 코드
 */
esp_err_t info_manager_get_system_info(info_system_info_t* info);

/**
 * @brief 시스템 정보 캐시 갱신
 * @return ESP_OK 성공, 그 외 오류 코드
 */
esp_err_t info_manager_update_system_info(void);
```

#### C++ 인터페이스

```cpp
/**
 * @brief 시스템 정보 조회
 * @return Result<info_system_info_t> 성공 시 시스템 정보, 실패 시 오류
 */
Result<info_system_info_t> InfoManager::getSystemInfo() const;

/**
 * @brief 시스템 정보 갱신
 * @return VoidResult 성공/실패 정보
 */
VoidResult InfoManager::updateSystemInfo();
```

### 3. Observer 패턴

#### C 인터페이스

```c
// Observer 콜백 함수 타입
typedef void (*info_observer_fn_t)(const info_system_info_t* info, void* ctx);

/**
 * @brief Observer 등록
 * @param callback 콜백 함수
 * @param ctx 사용자 컨텍스트
 * @param out_handle Observer 핸들 출력
 * @return ESP_OK 성공, 그 외 오류 코드
 */
esp_err_t info_manager_add_observer(info_observer_fn_t callback,
                                   void* ctx,
                                   info_observer_handle_t* out_handle);

/**
 * @brief Observer 제거
 * @param handle 제거할 Observer 핸들
 * @return ESP_OK 성공, 그 외 오류 코드
 */
esp_err_t info_manager_remove_observer(info_observer_handle_t handle);
```

#### C++ 인터페이스

```cpp
/**
 * @brief Observer 등록
 * @param callback 콜백 함수
 * @param ctx 사용자 컨텍스트
 * @return Result<info_observer_handle_t> 성공 시 핸들, 실패 시 오류
 */
Result<info_observer_handle_t> InfoManager::addObserver(info_observer_fn_t callback,
                                                      void* ctx);

/**
 * @brief Observer 제거
 * @param handle 제거할 Observer 핸들
 * @return VoidResult 성공/실패 정보
 */
VoidResult InfoManager::removeObserver(info_observer_handle_t handle);
```

### 4. 패킷 통계 관리

#### C 인터페이스

```c
/**
 * @brief 송신 패킷 카운트 증가
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_increment_packet_tx(void);

/**
 * @brief 수신 패킷 카운트 증가
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_increment_packet_rx(void);

/**
 * @brief 에러 카운트 증가
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_increment_error_count(void);
```

#### C++ 인터페이스

```cpp
/**
 * @brief 송신 패킷 카운트 증가
 * @return VoidResult 성공/실패 정보
 */
VoidResult InfoManager::incrementPacketTx();

/**
 * @brief 수신 패킷 카운트 증가
 * @return VoidResult 성공/실패 정보
 */
VoidResult InfoManager::incrementPacketRx();

/**
 * @brief 에러 카운트 증가
 * @return VoidResult 성공/실패 정보
 */
VoidResult InfoManager::incrementErrorCount();
```

## 데이터 구조

### info_system_info_t

```c
typedef struct {
    // 장치 정보
    char device_id[INFO_DEVICE_ID_MAX_LEN];  // 장치 ID (최대 15자 + NULL)
    char wifi_mac[18];                        // WiFi MAC 주소 (XX:XX:XX:XX:XX:XX)

    // 시스템 상태
    uint64_t uptime_sec;          // 업타임 (초)
    uint32_t free_heap;           // 사용 가능한 힙 메모리
    uint32_t min_free_heap;       // 최소 힙 메모리

    // 하드웨어 상태
    float temperature;            // 온도 (°C)
    float battery_percent;       // 배터리 (%)
    float voltage;               // 전압 (V)

    // LoRa 상태
    float lora_rssi;             // LoRa RSSI (dBm)
    float lora_snr;              // LoRa SNR (dB)

    // 통계 정보
    uint32_t packet_count_tx;    // 송신 패킷 수
    uint32_t packet_count_rx;    // 수신 패킷 수
    uint32_t error_count;        // 에러 수
} info_system_info_t;
```

## 사용 예제

### 예제 1: 기본 사용법 (C)

```c
#include "info/info_manager.h"

void app_main() {
    // 1. 초기화
    ESP_ERROR_CHECK(info_manager_init());

    // 2. 장치 ID 조회
    char device_id[INFO_DEVICE_ID_MAX_LEN];
    ESP_ERROR_CHECK(info_manager_get_device_id(device_id, sizeof(device_id)));
    ESP_LOGI(TAG, "Device ID: %s", device_id);

    // 3. 시스템 정보 조회
    info_system_info_t info;
    ESP_ERROR_CHECK(info_manager_get_system_info(&info));
    ESP_LOGI(TAG, "Uptime: %llu sec, Battery: %.1f%%",
             info.uptime_sec, info.battery_percent);

    // 4. 정리
    info_manager_deinit();
}
```

### 예제 2: Observer 사용 (C)

```c
#include "info/info_manager.h"

// Observer 콜백 함수
void on_system_info_changed(const info_system_info_t* info, void* ctx) {
    ESP_LOGI(TAG, "System updated - Battery: %.1f%%, Temp: %.1f°C",
             info->battery_percent, info->temperature);
}

void app_main() {
    // 1. 초기화
    ESP_ERROR_CHECK(info_manager_init());

    // 2. Observer 등록
    info_observer_handle_t handle;
    ESP_ERROR_CHECK(info_manager_add_observer(on_system_info_changed, NULL, &handle));

    // 3. 메인 루프
    while (1) {
        // InfoManager는 다른 컴포넌트에서 업데이트함
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 4. 정리
    info_manager_remove_observer(handle);
    info_manager_deinit();
}
```

### 예제 3: C++에서 사용

```cpp
#include "info/InfoManager.hpp"

extern "C" void app_main() {
    // 1. 초기화
    auto result = info::InfoManager::init();
    if (!result) {
        ESP_LOGE(TAG, "Init failed: %s", esp_err_to_name(result.error()));
        return;
    }

    // 2. InfoManager 인스턴스 접근
    auto* mgr = info::InfoManager::get();

    // 3. 장치 ID 설정
    result = mgr->setDeviceId("TEST001");
    if (!result) {
        ESP_LOGE(TAG, "Set device ID failed: %s", esp_err_to_name(result.error()));
    }

    // 4. 시스템 정보 조회
    auto info_result = mgr->getSystemInfo();
    if (info_result) {
        const auto& info = info_result.value();
        ESP_LOGI(TAG, "Device: %s, Uptime: %llu",
                 info.device_id, info.uptime_sec);
    }

    // 5. 정리
    info::InfoManager::deinit();
}
```

### 예제 4: DisplayManager 연동

```cpp
// DisplayManager.cpp
#include "info/info_manager.h"

// Observer 콜백
static void onSystemInfoChanged(const info_system_info_t* info, void* ctx) {
    // 디스플레이 업데이트
    s_system_info.battery_percent = info->battery_percent;
    s_system_info.temperature_celsius = info->temperature;
    strncpy(s_system_info.device_id, info->device_id,
            sizeof(s_system_info.device_id) - 1);

    // 화면 갱신 요청
    s_system_info.display_changed = true;
}

void DisplayManager_init() {
    // InfoManager 초기화 확인
    if (info_manager_is_initialized()) {
        // Observer 등록
        info_manager_add_observer(onSystemInfoChanged, NULL, &s_info_observer_handle);
        ESP_LOGI(TAG, "InfoManager Observer registered");
    }
}
```

## NVS 저장

InfoManager는 다음 데이터를 NVS에 영속화합니다:

- **Namespace**: `info_mgr`
- **Keys**:
  - `device_id`: 장치 ID (string)
  - `id_gen_type`: ID 생성 타입 (uint8: 0=수동, 1=MAC 기반)

## 스레드 안전성

- 모든 공개 API는 내부적으로 mutex를 사용하여 스레드 안전성을 보장합니다
- Observer 콜백은 mutex 외부에서 호출되어 데드락을 방지합니다
- 여러 태스크에서 동시에 API를 호출해도 안전합니다

## 오류 처리

### C 인터페이스
- `ESP_OK`: 성공
- `ESP_ERR_INVALID_ARG`: 잘못된 인자
- `ESP_ERR_INVALID_STATE`: 미초기화 상태
- `ESP_ERR_NO_MEM`: 메모리 부족
- `ESP_ERR_NOT_FOUND`: 데이터 없음

### C++ 인터페이스 (Result<T>)
- `result.is_ok()`: 성공 여부 확인
- `result.value()`: 성공 시 값 접근
- `result.error()`: 실패 시 에러 코드 접근

## 성능 고려사항

1. **캐싱**: 시스템 정보는 내부 캐시에 저장되어 빠른 접근이 가능합니다
2. **NVS 쓰기**: dirty flag를 사용하여 변경 시에만 NVS에 씁니다
3. **메모리**: Observer는 vector로 관리되며 동적으로 확장됩니다
4. **뮤텍스**: 최소화된 잠금 시간으로 성능 저하를 방지합니다

## 주의사항

1. **초기화 순서**: InfoManager를 가장 먼저 초기화해야 합니다
2. **메모리**: Observer는 등록된 순서대로 호출되며 너무 많은 Observer는 성능에 영향을 줄 수 있습니다
3. **콜백**: Observer 콜백에서 뮤텍스를 사용할 경우 데드락에 주의해야 합니다
4. **NVS 수명**: NVS 쓰기 횟수는 한정되어 있으므로 잦은 변경은 피해야 합니다