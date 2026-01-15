# SPEC-REFACTOR-004: 인수 기준

## TAG BLOCK

```
TAG: SPEC-REFACTOR-004
STATUS: Planned
DEFINITION_OF_DONE: Complete
```

## 1. Definition of Done

### 1.1 구현 완료 기준

- [ ] 모든 REQ-001 ~ REQ-010 요구사항이 구현되었음
- [ ] 코드 리뷰가 완료되었음
- [ ] 모든 테스트 시나리오가 통과하였음
- [ ] 컴파일 경고가 없음
- [ ] LOG_SYSTEM_REVIEW.md 문서가 업데이트되었음

### 1.2 품질 게이트

| 항목 | 기준 | 상태 |
|------|------|------|
| 빌드 성공 | TX/RX 모드 모두 성공 | [ ] |
| 컴파일 경고 | 0건 | [ ] |
| ESP_LOG 잔재 | Grep 결과 0건 | [ ] |
| 코드 커버리지 | 수정된 파일 테스트覆盖 | [ ] |
| 코딩 표준 | 영문 로그, 한국어 주석 | [ ] |

## 2. 테스트 시나리오 (Given-When-Then)

### 2.1 ESP_ERROR_CHECK 표준화

**Scenario 1: 커스텀 ESP_ERROR_CHECK 제거**

```gherkin
Given u8g2_esp32_hal.c 파일에 커스텀 ESP_ERROR_CHECK 매크로가 정의되어 있고
  And esp_log.h가 include되어 있을 때

When 개발자가 커스텀 ESP_ERROR_CHECK 매크로를 제거하고
  And t_log.h를 include 추가하면

Then u8g2_esp32_hal.c는 t_log.h의 ESP_ERROR_CHECK를 사용하고
  And esp_log.h include가 제거되며
  And 빌드가 성공해야 한다
```

**Scenario 2: ESP_ERROR_CHECK 동작 검증**

```gherkin
Given t_log.h의 ESP_ERROR_CHECK 매크로가 사용 가능하고

When ESP_ERROR_CHECK가 ESP_OK가 아닌 에러 코드를 받으면

Then T_LOGE로 에러가 로깅되고
  And abort()가 호출되어야 한다
```

### 2.2 Hex Dump 매크로

**Scenario 3: T_LOGI_HEX 매크로 추가**

```gherkin
Given t_log.h에 hex dump 매크로가 추가되지 않은 상태에서

When 개발자가 T_LOG_BUFFER_HEX와 T_LOGI_HEX 매크로를 추가하면

Then 16바이트 단위로 hex dump가 출력되고
  And 주소 오프셋이 표시되며
  And 모든 로그 레벨에 대한 매크로가 제공되어야 한다
```

**Scenario 4: Hex Dump 출력 형식**

```gherkin
Given 테스트 데이터가 주어지고

```c
uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
T_LOGI_HEX("TEST", data, sizeof(data));
```

When T_LOGI_HEX 매크로가 호출되면

Then 다음 형식으로 출력되어야 한다:
  | 출력 | 기대값 |
  |------|--------|
  | 라인 1 | "0000: 01 02 03 04 05 " |
```

### 2.3 ESP_LOG_BUFFER_HEXDUMP 대체

**Scenario 5: u8g2_hal hex dump 대체**

```gherkin
Given u8g2_esp32_hal.c에서 ESP_LOG_BUFFER_HEXDUMP가 사용되고 있고

When 개발자가 ESP_LOG_BUFFER_HEXDUMP를 T_LOGI_HEX로 대체하면

Then I2C 데이터 hex dump가 정상 출력되고
  And 동일한 디버깅 정보가 제공되어야 한다
```

**Scenario 6: 조건부 컴파일 검증**

```gherkin
Given T_LOG_DEFAULT_LEVEL이 T_LOG_INFO로 설정되어 있고

When T_LOGI_HEX 매크로가 호출되면

Then hex dump가 출력되고
  And T_LOGD_HEX 매크로는 출력되지 않아야 한다
```

### 2.4 Include 제거

**Scenario 7: 미사용 esp_log.h 제거**

```gherkin
Given 10개 파일에 esp_log.h include가 존재하고
  And 해당 파일에서 ESP_LOG 매크로가 사용되지 않을 때

When 개발자가 esp_log.h include를 제거하면

Then 각 파일이 정상 컴파일되고
  And 링크 오류가 발생하지 않아야 한다
```

**Scenario 8: Include 제거 대상 파일 검증**

```gherkin
Given 다음 파일들이 주어지고
  | 파일 |
  | web_server_config.cpp |
  | web_server_events.cpp |
  | web_server_json.cpp |
  | api_status.cpp |
  | api_license.cpp |
  | api_devices.cpp |
  | api_lora.cpp |
  | battery_hal.c |
  | ws2812_hal.c |
  | display_hal.c |

When 각 파일을 분석하면

Then ESP_LOG 매크로 사용이 없고
  And esp_log.h include 제거가 안전해야 한다
```

### 2.5 정적 분석

**Scenario 9: ESP_LOG 잔재 검증**

```gherkin
Given 모든 수정이 완료된 상태에서

When 개발자가 ESP_LOG 패턴으로 Grep 검색을 수행하면

Then 검색 결과가 0건이어야 한다:
  | 검색 패턴 | 기대 결과 |
  | ESP_LOGE | 0건 |
  | ESP_LOGW | 0건 |
  | ESP_LOGI | 0건 |
  | ESP_LOGD | 0건 |
  | ESP_LOGV | 0건 |
  | ESP_LOG_BUFFER | 0건 |
```

**Scenario 10: esp_log.h include 검증**

```gherkin
Given 전체 코드베이스에서

When 개발자가 esp_log.h include로 Grep 검색을 수행하면

Then 검색 결과가 0건이어야 한다
```

## 3. 회귀 테스트

### 3.1 기능 회귀 방지

| 기능 | 테스트 방법 | 기대 결과 |
|------|-----------|-----------|
| I2C 디스플레이 초기화 | u8g2 HAL 초기화 호출 | 성공 |
| I2C 데이터 전송 | U8X8_MSG_BYTE_SEND 처리 | 정상 동작 |
| 에러 처리 | ESP_ERROR_CHECK 동작 | abort 호출 |
| 로그 출력 | T_LOG 매크로 호출 | UART 출력 |

### 3.2 성능 회귀 방지

| 항목 | 측정 방법 | 기준 |
|------|-----------|------|
| 빌드 시간 | `pio run -e eora_s3_tx` | 변화 없음 |
| Flash 사용량 | 빌드 출력 | 증가 < 1KB |
| RAM 사용량 | 런타임 측정 | 변화 없음 |

## 4. 수동 테스트 체크리스트

### 4.1 빌드 테스트

```bash
# TX 모드 클린 빌드
pio run -e eora_s3_tx --target clean
pio run -e eora_s3_tx
# 기대: 빌드 성공, 경고 0건

# RX 모드 클린 빌드
pio run -e eora_s3_rx --target clean
pio run -e eora_s3_rx
# 기대: 빌드 성공, 경고 0건
```

### 4.2 런타임 테스트

```bash
# TX 모드 플래싱 및 모니터링
pio run -e eora_s3_tx --target upload
pio device monitor -b 921600
# 기대: 부팅 로그 정상 출력, ESP_LOG 에러 없음

# RX 모드 플래싱 및 모니터링
pio run -e eora_s3_rx --target upload
pio device monitor -b 921600
# 기대: 부팅 로그 정상 출력, OLED 동작
```

### 4.3 Grep 검증

```bash
# ESP_LOG 잔재 검증
grep -r "ESP_LOG" components/ --include="*.c" --include="*.cpp"
# 기대: T_LOG 관련만 출력

# esp_log.h include 검증
grep -r "esp_log.h" components/ --include="*.c" --include="*.cpp"
# 기대: 0건
```

## 5. 문서화 기준

### 5.1 LOG_SYSTEM_REVIEW.md 업데이트

```markdown
## Action Items 완료 상태

- [x] u8g2_esp32_hal.c ESP_ERROR_CHECK 수정
- [x] t_log hex dump 매크로 추가
- [x] ESP_LOG_BUFFER_HEXDUMP 대체
- [x] 미사용 esp_log.h include 제거 (10개 파일)

## 검증 결과

- ESP_LOG 잔재: 0건
- esp_log.h include: 0건
- 빌드 상태: 정상
```

### 5.2 코드 주석

```c
// I2C 데이터 hex dump 출력 (디버깅용)
T_LOGI_HEX(TAG, data_ptr, arg_int);
```

## 6. 승인 기준

### 6.1 최종 승인 조건

- [ ] 모든 테스트 시나리오 통과
- [ ] 빌드 경고 0건
- [ ] 기능 회귀 없음
- [ ] 문서 업데이트 완료
- [ ] 코드 리뷰 승인

### 6.2 릴리스 전 확인

- [ ] TX 모드 실제 장치 부팅 테스트
- [ ] RX 모드 실제 장치 부팅 테스트
- [ ] OLED 디스플레이 동작 확인

---

*Acceptance 버전: 1.0.0*
*생성일: 2026-01-15*
