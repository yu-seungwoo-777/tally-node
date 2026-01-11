# Tally Node 코드 리뷰

> **최종 업데이트**: 2025-01-11
> **전체 파일**: 233개 (C/C++/H)

---

## 📋 리뷰 진행 현황

| 레이어 | 컴포넌트 | 상태 | 비고 | 날짜 |
|--------|----------|------|------|------|
| 05_hal | battery_hal | ✅ 완료 | DEBUG 로그, 간결 메시지 | 2025-01-11 |
| 05_hal | display_hal | ✅ 완료 | DEBUG 로그, 간결 메시지 | 2025-01-11 |
| 05_hal | ethernet_hal | ✅ 완료 | DEBUG 로그, 간결 메시지 | 2025-01-11 |
| 05_hal | lora_hal | ⏭️ 건너뜀 | RadioLib (이미 잘 작성됨) | 2025-01-11 |
| 05_hal | temperature_hal | ✅ 완료 | DEBUG 로그, 간결 메시지 | 2025-01-11 |
| 05_hal | wifi_hal | ✅ 완료 | DEBUG 로그, 간결 메시지 | 2025-01-11 |
| 05_hal | ws2812_hal | ✅ 완료 | DEBUG 로그, 간결 메시지 | 2025-01-11 |
| 05_hal | u8g2_hal | ⏭️ 건너뜀 | 라이브러리 래퍼 | 2025-01-11 |
| 04_driver | battery_driver | ✅ 완료 | DEBUG 로그, 간결 메시지 | 2025-01-11 |
| 04_driver | temperature_driver | ✅ 완료 | DEBUG 로그, 간결 메시지 | 2025-01-11 |
| 04_driver | ws2812_driver | ✅ 완료 | DEBUG 로그, 간결 메시지 | 2025-01-11 |
| 04_driver | board_led_driver | ⬜ 예정 | | |
| 04_driver | display_driver | ⬜ 예정 | | |
| 04_driver | ethernet_driver | ⬜ 예정 | | |
| 04_driver | license_client | ⬜ 예정 | | |
| 04_driver | lora_driver | ⬜ 예정 | | |
| 04_driver | switcher_driver | ⬜ 예정 | | |
| 04_driver | wifi_driver | ⬜ 예정 | | |
| 03_service | button_service | ⬜ 예정 | | |
| 03_service | config_service | ⬜ 예정 | | |
| 03_service | device_manager | ⬜ 예정 | | |
| 03_service | hardware_service | ⬜ 예정 | | |
| 03_service | led_service | ⬜ 예정 | | |
| 03_service | license_service | ⬜ 예정 | | |
| 03_service | lora_service | ⬜ 예정 | | |
| 03_service | network_service | ⬜ 예정 | | |
| 03_service | switcher_service | ⬜ 예정 | | |
| 03_service | tally_test_service | ⬜ 예정 | | |
| 02_presentation | display | ⬜ 예정 | | |
| 02_presentation | web_server | ⬜ 예정 | | |
| 01_app | prod_rx_app | ⬜ 예정 | | |
| 01_app | prod_tx_app | ⬜ 예정 | | |
| 00_common | event_bus | ⬜ 예정 | | |
| 00_common | lora_protocol | ⬜ 예정 | | |
| 00_common | t_log | ⬜ 예정 | | |
| 00_common | tally_types | ⬜ 예정 | | |

**범례**:
- ⬜ 예정 - 리뷰 전
- 🔄 진행 중 - 현재 리뷰 중
- ✅ 완료 - 리뷰 완료
- ⚠️  수정 필요 - 이슈 발견
- ⏭️  건너뜀 - 리뷰 생략

---

## 🎯 리뷰 작업 표준

### 주석 작성 (한글)
- 함수 동작 설명
- 파라미터/반환값 설명
- 복잡한 로직에 대한 설명
- 주요 상수/매직 넘버 설명

### T_LOG 작성 (영문)

**로그 레벨 정책 (현재)**:
| 레이어 | DEBUG | INFO | ERROR |
|--------|-------|------|-------|
| 05_hal | O | - | O |
| 04_driver | O (간결) | - | O |
| 03_service | O | - | O |
| 02_presentation | O | - | O |
| 01_app | O | - | O |

> **INFO 로그는 추후 서비스 계층부터 추가 예정**

**DRIVER 계층 DEBUG 로그 패턴 (간결하게)**:
```c
// 함수 진입
T_LOGD(TAG, "init");
T_LOGD(TAG, "getVoltage");

// 성공
T_LOGD(TAG, "ok");
T_LOGD(TAG, "ok:%.2fV", voltage);

// 에러 (ERROR 레벨)
T_LOGE(TAG, "fail:0x%x", ret);
T_LOGE(TAG, "fail:null");
```

---

## 📝 리뷰 노트

### 05_hal 레이어 완료 (2025-01-11)

**완료된 HAL 컴포넌트**:
1. **battery_hal** - ADC 캘리브레이션, 전압 측정
2. **temperature_hal** - ESP32-S3 내장 온도 센서
3. **wifi_hal** - AP/STA 모드, 스캔, 이벤트 처리
4. **ethernet_hal** - W5500 Ethernet, SPI, DNS 설정
5. **ws2812_hal** - RMT 드라이버, WS2812 LED 제어
6. **display_hal** - I2C 인터페이스, 전원 제어
7. **lora_hal** - RadioLib HAL 구현 (이미 잘 작성됨)
8. **u8g2_hal** - U8g2 라이브러리 래퍼

**적용된 개선사항**:
- 한글 함수/파라미터 주석 추가
- 영문 T_LOG 로그 메시지
- ESP-IDF 에러 코드 포함 (`esp_err_to_name(ret)`, `0x%x`)
- NULL 포인터 검사
- 매직 넘버를 상수로 정의

---

## 🔍 리뷰 항목별 상세

### 1. 메모리 안전성
| 항목 | 설명 |
|------|------|
| 동적 할당 | malloc/calloc 사용 후 free 확인 |
| 버퍼 경계 | strncpy, snprintf 사용 및 null 종료 확인 |
| 스택 사용 | 대용량 배열이 힙에 할당되었는가 |

### 2. 동시성
| 항목 | 설명 |
|------|------|
| 레이스 컨디션 | 공유 변수의 mutex/atomic 보호 |
| 데드락 | lock 순서 일관성 |
| 인터럽트 안전성 | ISR에서 호출 가능한 함수만 사용 |

### 3. 에러 처리
| 항목 | 설명 |
|------|------|
| 반환값 검사 | ESP_OK, NULL 체크 |
| 에러 전파 | 상위 계층으로 에러 반환 |
| 복구 동작 | 에러 발생 시 정리 처리 |

### 4. 코드 품질
| 항목 | 설명 |
|------|------|
| 중복 제거 | 동일 로직의 함수화 |
| 함수 길이 | 너무 긴 함수 분리 (100줄 이하 권장) |
| 매직 넘버 | 상수/매크로로 정의 |

---

## 📝 리뷰 템플릿

```markdown
## [컴포넌트명] 리뷰

**파일**: `components/XX_YY/zz/zz.cpp`
**날짜**: YYYY-MM-DD

### 관찰사항
(리뷰 내용)

### 이슈
- [ ] 이슈1
- [ ] 이슈2

### 개선 제안
1. 주석 추가 (한글)
2. T_LOG 추가 (영문)
3. 에러 코드 포함

### 결론
- [ ] 승인
- [ ] 수정 필요
```

---

## 📊 통계

- **전체 컴포넌트**: 33개
- **리뷰 완료**: 8개 (24%)
- **05_hal 완료**: 8/8 (100%)
- **진행 중**: 0개
- **예정**: 25개
