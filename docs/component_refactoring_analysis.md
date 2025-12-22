# 컴포넌트 리팩토링 규칙 준수 분석

## 분석 개요

TALLY-NODE 프로젝트의 각 컴포넌트가 리팩토링 계획에 명시된 규칙을 잘 준수하는지 분석합니다.

## 리팩토링 규칙 요약

1. **역할 기반 분리**
   - Core Service Layer: 시스템 핵심 기능
   - Domain Manager Layer: 비즈니스 로직 조율
   - Infrastructure Layer: 하드웨어 추상화

2. **설계 원칙**
   - 단일 책임 원칙 (Single Responsibility)
   - 의존성 역전 (Dependency Inversion)
   - 스레드 안전성

3. **코딩 규칙**
   - C/C++ 하이브리드
   - 정적 할당 선호
   - 에러 처리 일관성

## 분석 결과

### ✅ 잘 준수된 컴포넌트

#### 1. InfoManager (Core Service Layer)
```cpp
// 장치 관리자: DeviceIdManager.cpp
class DeviceIdManager {
public:
    static esp_err_t init();
    static esp_err_t generateDeviceIdFromMac(char* device_id, size_t size);
private:
    DeviceIdManager() = delete;
};

// 정보 관리자: InfoManager.cpp
class InfoManager {
public:
    static Result<std::string> getDeviceId() const;
    static VoidResult addObserver(info_observer_fn_t callback, void* ctx);
private:
    mutable std::mutex mutex_;
    std::vector<ObserverEntry> observers_;
    static InfoManager* instance_;
};
```

**준수 사항:**
- ✅ 명확한 책임 분리 (DeviceIdManager, InfoManager)
- ✅ 스레드 안전성 (mutex 보호)
- ✅ Result<T> 패턴 사용
- ✅ C/C++ 하이브리드 인터페이스
- ✅ 싱글톤 패턴

#### 2. NetworkManager (Domain Manager Layer)
```cpp
class NetworkManager {
public:
    static esp_err_t init();
    static NetworkStatus getStatus();
    static bool isInitialized();

private:
    NetworkManager() = delete;
    ~NetworkManager() = delete;
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    static bool s_initialized;
};
```

**준수 사항:**
- ✅ 명확한 책임 (Core API 조율)
- ✅ 상태 관리
- ✅ 싱글톤 패턴
- ✅ 정적 메서드

#### 3. LoRaManager (Domain Manager Layer)
```cpp
class LoRaManager {
public:
    static esp_err_t init(const LoRaConfig* config);
    static esp_err_t transmit(const uint8_t* data, size_t length);
    static LoRaStatus getStatus();
    static bool isInitialized();

private:
    LoRaManager() = delete;
    static bool s_initialized;
};
```

**준수 사항:**
- ✅ LoRaCore 위한 추상화 계층
- ✅ 상태 관리
- ✅ ConfigCore 통합

### ⚠️ 개선이 필요한 부분

#### 1. SystemMonitor (Core Service Layer)

**문제점:**
- C++ 클래스가 아닌 C 스타일로 구현됨
- InfoManager와의 중복 기능 (device_id 관리)
- 스레드 안전성 보장 장치 부족

```c
// 현재 구조
struct SystemHealth {
    char device_id[16];     // InfoManager와 중복
    char wifi_mac[18];
    float voltage;
    // ...
};

// 개선 제안
class SystemMonitor {
public:
    static Result<SystemHealth> getHealth();
    static esp_err_t startMonitoring();
    static VoidResult addObserver(HealthObserverFn_t callback, void* ctx);

private:
    static std::mutex mutex_;
    static SystemHealth s_health;
    static std::vector<ObserverEntry> s_observers;
};
```

#### 2. DisplayManager (Domain Manager Layer)

**문제점:**
- C 스타일로 구현됨
- 전역 변수 과다 사용
- InfoManager Observer 등록은 있지만 C 스타일 구현

```cpp
// 현재 구조
static DisplaySystemInfo_t s_system_info;
static SemaphoreHandle_t s_display_mutex;

// 개선 제안
class DisplayManager {
public:
    static esp_err_t init();
    static VoidResult registerForSystemUpdates();
    static VoidResult showTallyData(const TallyData& data);

private:
    static std::mutex display_mutex_;
    static DisplayState s_state;
    static ObserverHandle s_info_observer;
};
```

#### 3. SwitcherManager (Domain Manager Layer)

**문제점:**
- 구조체 중첩이 복잡함
- 초기화 로직이 너무 긺
- 에러 처리가 일관성 없음

```cpp
// 현재 구조
struct SwitcherContext {
    SwitcherConfig config;
    switcher_t* handle;
    // ... 10개 이상의 필드
};

// 개선 제안
class SwitcherManager {
public:
    struct SwitcherInfo {
        SwitcherConfig config;
        ConnectionStatus status;
        TallyState last_tally;
    };

    static Result<std::vector<SwitcherInfo>> getSwitchers();
    static Result<SwitcherHandle> connect(const SwitcherConfig& config);
    static VoidResult startTallyMonitoring();

private:
    static std::map<SwitcherIndex, std::unique_ptr<Switcher>> s_switchers;
    static std::mutex switchers_mutex_;
};
```

### 📊 규칙 준수 현황

| 규칙 항목 | InfoManager | NetworkManager | LoRaManager | SystemMonitor | DisplayManager | SwitcherManager | 평균 |
|-----------|-------------|---------------|-------------|---------------|--------------|---------------|------|
| 역할 분리 | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ✅ | 80% |
| 단일 책임 | ✅ | ✅ | ✅ | ❌ | ⚠️ | ⚠️ | 65% |
| 스레드 안전 | ✅ | ❌ | ❌ | ❌ | ⚠️ | ❌ | 30% |
| 정적 할당 | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | 70% |
| C/C++ 하이브리드 | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ✅ | 75% |
| 에러 처리 | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | 45% |

### 개선 우선순위

#### 1. SystemMonitor C++ 클래스화 (긴급)
- InfoManager 중복 기능 제거
- 스레드 안전성 보장
- Observer 패턴 적용

#### 2. DisplayManager 리팩토링 (중간)
- 클래스 기반으로 전환
- 전역 변수 제거
- InfoManager와 통합 강화

#### 3. 스레드 안전성 개선 (중간)
- 각 Manager 클래스에 mutex 추가
- 공유 자원 보호
- 데드락 방지 설계

#### 4. SwitcherManager 구조 개선 (장기)
- 복잡한 구조체 단순화
- 에러 처리 표준화
- 상태 관리 개선

## 권장 리팩토링 계획

### 1단계: SystemMonitor 개선 (3일)

```cpp
// components/system/monitor/SystemMonitor.hpp
class SystemMonitor {
public:
    struct HealthData {
        float voltage;
        float temperature;
        uint64_t uptime_ms;
        // InfoManager에서 가져올 정보 제외
    };

    using HealthObserver = std::function<void(const HealthData&)>;

    static Result<HealthData> getHealth() const;
    static esp_err_t startMonitoring();
    static VoidResult addObserver(HealthObserver observer);
    static esp_err_t stopMonitoring();

private:
    static std::mutex mutex_;
    static HealthData s_health;
    static std::vector<HealthObserver> s_observers;
    static TaskHandle_t s_monitor_task;
};
```

### 2단계: DisplayManager 개선 (2일)

```cpp
// components/display/DisplayManager.hpp
class DisplayManager {
public:
    enum class DisplayType {
        BOOT,
        RX_TALLY,
        TX_STATUS,
        SYSTEM_INFO
    };

    static esp_err_t init();
    static VoidResult showPage(DisplayType type);
    static VoidResult updateTally(const TallyData& data);

private:
    class DisplayState {
    public:
        DisplayType current_page;
        bool boot_complete;
        DisplaySystemInfo system_info;
    };

    static std::mutex display_mutex_;
    static DisplayState s_state;
    static ObserverHandle s_info_observer;
};
```

### 3단계: 스레드 안전성 적용 (1일)

주요 Manager 클래스에 mutex 적용:
- NetworkManager: `static std::mutex s_mutex_;`
- LoRaManager: `static std::mutex s_mutex_;`
- SwitcherManager: `static std::mutex s_mutex_;`

### 4단계: 공통 인터페이스 정의 (1일)

```cpp
// components/core/ManagerInterface.hpp
template<typename StateType>
class ManagerInterface {
public:
    virtual ~ManagerInterface() = default;
    virtual esp_err_t init() = 0;
    virtual bool isInitialized() const = 0;
    virtual Result<StateType> getState() const = 0;

protected:
    mutable std::mutex mutex_;
};
```

## 예상 효과

1. **유지보수성 향상**: 클래스 기반으로 객체 지향 프로그래밍 가능
2. **스레드 안전성**: 동시 접근 시 데이터 무결성 보장
3. **테스트 용이성**: 의존성 주입으로 모의 객체 테스트 가능
4. **확장성**: 인터페이스 기반으로 새로운 Manager 추가 용이

## 결론

InfoManager를 제외한 대부분의 컴포넌트가 리팩토링 규칙을 부분적으로만 준수하고 있습니다. 특히 스레드 안전성과 C++ 클래스 기반 구현이 필요합니다.

점진적 개선을 통해 전체적인 코드 품질을 향상시키고, InfoManager 중앙 관리 시스템과 더 잘 통합될 수 있을 것입니다.