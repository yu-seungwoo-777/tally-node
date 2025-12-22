# InfoManager 리팩토링 요약

## 개요

본 문서는 TALLY-NODE 프로젝트의 InfoManager 도입을 통한 리팩토링 과정과 결과를 요약합니다.

## 리팩토링 목표

1. **중앙 정보 관리**: 각 컴포넌트에서 흩어져 관리되던 시스템 정보를 중앙에서 관리
2. **Device ID 문제 해결**: RX 모드에서 device_id가 "228"로 표시되던 문제 해결
3. **실시간 업데이트**: Observer 패턴을 통한 실시간 정보 전파
4. **스레드 안전성**: 멀티태스크 환경에서의 안전한 데이터 접근 보장
5. **코드 중복 제거**: 동일한 기능을 여러 곳에서 구현하는 코드 제거

## 구현 결과

### 1. InfoManager 컴포넌트 생성

```
components/info/
├── include/
│   ├── info/
│   │   ├── InfoManager.hpp    # C++ 핵심 클래스
│   │   └── result.hpp         # Result<T> 패턴
│   └── info_manager.h         # C 인터페이스
└── src/
    ├── InfoManager.cpp        # C++ 구현
    ├── DeviceIdManager.cpp    # 장치 ID 생성
    └── info_manager_c.cpp     # C/C++ 브릿지
```

### 2. 핵심 기능 구현

#### 장치 ID 관리
- WiFi MAC 주소 기반 자동 생성
- NVS 영속화 저장
- C/C++ 양쪽에서 접근 가능한 API

```cpp
// 초기화 시 자동 생성
esp_err_t info_manager_init();

// MAC 기반 ID 생성
esp_err_t info_manager_generate_device_id();
```

#### Observer 패턴
- 실시간 정보 업데이트 알림
- 스레드 안전한 콜백 호출

```c
// Observer 등록
info_manager_add_observer(onSystemInfoChanged, &display_ctx, &handle);

// 콜백 함수
void onSystemInfoChanged(const info_system_info_t* info, void* ctx);
```

#### 패킷 통계 관리
- 중앙 통계 집계
- 스레드 안전한 카운트 증가

```c
// 패킷 송수신 시 통계 업데이트
info_manager_increment_packet_tx();
info_manager_increment_packet_rx();
info_manager_increment_error_count();
```

### 3. 컴포넌트 통합

#### DisplayManager
- Observer 등록으로 실시간 업데이트
- device_id 중앙 관리로 RX 모드 문제 해결

```cpp
// DisplayManager 초기화 시 Observer 등록
info_manager_add_observer(onSystemInfoChanged, NULL, &s_info_observer_handle);

// 콜백에서 s_system_info 업데이트
static void onSystemInfoChanged(const info_system_info_t* info, void* ctx) {
    s_system_info.battery_percent = info->battery_percent;
    s_system_info.temperature_celsius = info->temperature;
    strncpy(s_system_info.device_id, info->device_id,
            sizeof(s_system_info.device_id) - 1);
    s_system_info.display_changed = true;
}
```

#### CommunicationManager
- 패킷 통계 중앙 관리
- 로컬 변수 제거로 메모리 최적화

```cpp
// 송신 성공 시
if (err == ESP_OK) {
    info_manager_increment_packet_tx();
} else {
    info_manager_increment_error_count();
}
```

#### SystemMonitor
- InfoManager와 양방향 동기화
- 중복 정보 제거

#### Web API
- /api/health 엔드포인트에 InfoManager 데이터 통합
- 실시간 시스템 정보 제공

## 성능 개선

### 메모리 사용량

| 모드 | 리팩토링 전 | 리팩토링 후 | 변화 |
|------|-------------|-------------|------|
| TX RAM | ~102KB | ~102KB | 동일 |
| RX RAM | ~58KB | ~57KB | -1KB |
| TX Flash | 1026KB | 1048KB | +22KB |
| RX Flash | 360KB | 362KB | +2KB |

메모리 사용량 변화가 미미한 이유:
- 기존 코드도 중앙 변수를 사용하던 구조
- InfoManager가 기존 변수를 대체하면서 중복 제거
- Observer 패턴은 적은 메모리로 구현

### 실행 시간
- **캐싱 효과**: 시스템 정보 조회 시 NVS 접근 제거
- **업데이트 효율**: Observer 패턴으로 필요한 곳만 업데이트
- **락 최소화**: 읽기 작업에서 락 최소화

## 코드 품질 개선

### 1. 중복 코드 제거
- device_id 관리 코드 3곳 → 1곳
- 패킷 통계 코드 2곳 → 1곳
- 시스템 정보 접근 코드 다수 → InfoManager API

### 2. 의존성 단순화
- 컴포넌트 간 직접 의존 감소
- InfoManager를 통한 간접 통신
- 테스트 용이성 향상

### 3. 에러 처리 개선
- Result<T> 패턴으로 타입 안전한 에러 처리
- 일관된 에러 코드 사용
- 예외 사용 제거 (-fno-exceptions)

## 해결된 문제

### 1. Device ID 표시 문제
**문제**: RX 모드에서 device_id가 "228"로 표시
- **원인**: WiFi 초기화 없이 기본값 사용
- **해결**: InfoManager에서 부팅 시 MAC 주소 기반 ID 생성

### 2. 정보 동기화 문제
**문제**: 컴포넌트별 정보 불일치
- **원인**: 각 컴포넌트에서 독립 관리
- **해결**: Observer 패턴으로 실시간 동기화

### 3. 스레드 안전성
**문제**: 멀티태스크 환경에서 데이터 경쟁
- **원인**: 공유 변수에 보호 없이 접근
- **해결**: mutex로 모든 API 보호

## 남은 과제

### 1. TODO 항목 정리
- [ ] CommunicationManager의 미구현 display 업데이트 로직
- [ ] RX 모드 상태 저장 및 웹 UI 업데이트
- [ ] 실제 온도 센서 값 활용

### 2. 성능 최적화
- [ ] NVS 쓰기 빈도 최적화 (dirty flag 활용)
- [ ] Observer 호출 비용 측정
- [ ] 캐시 유효기간 정책

### 3. 추가 기능
- [ ] 암호화된 장치 ID 저장
- [ ] 통계 이력 기능
- [ ] 디버깅을 위한 정보 덤프

## 교훈

### 1. 중앙 관리의 중요성
- 정보 흩어지면 유지보수 어려움
- 초기 중앙화 설계 중요

### 2. C/C++ 하이브리드 전략
- C++로 핵심 기능 구현
- C로 레거시 호환성 유지
- 성능과 유지보수성 균형

### 3. 점진적 리팩토링
- 기존 기능 유지하며 점진적 전환
- 각 단계별 테스트 필수
- 롤백 계획 수립

## 결론

InfoManager 도입을 통한 리팩토링은 다음과 같은 효과를 가져왔습니다:

1. **안정성 향상**: 스레드 안전성, 데이터 일관성 보장
2. **유지보수성 개선**: 중앙 관리로 코드 중복 제거
3. **확장성 확보**: Observer 패턴으로 기능 확장 용이
4. **문제 해결**: Device ID 표시 문제 등 장기적인 문제 해결

특히 RX 모드에서의 Device ID 문제 해결은 사용자 경험에 직접적인 영향을 주었으며, Observer 패턴 도입은 향후 기능 확장의 기반이 되었습니다.

향후 InfoManager를 기반으로 로깅 시스템, 상태 머신, 설정 관리 등 추가 기능을 통합하여 더욱 견고한 시스템을 구축할 수 있을 것입니다.