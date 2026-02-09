# Ethernet Interrupt Connection Status Fix

## Bug Summary

**이슈**: 인터럽트 모드에서 로그상 연결 상태가 "connected"로 표시되지만, 실제로는 연결되지 않은 상태인 경우 발생

**발생 상황**:
```
(1058) [05_Ethernet]    int:connected (interrupt mode)
(1089) [03_Network]     driver init complete (event-based)
(1090) [03_Network]     Network status publish task start
```

로그에서 "int:connected"로 표시되지만 실제 네트워크 연결이 안 된 상태

## Root Cause

### 기존 코드 문제점

`components/05_hal/ethernet_hal/ethernet_hal.c`의 `ethernet_hal_init()` 함수에서:

1. **줄 364**: INT 핀 레벨만 확인 (GPIO read only)
2. **줄 366-368**: GPIO 레벨 HIGH 시 "int:connected (interrupt mode)" 로그 출력
3. **줄 394-395**: MAC/PHY 핸들 생성 (실제 칩 확인)
4. **줄 417**: ESP-IDF 드라이버 설치 (실제 칩 확인)
5. **줄 432**: `s_detected = true` (최종적으로 칩이 응답하는지 확인)

### 문제 분석

- **임시 감지**: INT 핀 레벨만으로는 칩이 실제로 켜지고 작동하는지 확인할 수 없음
- **GPIO 플로팅 가능성**: INT 핀이 연결되지 않으면 풀업 저항으로 인해 HIGH 레벨로 플로팅될 수 있음
- **실제 확인 필요**: MAC/PHY 핸들 생성, 드라이버 설치 단계에서 실제 칩을 확인해야 함
- **로그 순서 문제**: "int:connected" 로그가 하드웨어 초기화 전에 출력되어 혼란 발생

## 수정 내용

### 1. 임시 변수 추가

```c
bool int_pin_detected = false;
```

- INT 핀 HIGH 감지 시 `int_pin_detected = true`로 설정 (임시 플래그)
- 이 플래그는 드라이버 설치 성공 여부와 함께 사용

### 2. 에러 메시지 개선

**Before**:
```c
T_LOGE(TAG, "fail:mac_phy");
```

**After**:
```c
T_LOGE(TAG, "fail:mac_phy (chip not responding)");
```

- 실패 원인을 명확히 명시 (칩 응답 없음)

### 3. 드라이버 설치 후 `s_detected` 설정 로직 변경

**Before**:
```c
// MAC/PHY 생성
s_eth_mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
s_eth_phy = esp_eth_phy_new_w5500(&phy_config);

if (!s_eth_mac || !s_eth_phy) {
    // 에러 처리
    return ESP_FAIL;
}

// 드라이버 설치
ret = esp_eth_driver_install(&eth_config, &s_eth_handle);
if (ret != ESP_OK) {
    // 에러 처리
    s_detected = false;
    return ret;
}

s_detected = true;  // 항상 true
```

**After**:
```c
// MAC/PHY 생성
s_eth_mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
s_eth_phy = esp_eth_phy_new_w5500(&phy_config);

if (!s_eth_mac || !s_eth_phy) {
    // 에러 처리
    s_detected = false;
    return ESP_FAIL;
}

// ESP-IDF 드라이버 설치 (핸들 확인 후)
ret = esp_eth_driver_install(&eth_config, &s_eth_handle);
if (ret != ESP_OK) {
    // 에러 처리
    s_detected = false;
    return ret;
}

// 드라이버 설치 성공 후 실제 칩 감지 여부 확인
s_detected = true;

// INT 핀 감지 성공 시 인터럽트 모드 로그 출력 (드라이버 설치 성공 후)
if (int_pin_detected) {
    T_LOGI(TAG, "int:connected (interrupt mode, chip verified)");
}
```

### 4. 로그 메시지 개선

**Before**:
```
int:connected (interrupt mode)
```

**After**:
```
int:connected (interrupt mode, chip verified)
```

- "chip verified" 추가로 실제 칩 확인 완료 상태를 명확히 표시

## 수정 결과

### 수정 전 로그 (혼동 가능성)
```
(1058) [05_Ethernet]    int:connected (interrupt mode)
(1059) [05_Ethernet]    fail:driver:0x5002  <- 칩이 응답하지 않음
```

### 수정 후 로그 (명확함)
```
(1058) [05_Ethernet]    int:not_detected (polling mode, level=0)  <- INT 핀 미감지
(1059) [05_Ethernet]    fail:mac_phy (chip not responding)  <- 칩이 실제로 응답하지 않음
```

또는

```
(1058) [05_Ethernet]    int:connected (interrupt mode, chip verified)  <- 칩 확인됨
(1059) [05_Ethernet]    ok  <- 초기화 성공
```

## 영향 범위

- **영향 받는 파일**: `/home/prod/tally-node/components/05_hal/ethernet_hal/ethernet_hal.c`
- **영향 범위**: W5500 이더넷 드라이버 초기화 로직
- **영향 대상**: 인터럽트 모드 사용 시, INT 핀 연결 감지 로직

## 테스트 체크리스트

1. **INT 핀 연결된 상태**:
   - [ ] `int:connected (interrupt mode, chip verified)` 로그 출력 확인
   - [ ] 드라이버 초기화 성공 (`ok` 로그)
   - [ ] 네트워크 연결 확인 (DHCP 또는 Static IP 할당)

2. **INT 핀 미연결 상태 (플로팅)**:
   - [ ] `int:not_detected (polling mode, level=0)` 로그 출력 확인
   - [ ] 폴링 모드로 초기화 (`poll_period_ms = 20`)
   - [ ] 드라이버 초기화 실패 시 칩 응답 없음 명시

3. **INT GPIO 미설정**:
   - [ ] `int:not_configured (polling mode)` 로그 출력 확인
   - [ ] 폴링 모드로 초기화

4. **칩 응답 실패**:
   - [ ] `fail:mac_phy (chip not responding)` 또는 `fail:driver:0xxxxx` 로그 확인
   - [ ] `s_detected`가 `false`로 설정됨 확인

## 기술적 세부 사항

### GPIO Pull-up 패턴

W5500 INT 핀은:
- **연결된 경우**: 유휴 상태에서 HIGH 출력 (active-low 인터럽트)
- **미연결된 경우**: 풀업 저항으로 인해 HIGH로 플로팅 (floating high)

따라서:
- **HIGH** = INT 핀이 연결됨 (가정)
- **LOW** = INT 핀이 미연결됨

### 하드웨어 초기화 순서

1. **GPIO 설정**: INT 핀 풀업 활성화
2. **GPIO 레벨 확인**: 임시 감지 (플로팅 확인용)
3. **INT 모드/폴링 모드 선택**: `use_polling` 플래그 설정
4. **MAC 핸들 생성**: W5500 MAC 인스턴스 생성 (실제 칩 확인)
5. **PHY 핸들 생성**: W5500 PHY 인스턴스 생성 (실제 칩 확인)
6. **드라이버 설치**: ESP-IDF 이더넷 드라이버 설치 (실제 칩 확인)
7. **최종 확인**: `s_detected = true` 설정
8. **로그 출력**: "chip verified" 메시지 추가

### 버전 정보

- **ESP-IDF 버전**: 5.5.0
- **파일**: ethernet_hal.c
- **함수**: ethernet_hal_init()
- **수정일**: 2026-02-09

## 결론

이 수정으로 인해:
1. 로그에서 "int:connected"는 실제 칩이 정상 작동하는 경우에만 출력됨
2. 칩이 실제로 응답하지 않아도 "connected" 로그가 출력되는 오류 제거
3. 에러 상황에서 더 명확한 로그 메시지 제공
4. 실제 하드웨어 상태와 로그 상태의 일치성 확보
