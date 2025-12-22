# EoRa-S3 역할 기반 아키텍처 리팩토링 계획 v2

## 문서 정보
- **위치**: `/docs/refactoring_plan.md`
- **작성일**: 2025-12-12
- **버전**: 2.0
- **상태**: 설계 단계

## 1. 개요

### 1.1 현재 상태
- 플랫폼: ESP32-S3 (EoRa-S3), ESP-IDF 5.5.0
- 언어: C (2726 파일) / C++ (84 파일) 혼용
- 이슈: 역할이 명확히 구분되지 않은 컴포넌트 구조

### 1.2 리팩토링 목표
1. **역할 기반 분리**: 각 컴포넌트가 명확한 역할을 가지도록 재구성
2. **레이어드 아키텍처**: 수직적 의존성 관계 명확화
3. **C/C++ 하이브리드**: 새로운 기능은 C++로, 하드웨어는 C로 유지
4. **ESP-IDF 준수**: 컴포넌트 규칙과 빌드 시스템 준수
5. **스레드 안전성**: 멀티태스크 환경에서의 안전한 동작 보장

## 2. 역할 기반 컴포넌트 분류

### 2.1 컴포넌트 역할 정의

#### 역할 유형
1. **Application Layer**
   - 사용자 시나리오 구현
   - 비즈니스 로직 처리

2. **Domain Manager Layer**
   - 각 도메인의 관리자
   - 상위 계층의 요청을 처리하고 하위 계층을 조율

3. **Core Service Layer**
   - 하드웨어 추상화
   - 저수준 기능 제공

4. **Infrastructure Layer**
   - 공통 기반 기술
   - 유틸리티 및 지원 기능

#### 현재 컴포넌트 분류
```
Application Layer:
├── main.cpp                 # 메인 애플리케이션

Domain Manager Layer:
├── display/                 # 디스플레이 관리자
│   ├── DisplayManager.cpp
│   ├── PageManager.c
│   └── RxPage.cpp
├── lora/                    # LoRa 통신 관리자
│   ├── LoRaManager.cpp
│   └── CommunicationManager.cpp
├── network/                 # 네트워크 관리자 (TX)
│   └── NetworkManager.cpp
├── switcher/                # 스위처 관리자 (TX)
│   └── SwitcherManager.cpp
├── system/                  # 시스템 관리자
│   ├── SystemMonitor.cpp
│   ├── ConfigCore.cpp
│   └── button_poll/
└── interface/               # 인터페이스 관리자
    ├── cli/                 # CLI 관리자
    └── web/                 # 웹 인터페이스 관리자

Core Service Layer:
├── lora/core/               # LoRa 하드웨어 추상화
├── display/u8g2-hal/        # OLED 드라이버
├── switcher/protocol/       # 스위처 프로토콜
└── system/button_poll/core/ # 버튼 드라이버

Infrastructure Layer:
├── simple_log/              # 로깅 시스템
├── json/                    # JSON 처리
└── common/                  # 공통 유틸리티
```

## 3. 새로운 아키텍처 설계

### 3.1 InfoManager 도입

#### 개요
시스템의 모든 정보(장치 ID, 상태, 통계 등)를 중앙에서 관리하는 컴포넌트

#### 위치
```
components/
└── info/
    ├── include/
    │   └── info/
    │       ├── InfoManager.hpp    # C++ 헤더
    │       ├── info_manager.h     # C 인터페이스
    │       ├── info_types.h       # 공용 타입 정의
    │       └── result.hpp         # Result<T> 패턴
    ├── src/
    │   ├── InfoManager.cpp        # C++ 구현
    │   ├── DeviceIdManager.cpp    # 장치 ID 관리
    │   └── info_manager_c.cpp     # C 래퍼
    ├── test/
    │   └── test_info_manager.cpp  # 유닛 테스트
    └── CMakeLists.txt
```

#### 책임
1. 장치 ID 생성 및 관리 (NVS 영속화)
2. 시스템 정보 캐시
3. 정보 요청에 대한 중앙 집중식 처리
4. Observer 패턴을 통한 변경 알림
5. 스레드 안전한 접근 제공

### 3.2 공용 타입 정의

#### info_types.h
```c
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 장치 ID 최대 길이
#define INFO_DEVICE_ID_MAX_LEN  16
#define INFO_MAC_ADDR_STR_LEN   18

// 시스템 정보 구조체
typedef struct {
    char device_id[INFO_DEVICE_ID_MAX_LEN];
    char wifi_mac[INFO_MAC_ADDR_STR_LEN];
    float battery_percent;
    float temperature;
    uint32_t uptime_sec;
    uint32_t free_heap;
    uint32_t min_free_heap;
} info_system_info_t;

// Observer 콜백 타입 (함수 포인터 + context)
typedef void (*info_observer_fn_t)(const info_system_info_t* info, void* ctx);

// Observer 핸들
typedef struct info_observer* info_observer_handle_t;

#ifdef __cplusplus
}
#endif
```

### 3.3 Result 패턴 구현

#### result.hpp
```cpp
#pragma once

#include "esp_err.h"
#include <type_traits>

namespace info {

// 값이 없는 결과용
struct Void {};

template<typename T = Void>
class Result {
public:
    // 성공 생성자
    static Result ok(T value) {
        Result r;
        r.value_ = value;
        r.err_ = ESP_OK;
        return r;
    }

    // 성공 생성자 (Void 특수화)
    static Result ok() {
        static_assert(std::is_same_v<T, Void>, "Use ok(value) for non-void types");
        Result r;
        r.err_ = ESP_OK;
        return r;
    }

    // 실패 생성자
    static Result fail(esp_err_t err) {
        Result r;
        r.err_ = err;
        return r;
    }

    // 상태 확인
    bool is_ok() const { return err_ == ESP_OK; }
    bool is_err() const { return err_ != ESP_OK; }
    explicit operator bool() const { return is_ok(); }

    // 값 접근 (성공 시에만 유효)
    const T& value() const { return value_; }
    T& value() { return value_; }

    // 에러 코드 접근
    esp_err_t error() const { return err_; }

    // 기본값 반환
    T value_or(const T& default_val) const {
        return is_ok() ? value_ : default_val;
    }

private:
    Result() = default;
    T value_{};
    esp_err_t err_ = ESP_FAIL;
};

// Void 특수화 헬퍼
using VoidResult = Result<Void>;

} // namespace info
```

### 3.4 C++ 인터페이스 설계

#### InfoManager.hpp
```cpp
#pragma once

#include "info/info_types.h"
#include "info/result.hpp"
#include <string>
#include <vector>
#include <mutex>

namespace info {

class InfoManager {
public:
    // 명시적 초기화/해제 (싱글톤 대신)
    static esp_err_t init();
    static void deinit();
    static InfoManager* get();  // init() 후에만 유효

    // 복사/이동 금지
    InfoManager(const InfoManager&) = delete;
    InfoManager& operator=(const InfoManager&) = delete;
    InfoManager(InfoManager&&) = delete;
    InfoManager& operator=(InfoManager&&) = delete;

    // 장치 ID 관리 (스레드 안전)
    Result<std::string> getDeviceId() const;
    VoidResult setDeviceId(const std::string& device_id);
    VoidResult generateDeviceId();  // MAC 기반 자동 생성

    // 시스템 정보 (스레드 안전)
    Result<info_system_info_t> getSystemInfo() const;
    VoidResult updateSystemInfo();  // 내부 캐시 갱신

    // Observer 관리 (스레드 안전)
    // 반환된 핸들로 나중에 제거 가능
    Result<info_observer_handle_t> addObserver(info_observer_fn_t callback, void* ctx);
    VoidResult removeObserver(info_observer_handle_t handle);

    // 모든 옵저버에게 알림 (내부 사용)
    void notifyObservers();

private:
    InfoManager();
    ~InfoManager();

    // NVS 관련
    esp_err_t loadFromNvs();
    esp_err_t saveToNvs();

    // 멤버 변수
    mutable std::mutex mutex_;
    std::string device_id_;
    info_system_info_t cached_info_{};

    // Observer 저장소
    struct ObserverEntry {
        info_observer_fn_t callback;
        void* ctx;
        bool active;
    };
    std::vector<ObserverEntry> observers_;
    uint32_t next_observer_id_ = 0;

    // 싱글톤 인스턴스 (init/deinit으로 관리)
    static InfoManager* instance_;
    static std::mutex init_mutex_;
};

} // namespace info
```

### 3.5 C 인터페이스 설계

#### info_manager.h
```c
#pragma once

#include "info/info_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief InfoManager 초기화
 * @return ESP_OK 성공, 그 외 실패
 * @note app_main()에서 다른 컴포넌트보다 먼저 호출
 */
esp_err_t info_manager_init(void);

/**
 * @brief InfoManager 해제
 */
void info_manager_deinit(void);

/**
 * @brief 초기화 여부 확인
 * @return true 초기화됨, false 미초기화
 */
bool info_manager_is_initialized(void);

/**
 * @brief 장치 ID 조회
 * @param[out] buf 결과 저장 버퍼
 * @param[in] buf_len 버퍼 크기 (최소 INFO_DEVICE_ID_MAX_LEN)
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 잘못된 인자, ESP_ERR_INVALID_STATE 미초기화
 */
esp_err_t info_manager_get_device_id(char* buf, size_t buf_len);

/**
 * @brief 장치 ID 설정
 * @param[in] device_id 새 장치 ID (null-terminated)
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 잘못된 인자
 */
esp_err_t info_manager_set_device_id(const char* device_id);

/**
 * @brief MAC 주소 기반 장치 ID 자동 생성
 * @return ESP_OK 성공
 */
esp_err_t info_manager_generate_device_id(void);

/**
 * @brief 시스템 정보 조회
 * @param[out] info 결과 저장 구조체
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG 잘못된 인자
 */
esp_err_t info_manager_get_system_info(info_system_info_t* info);

/**
 * @brief 시스템 정보 캐시 갱신
 * @return ESP_OK 성공
 */
esp_err_t info_manager_update_system_info(void);

/**
 * @brief Observer 등록
 * @param[in] callback 콜백 함수
 * @param[in] ctx 사용자 컨텍스트 (콜백에 전달됨)
 * @param[out] out_handle Observer 핸들 (제거 시 사용)
 * @return ESP_OK 성공, ESP_ERR_NO_MEM 메모리 부족
 */
esp_err_t info_manager_add_observer(info_observer_fn_t callback, 
                                     void* ctx,
                                     info_observer_handle_t* out_handle);

/**
 * @brief Observer 제거
 * @param[in] handle 등록 시 받은 핸들
 * @return ESP_OK 성공, ESP_ERR_NOT_FOUND 핸들 없음
 */
esp_err_t info_manager_remove_observer(info_observer_handle_t handle);

#ifdef __cplusplus
}
#endif
```

### 3.6 스레드 안전성 전략

#### 동기화 원칙
```
┌─────────────────────────────────────────────────────────────┐
│                    스레드 안전성 매트릭스                      │
├─────────────────────┬───────────────────────────────────────┤
│ 작업                │ 동기화 방식                            │
├─────────────────────┼───────────────────────────────────────┤
│ 읽기 작업           │ std::lock_guard<std::mutex>           │
│ 쓰기 작업           │ std::lock_guard<std::mutex>           │
│ Observer 알림       │ 뮤텍스 해제 후 콜백 호출 (데드락 방지)   │
│ NVS 접근            │ InfoManager 뮤텍스로 보호              │
│ ISR에서 호출        │ 금지 - 태스크에서만 호출               │
└─────────────────────┴───────────────────────────────────────┘
```

#### 구현 예시
```cpp
Result<std::string> InfoManager::getDeviceId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (device_id_.empty()) {
        return Result<std::string>::fail(ESP_ERR_NOT_FOUND);
    }
    return Result<std::string>::ok(device_id_);
}

void InfoManager::notifyObservers() {
    // 콜백 목록 복사 (뮤텍스 보호 하에)
    std::vector<ObserverEntry> observers_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_copy = observers_;
    }

    // 뮤텍스 해제 후 콜백 호출 (데드락 방지)
    info_system_info_t info;
    if (getSystemInfo().is_ok()) {
        info = getSystemInfo().value();
        for (const auto& entry : observers_copy) {
            if (entry.active && entry.callback) {
                entry.callback(&info, entry.ctx);
            }
        }
    }
}
```

### 3.7 NVS 영속화 설계

#### NVS 키 구조
```
NVS Namespace: "info_mgr"
├── "device_id"   : string (최대 15자)
├── "id_gen_type" : uint8 (0=manual, 1=mac_based)
└── "first_boot"  : uint8 (0=no, 1=yes)
```

#### 구현 예시
```cpp
esp_err_t InfoManager::loadFromNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("info_mgr", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        // 첫 부팅 - 기본값 사용
        return generateDeviceId().error();
    }

    char buf[INFO_DEVICE_ID_MAX_LEN];
    size_t len = sizeof(buf);
    err = nvs_get_str(handle, "device_id", buf, &len);
    if (err == ESP_OK) {
        device_id_ = buf;
    }

    nvs_close(handle);
    return err;
}

esp_err_t InfoManager::saveToNvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("info_mgr", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, "device_id", device_id_.c_str());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}
```

## 4. 계층별 설계 원칙

### 4.1 Application Layer
- 책임: 사용자 시나리오 오케스트레이션
- 구현: C++ 가능
- 의존성: Domain Manager Layer에만 의존
- 예시:
```cpp
// main.cpp
extern "C" void app_main() {
    // 1. Infrastructure 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || 
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. InfoManager 초기화 (가장 먼저)
    ESP_ERROR_CHECK(info_manager_init());

    // 3. 나머지 매니저 초기화
    ESP_ERROR_CHECK(SystemMonitor::init());
    ESP_ERROR_CHECK(DisplayManager::init());
    ESP_ERROR_CHECK(LoRaManager::init());

    // 4. 장치 정보 로깅
    char device_id[INFO_DEVICE_ID_MAX_LEN];
    info_manager_get_device_id(device_id, sizeof(device_id));
    LOG_0(TAG_MAIN, "Device ID: %s", device_id);

    // 5. 메인 루프
    while (true) {
        CommunicationManager::loop();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

### 4.2 Domain Manager Layer
- 책임: 도메인별 비즈니스 로직
- 구현: C++ 권장 (C 인터페이스 제공)
- 의존성: Core Service Layer + 다른 Domain Manager
- 예시:
```cpp
// DisplayManager.cpp
class DisplayManager {
public:
    static esp_err_t init() {
        // InfoManager observer 등록
        info_observer_handle_t handle;
        esp_err_t err = info_manager_add_observer(
            onSystemInfoChanged, 
            nullptr, 
            &handle
        );
        if (err != ESP_OK) {
            return err;
        }
        observer_handle_ = handle;
        return ESP_OK;
    }

private:
    static void onSystemInfoChanged(const info_system_info_t* info, void* ctx) {
        // 디스플레이 업데이트
        page_manager_update_device_info(info->device_id);
    }

    static info_observer_handle_t observer_handle_;
};
```

### 4.3 Core Service Layer
- 책임: 하드웨어 추상화, 저수준 기능
- 구현: C 유지 (성능 및 메모리 효율성)
- 의존성: Infrastructure Layer만
- 예시:
```c
// LoRaCore.c
esp_err_t lora_send_packet(const uint8_t* data, size_t length) {
    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 하드웨어 직접 제어
    spi_transaction_t trans = {
        .length = length * 8,
        .tx_buffer = data,
    };

    return spi_device_transmit(spi_handle, &trans);
}
```

### 4.4 Infrastructure Layer
- 책임: 공통 기반 기술
- 구현: C 또는 C++
- 의존성: 외부 라이브러리, ESP-IDF

## 5. C/C++ 하이브리드 전략

### 5.1 언어 선택 가이드라인

| 구분 | C++ 사용 | C 사용 |
|------|----------|--------|
| 새로운 매니저 | ✅ | ❌ |
| UI/비즈니스 로직 | ✅ | ⭕ |
| 하드웨어 드라이버 | ❌ | ✅ |
| 성능 중심 코드 | ⭕ | ✅ |
| FreeRTOS 인터페이스 | ⭕ | ✅ |
| ESP-IDF 래퍼 | ✅ | ⭕ |
| ISR 핸들러 | ❌ | ✅ |

### 5.2 인터페이스 통합 방식

#### C 래퍼 안전한 구현
```cpp
// info_manager_c.cpp
#include "info/InfoManager.hpp"
#include "info/info_manager.h"

extern "C" {

esp_err_t info_manager_get_device_id(char* buf, size_t buf_len) {
    if (buf == nullptr || buf_len < INFO_DEVICE_ID_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->getDeviceId();
    if (!result) {
        return result.error();
    }

    strncpy(buf, result.value().c_str(), buf_len - 1);
    buf[buf_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t info_manager_add_observer(info_observer_fn_t callback, 
                                     void* ctx,
                                     info_observer_handle_t* out_handle) {
    if (callback == nullptr || out_handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    auto* mgr = info::InfoManager::get();
    if (mgr == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    auto result = mgr->addObserver(callback, ctx);
    if (!result) {
        return result.error();
    }

    *out_handle = result.value();
    return ESP_OK;
}

} // extern "C"
```

## 6. 이전 계획

### 6.1 단계별 이전

#### 1단계: 기반 구축 (2주)
**목표**: InfoManager 핵심 구현

| 작업 | 예상 시간 | 산출물 |
|------|----------|--------|
| Result<T> 패턴 구현 | 2일 | result.hpp |
| info_types.h 정의 | 1일 | info_types.h |
| InfoManager 기본 구조 | 3일 | InfoManager.hpp/cpp |
| NVS 영속화 구현 | 2일 | loadFromNvs/saveToNvs |
| C 래퍼 구현 | 2일 | info_manager.h/c |
| 유닛 테스트 | 2일 | test_info_manager.cpp |

**완료 조건**:
- [ ] 장치 ID 저장/로드 동작
- [ ] C/C++ 양쪽에서 API 호출 가능
- [ ] 리부팅 후에도 ID 유지

#### 2단계: Observer 패턴 및 통합 (1주)
**목표**: Observer 구현 및 SystemMonitor 통합

| 작업 | 예상 시간 | 산출물 |
|------|----------|--------|
| Observer 구현 | 2일 | addObserver/removeObserver |
| 스레드 안전성 테스트 | 1일 | 멀티태스크 테스트 |
| SystemMonitor 통합 | 2일 | 기존 코드 수정 |

**완료 조건**:
- [ ] Observer 등록/해제 동작
- [ ] 여러 태스크에서 동시 접근 시 안정적
- [ ] SystemMonitor에서 InfoManager 사용

#### 3단계: DisplayManager 개선 (1주)
**목표**: DisplayManager를 InfoManager와 연동

| 작업 | 예상 시간 | 산출물 |
|------|----------|--------|
| Observer 연동 | 2일 | 자동 디스플레이 갱신 |
| 기존 device_id 코드 제거 | 1일 | 코드 정리 |
| 통합 테스트 | 2일 | E2E 테스트 |

**완료 조건**:
- [ ] 장치 ID 변경 시 디스플레이 자동 갱신
- [ ] 기존 기능 모두 정상 동작

#### 4단계: CommunicationManager 정리 (1주)
**목표**: 프로토콜 계층과 InfoManager 통합

| 작업 | 예상 시간 | 산출물 |
|------|----------|--------|
| InfoManager 통합 | 2일 | 프로토콜에서 ID 사용 |
| 상태 관리 개선 | 2일 | 통신 상태를 Info로 노출 |
| 테스트 | 1일 | 통합 테스트 |

#### 5단계: 문서화 및 안정화 (1주)
**목표**: 문서화 및 코드 정리

| 작업 | 예상 시간 | 산출물 |
|------|----------|--------|
| API 문서 작성 | 2일 | Doxygen 주석 |
| 아키텍처 다이어그램 | 1일 | draw.io 다이어그램 |
| 코드 리뷰 및 정리 | 2일 | 리팩토링 |

### 6.2 전체 일정 (6주)
```
Week 1-2: 1단계 - InfoManager 핵심 구현
Week 3:   2단계 - Observer 및 SystemMonitor 통합
Week 4:   3단계 - DisplayManager 개선
Week 5:   4단계 - CommunicationManager 정리
Week 6:   5단계 - 문서화 및 안정화
```

### 6.3 리스크 관리

#### 기술적 리스크
| 리스크 | 확률 | 영향 | 대응 |
|--------|------|------|------|
| C++ 예외 오버헤드 | 중 | 중 | -fno-exceptions 플래그 사용 |
| std::mutex 성능 | 낮 | 중 | 필요시 FreeRTOS mutex로 교체 |
| NVS 쓰기 마모 | 낮 | 중 | 쓰기 빈도 제한, dirty flag 사용 |
| 메모리 단편화 | 중 | 높 | vector 예약, 풀 allocator 검토 |

#### 호환성 유지 전략
```
1. 기존 API 유지
   - 기존 함수 시그니처 변경 금지
   - deprecated 마킹 후 최소 1버전 유지

2. 점진적 전환
   - 새 API와 기존 API 공존
   - 기능별로 순차 전환

3. 롤백 계획
   - 각 단계별 Git 태그
   - main 브랜치 보호
   - 문제 발생 시 이전 태그로 복구
```

## 7. 코딩 규칙

### 7.1 로깅
```c
#include "log.h"
#include "log_tags.h"

LOG_0(TAG_INFO, "장치 ID: %s", device_id);  // 레벨 0: 항상 출력
LOG_1(TAG_INFO, "상세 정보: %d", status);   // 레벨 1: 디버그
```

### 7.2 명명 규칙
- 파일명: PascalCase.cpp/hpp, snake_case.c/h
- C 함수: snake_case (info_manager_get_device_id)
- C++ 클래스: PascalCase (InfoManager)
- C++ 메서드: camelCase (getDeviceId)
- 변수: snake_case (device_id, system_info)
- 상수/매크로: UPPER_SNAKE_CASE (INFO_DEVICE_ID_MAX_LEN)
- 네임스페이스: lowercase (info, lora, display)

### 7.3 에러 처리
```cpp
// C++: Result<T> 패턴
Result<std::string> getDeviceId() const;

// C: esp_err_t 반환 + out 파라미터
esp_err_t info_manager_get_device_id(char* buf, size_t buf_len);
```

### 7.4 헤더 가드
```cpp
// C++ 헤더
#pragma once

// C 헤더
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ...

#ifdef __cplusplus
}
#endif
```

## 8. 빌드 설정

### 8.1 CMakeLists.txt
```cmake
# components/info/CMakeLists.txt
idf_component_register(
    SRCS
        "src/InfoManager.cpp"
        "src/DeviceIdManager.cpp"
        "src/info_manager_c.cpp"
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES
        nvs_flash
        esp_netif
        simple_log
)

# C++ 설정
target_compile_options(${COMPONENT_LIB} PRIVATE
    -std=c++17
    -fno-exceptions
    -fno-rtti
)
```

### 8.2 sdkconfig 권장 설정
```
# C++ 지원
CONFIG_COMPILER_CXX_EXCEPTIONS=n
CONFIG_COMPILER_CXX_RTTI=n

# 스택 크기 (C++ 사용 시 증가 필요)
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192

# NVS
CONFIG_NVS_ENCRYPTION=n
```

## 9. 테스트 전략

### 9.1 유닛 테스트
```cpp
// test/test_info_manager.cpp
#include "unity.h"
#include "info/InfoManager.hpp"

TEST_CASE("InfoManager init/deinit", "[info]") {
    TEST_ASSERT_EQUAL(ESP_OK, info::InfoManager::init());
    TEST_ASSERT_NOT_NULL(info::InfoManager::get());
    info::InfoManager::deinit();
    TEST_ASSERT_NULL(info::InfoManager::get());
}

TEST_CASE("Device ID persistence", "[info][nvs]") {
    info::InfoManager::init();
    auto* mgr = info::InfoManager::get();

    // 설정
    auto result = mgr->setDeviceId("TEST01");
    TEST_ASSERT_TRUE(result.is_ok());

    // 재초기화 후 확인
    info::InfoManager::deinit();
    info::InfoManager::init();
    mgr = info::InfoManager::get();

    auto id_result = mgr->getDeviceId();
    TEST_ASSERT_TRUE(id_result.is_ok());
    TEST_ASSERT_EQUAL_STRING("TEST01", id_result.value().c_str());

    info::InfoManager::deinit();
}

TEST_CASE("Thread safety", "[info][multithread]") {
    info::InfoManager::init();

    // 여러 태스크에서 동시 접근 테스트
    // ...

    info::InfoManager::deinit();
}
```

### 9.2 통합 테스트 체크리스트
- [ ] 부팅 시 장치 ID 로드
- [ ] 장치 ID 변경 시 NVS 저장
- [ ] Observer 콜백 정상 호출
- [ ] 디스플레이 자동 갱신
- [ ] LoRa 패킷에 장치 ID 포함
- [ ] CLI에서 장치 ID 조회/변경
- [ ] 웹 인터페이스에서 장치 ID 표시

## 10. 성능 고려사항

### 10.1 메모리 사용
| 항목 | 예상 사용량 | 비고 |
|------|------------|------|
| InfoManager 인스턴스 | ~200 bytes | 고정 |
| Observer 엔트리 | ~16 bytes/개 | 동적 |
| cached_info_ | ~64 bytes | 고정 |
| std::string device_id | ~32 bytes | 동적 (SSO) |

### 10.2 실시간성
- ISR에서 InfoManager 호출 금지
- 긴급 데이터는 FreeRTOS Queue 사용
- Observer 콜백은 빠르게 완료 (무거운 작업은 태스크로 위임)

### 10.3 최적화 옵션
```cmake
# Release 빌드 시
target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Release>:-flto>
)
```

## 11. 다음 단계

### 즉시 실행
1. **Git 브랜치 생성**: `feature/info-manager`
2. **기본 구조 생성**: 디렉토리 및 CMakeLists.txt
3. **Result<T> 구현**: result.hpp 작성

### 1주차 목표
- [ ] info_types.h 완성
- [ ] result.hpp 완성
- [ ] InfoManager 기본 뼈대 구현
- [ ] 컴파일 성공

### 마일스톤
| 마일스톤 | 목표일 | 확인 방법 |
|----------|--------|----------|
| M1: 컴파일 성공 | Week 1 | 빌드 통과 |
| M2: 기본 기능 | Week 2 | 유닛 테스트 통과 |
| M3: 통합 완료 | Week 4 | 기존 기능 정상 동작 |
| M4: 안정화 | Week 6 | 48시간 연속 동작 |

## 12. 참고 자료

- [ESP-IDF Component Management](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)
- [ESP-IDF NVS API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [FreeRTOS Task Notifications](https://www.freertos.org/RTOS-task-notifications.html)
- [Embedded C++ Best Practices](https://www.embedded.com/modern-c-in-embedded-systems/)