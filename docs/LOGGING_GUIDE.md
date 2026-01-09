# 로깅 가이드라인

> 작성일: 2026-01-10
> 버전: 1.1

## 목차

1. [TAG 명명 규칙](#1-tag-명명-규칙)
2. [로그 레벨 가이드라인](#2-로그-레벨-가이드라인)
3. [로그 메시지 형식](#3-로그-메시지-형식)
4. [함수 단위 로그 리팩토링](#4-함수-단위-로그-리팩토링)
5. [개선 우선순위](#5-개선-우선순위)
6. [현재 문제점](#6-현재-문제점)

---

## 1. TAG 명명 규칙

### 1.1 표준 규칙

```
{LayerNumber}_{ComponentName}
```

- **레이어 번호 접두사** (01~05, 00)
- **언더스코어 구분자**
- **PascalCase** 컴포넌트명
- **계층 타입 접미사 제외** (Service, Driver, Hal 등)

### 1.2 레이어 번호

| 번호 | 계층 | 설명 |
|------|------|------|
| 00 | common | 공통 (EventBus 등) |
| 01 | app | 앱 (TxApp, RxApp) |
| 02 | presentation | 프레젠테이션 (WebSvr, Display) |
| 03 | service | 서비스 (Device, Config, Led) |
| 04 | driver | 드라이버 (LoRa, WiFi, Atem) |
| 05 | hal | HAL (LoRa, WiFi, Battery) |

### 1.3 예시

| 계층 | 현재 | 제안 | 비고 |
|------|------|------|------|
| App | `prod_rx_app` | `01_RxApp` | 레이어 + 간결명 |
| App | `prod_tx_app` | `01_TxApp` | 레이어 + 간결명 |
| Service | `ButtonSvc` | `03_Button` | Service 접미사 제외 |
| Service | `DeviceMgr` | `03_Device` | Mgr 약어 제외 |
| Service | `ConfigService` | `03_Config` | Service 접미사 제외 |
| Driver | `LoRaDriver` | `04_LoRa` | Driver 접미사 제외 |
| Driver | `AtemDriver` | `04_Atem` | Driver 접미사 제외 |
| HAL | `LoRaHal` | `05_LoRa` | Hal 접미사 제외 |
| HAL | `DISP_HAL` | `05_Display` | 대문자 → PascalCase |
| HAL | `TEMP_HAL` | `05_Temp` | 대문자 → PascalCase |

### 1.4 전체 TAG 목록

```
# 00_common
static const char* TAG = "00_EventBus";

# 01_app
static const char* TAG = "01_TxApp";
static const char* TAG = "01_RxApp";

# 02_presentation
static const char* TAG = "02_WebSvr";
static const char* TAG = "02_Display";
static const char* TAG = "02_BootPage";
static const char* TAG = "02_RxPage";
static const char* TAG = "02_TxPage";

# 03_service
static const char* TAG = "03_Device";
static const char* TAG = "03_Config";
static const char* TAG = "03_Button";
static const char* TAG = "03_Led";
static const char* TAG = "03_Network";
static const char* TAG = "03_Switcher";
static const char* TAG = "03_License";
static const char* TAG = "03_Hardware";

# 04_driver
static const char* TAG = "04_LoRa";
static const char* TAG = "04_WiFi";
static const char* TAG = "04_Ethernet";
static const char* TAG = "04_DispDrv";
static const char* TAG = "04_Atem";
static const char* TAG = "04_Vmix";
static const char* TAG = "04_Battery";
static const char* TAG = "04_Temp";
static const char* TAG = "04_Ws2812";
static const char* TAG = "04_BoardLed";
static const char* TAG = "04_LicenseCli";

# 05_hal
static const char* TAG = "05_LoRa";
static const char* TAG = "05_WiFi";
static const char* TAG = "05_Ethernet";
static const char* TAG = "05_Display";
static const char* TAG = "05_Battery";
static const char* TAG = "05_Temp";
static const char* TAG = "05_Ws2812";
static const char* TAG = "05_U8g2";
```

---

## 2. 로그 레벨 가이드라인

### 2.1 레벨 정의

| 레벨 | 용도 | 출력 여부 |
|------|------|-----------|
| ERROR | 심각한 오류, 복구 필요 | 항상 |
| WARN | 잠재적 문제, 대체 값 사용 | 항상 |
| INFO | 주요 상태 변화, 사용자 알림 | 기본 |
| DEBUG | 개발용 디버깅 정보 | 선택 |
| VERBOSE | 상세 추적 정보 | 선택 |

### 2.2 사용 기준

#### ERROR - 항상 출력

```cpp
// 초기화 실패
T_LOGE(TAG, "초기화 실패: %s", esp_err_to_name(ret));

// 칩 통신 오류
T_LOGE(TAG, "I2C 통신 실패: addr=0x%02X", i2c_addr);

// 리소스 부족
T_LOGE(TAG, "메모리 할당 실패: %zu bytes", size);
```

#### WARN - 항상 출력

```cpp
// 설정 없음, 기본값 사용
T_LOGW(TAG, "NVS에 설정 없음, 기본값 사용");

// 디바이스 없음
T_LOGW(TAG, "연결된 디바이스 없음");

// 재시도
T_LOGW(TAG, "연결 실패, 재시도 (%d/%d)", retry, MAX_RETRY);
```

#### INFO - 기본 출력 (상태 변화)

```cpp
// 시스템 시작
T_LOGI(TAG, "부팅 완료");
T_LOGI(TAG, "서비스 시작");

// 연결 상태 변화
T_LOGI(TAG, "WiFi 연결됨: %s", ssid);
T_LOGI(TAG, "스위처 연결됨: %s", switcher_name);

// 사용자 동작
T_LOGI(TAG, "디바이스 등록: %s", device_id);
T_LOGI(TAG, "설정 저장 완료");
```

#### DEBUG - 선택 출력

```cpp
// 이벤트 발행 (❌ INFO에서 DEBUG로 변경 필요)
T_LOGD(TAG, "이벤트 발행: %s", event_name);

// 상태 변경 디테일
T_LOGD(TAG, "상태 변경: %d -> %d", old_value, new_value);

// 함수 진입/퇴출
T_LOGD(TAG, "진입: %s", __FUNCTION__);
```

### 2.3 레벨 변경 필요 사항

| 파일 | 현재 | 제안 | 개수 |
|------|------|------|------|
| `prod_rx_app.cpp` | INFO | DEBUG | 4개 |
| `prod_tx_app.cpp` | INFO | DEBUG | 8개 |
| `config_service.cpp` | INFO | DEBUG | 3개 |
| `DisplayManager.cpp` | **INFO (5초 간격)** | **제거 또는 DEBUG** | 10+ |

---

## 3. 로그 메시지 형식

### 3.1 표준 템플릿

```cpp
// 에러: 실패 원인 포함
T_LOGE(TAG, "{작업} 실패: {원인}");
T_LOGE(TAG, "I2C 읽기 실패: 0x%02X", ret);

// 경고: 대체 값 사용 시
T_LOGW(TAG, "{설정} 없음, 기본값 사용");
T_LOGW(TAG, "NVS에 brightness 없음, 기본값 50 사용");

// 정보: 상태 변화
T_LOGI(TAG, "{상태} 완료: {결과}");
T_LOGI(TAG, "WiFi 연결 완료: %s", ip_address);

// 디버그: 상세 추적
T_LOGD(TAG, "{함수}: {값}");
T_LOGD(TAG, "process_packet: header=0x%02X, len=%d", header, len);
```

### 3.2 메시지 작성 원칙

1. **한글 사용** (일관성)
2. **100자 이내** (가독성)
3. **민감 정보 노출 금지** (비밀번호, 토큰 등)
4. **불필요한 로그 제거** (루프 내 반복)

### 3.3 나쁜 예시

```cpp
// ❌ 너무 김
T_LOGI(TAG, "RF 설정 저장: %.1f MHz, Sync 0x%02X, SF%d, CR%d, BW%.0f, TXP%ddBm (NVS)",
       frequency, sync_word, sf, cr, bw, tx_power);

// ❌ 민감 정보 노출 가능성
T_LOGI(TAG, "WiFi 연결: ssid=%s, password=%s", ssid, password);

// ❌ 루프 내 반복
while (1) {
    T_LOGI(TAG, "처리 중...");  // 매번 출력
}

// ❌ 영어/한글 혼용
T_LOGI(TAG, "Config saved: 타입=%d", type);
```

### 3.4 좋은 예시

```cpp
// ✅ 적절한 길이
T_LOGI(TAG, "RF 설정 저장: %.1f MHz, SF%d", frequency, sf);
T_LOGD(TAG, "RF 상세: Sync=0x%02X, CR=4/%d, BW=%.0fkHz",
       sync_word, cr, bw);

// ✅ 민감 정보 보호
T_LOGI(TAG, "WiFi 연결: ssid=%s, password_set=%d",
       ssid, (password != NULL && strlen(password) > 0));

// ✅ 상태 변화 시에만 출력
static uint32_t last_log_time = 0;
if (now - last_log_time > 30000) {  // 30초 간격
    T_LOGI(TAG, "처리 중: count=%d", count);
    last_log_time = now;
}

// ✅ 일관된 한글 사용
T_LOGI(TAG, "설정 저장 완료: 타입=%d", type);
```

---

## 4. 함수 단위 로그 리팩토링

### 4.1 리팩토링 원칙

함수별로 로그를 검토하고, **필요한 로그는 일관성 있게 추가**, **불필요한 로그는 제거**, **적절한 레벨 할당**

### 4.2 함수 유형별 로그 가이드

#### 초기화 함수

```cpp
esp_err_t my_component_init(void) {
    T_LOGI(TAG, "초기화 시작");  // INFO: 진입 로그

    ret = step1();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "step1 실패: %s", esp_err_to_name(ret));  // ERROR
        return ret;
    }

    ret = step2();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "step2 실패, 기본값 사용");  // WARN: 복구 가능
        // 기본값 적용...
    }

    T_LOGI(TAG, "초기화 완료");  // INFO: 성공
    return ESP_OK;
}
```

#### 상태 변경 함수

```cpp
void set_brightness(int brightness) {
    if (brightness < 0 || brightness > 100) {
        T_LOGW(TAG, "밝기 범위 초과: %d, 0~100으로 제한", brightness);
        brightness = 50;  // 기본값
    }

    if (s_brightness == brightness) {
        T_LOGD(TAG, "밝기 변화 없음: %d", brightness);  // DEBUG: 무시
        return;
    }

    int old = s_brightness;
    s_brightness = brightness;

    T_LOGI(TAG, "밝기 변경: %d -> %d", old, brightness);  // INFO: 상태 변화
}
```

#### 반복 호출 함수 (루프)

```cpp
// ❌ 나쁨: 매번 로그 출력
void process_packet(packet_t* pkt) {
    T_LOGI(TAG, "패킷 처리: len=%d", pkt->len);  // 매번!
    // ...
}

// ✅ 좋음: 오류/경고만, 상세는 DEBUG
void process_packet(packet_t* pkt) {
    if (pkt->len > MAX_LEN) {
        T_LOGW(TAG, "패킷 길이 초과: %d > %d", pkt->len, MAX_LEN);
        return;
    }

    T_LOGD(TAG, "패킷 처리: header=0x%02X, len=%d", pkt->header, pkt->len);
    // ...
}
```

#### 이벤트 발행 함수

```cpp
void publish_brightness_event(int brightness) {
    brightness_event_t evt = { .brightness = brightness };

    // ❌ 이벤트 발행 로그는 DEBUG로
    // T_LOGI(TAG, "밝기 이벤트 발행: %d", brightness);

    T_LOGD(TAG, "이벤트 발행: EVT_BRIGHTNESS, value=%d", brightness);  // DEBUG
    event_bus_publish(EVT_BRIGHTNESS, &evt, sizeof(evt));
}
```

### 4.3 로그 배치 검토 체크리스트

함수별로 다음 항목을 확인:

| 항목 | 확인 내용 |
|------|-----------|
| 진입/퇴출 | 초기화/종료 함수만 INFO, 나머지는 DEBUG |
| 성공 | 중요 상태 변화만 INFO |
| 실패 | 모두 ERROR, 복구 가능하면 WARN |
| 반복 호출 | 루프 내는 DEBUG, 루프 외는 INFO |
| 이벤트 발행 | 모두 DEBUG |
| 디버깅용 | 모두 DEBUG 또는 VERBOSE |
| 조건부 로그 | 상태 변화 시에만 출력 |

### 4.4 주석 처리된 로그 처리

```cpp
// ❌ 나쁨: 주석 처리된 로그 방치
// ESP_LOGI(TAG, "debug info: %d", value);

// ✅ 좋음 1: T_LOGD로 활성화
T_LOGD(TAG, "debug info: %d", value);

// ✅ 좋음 2: 완전 제거 (필요 없는 경우)
// 삭제
```

### 4.5 리팩토링 순서

1. **TAG 변경** → `{LayerNumber}_{ComponentName}`
2. **불필요한 로그 제거** → 주석 처리된 것, 반복 출력
3. **로그 레벨 조정** → 이벤트 발행 → DEBUG
4. **누락된 로그 추가** → 에러 처리, 상태 변화
5. **메시지 형식 통일** → 한글, 100자 이내

---

## 5. 개선 우선순위

| 순위 | 작업 | 파일 | 영향 | 예상 시간 |
|------|------|------|------|----------|
| 1 | 5초 간격 상태 로그 제거 | `DisplayManager.cpp` | 로그 50% 감소 | 10분 |
| 2 | 이벤트 발행 로그 → DEBUG | 전체 | 로그 20% 감소 | 20분 |
| 3 | 시작 로그 정리 | App, Service | 부팅 로그 간소화 | 15분 |
| 4 | TAG 명명 통일 | 전체 | 일관성 향상 | 30분 |
| 5 | 메시지 형식 표준화 | 전체 | 가독성 향상 | 40분 |

---

## 6. 현재 문제점

### 6.1 TAG 명명 불일치

- camelCase (`prod_rx_app`)와 PascalCase (`EventBus`) 혼용
- 약어 사용 (`Mgr`, `Svc`, `HAL`)
- 대문자 상수형 (`DISP_HAL`, `TEMP_HAL`)
- **개선**: `01_TxApp`, `03_Device` 형식으로 통일

### 6.2 로그 레벨 오사용

- 이벤트 발행 로그가 INFO로 출력 (DEBUG로 변경 필요)
- 5초마다 반복 출력되는 상태 로그
- 시작 로그 과다 출력

### 6.3 메시지 형식 불일치

- 한글/영어 혼용
- 100자 이상 긴 로그
- 민감 정보 노출 가능성

---

## 참고 자료

- `include/LogConfig.h` - T_LOG 설정
- `sdkconfig.defaults` - ESP-IDF 로그 설정
