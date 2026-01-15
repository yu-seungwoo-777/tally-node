# SPEC-REFACTOR-004: 구현 계획

## TAG BLOCK

```
TAG: SPEC-REFACTOR-004
STATUS: Planned
PRIORITY: Medium
MILESTONE: Code Quality Improvement
```

## 1. 마일스톤 (우선순위 기반)

### Milestone 1: Primary Goal (필수)

**목표:** ESP_LOG 사용을 완전히 제거하고 t_log 표준화 완료

- [ ] u8g2_esp32_hal.c ESP_ERROR_CHECK 매크로 제거
- [ ] t_log.h hex dump 매크로 추가
- [ ] u8g2_esp32_hal.c ESP_LOG_BUFFER_HEXDUMP 대체
- [ ] u8g2_esp32_hal.c esp_log.h include 제거

### Milestone 2: Secondary Goal (중요)

**목표:** 미사용 esp_log.h include 일괄 제거

- [ ] web_server 관련 6개 파일 esp_log.h 제거
- [ ] battery_hal esp_log.h 제거
- [ ] ws2812_hal esp_log.h 제거
- [ ] display_hal esp_log.h 제거

### Milestone 3: Final Goal (검증)

**목표:** 빌드 및 기능 검증

- [ ] TX 모드 빌드 성공 확인
- [ ] RX 모드 빌드 성공 확인
- [ ] ESP_LOG 잔재 Grep 검증 (0건)
- [ ] LOG_SYSTEM_REVIEW.md 문서 업데이트

## 2. 기술적 접근 방법

### 2.1 아키텍처 고려사항

**의존성 방향:**
```
u8g2_hal (05_hal)
    ↓ 의존
t_log (00_common)
```

- t_log는 00_common 계층으로, 모든 계층에서 의존 가능
- Circular dependency 없음
- 기존 아키텍처 유지

### 2.2 수정 순서

1. **t_log.h 수정** (가장 하위 계층)
   - hex dump 매크로 추가
   - 컴파일 검증

2. **u8g2_esp32_hal.c 수정** (의존성 해결)
   - 커스텀 ESP_ERROR_CHECK 제거
   - ESP_LOG_BUFFER_HEXDUMP 대체
   - esp_log.h include 제거
   - t_log.h include 추가

3. **일괄 include 제거** (독립적 작업)
   - 10개 파일에서 esp_log.h 제거
   - 병렬 처리 가능

### 2.3 빌드 검증 전략

```bash
# 1. t_log.h 수정 후 검증
pio run -e eora_s3_tx

# 2. u8g2_hal 수정 후 검증
pio run -e eora_s3_tx
pio run -e eora_s3_rx

# 3. 전체 빌드 검증
pio run -e eora_s3_tx --target clean
pio run -e eora_s3_tx
pio run -e eora_s3_rx
```

## 3. 위험 및 대응 계획

### 3.1 식별된 위험

| 위험 | 확률 | 영향 | 완화 방법 |
|------|------|------|-----------|
| t_log.h hex dump 매크로 버그 | Medium | Low | 단위 테스트로 검증 |
| include 제거로 인한 컴파일 오류 | Low | Medium | 일괄 제거 후 빌드 검증 |
| u8g2_hal 기능 회귀 | Low | High | I2C 디스플레이 동작 테스트 |

### 3.2 롤백 계획

- Git 커밋을 마일스톤별로 분리
- 각 마일스톤 완료 후 커밋
- 문제 발생 시 해당 마일스톤으로 revert

## 4. 구현 세부사항

### 4.1 t_log.h 수정 상세

**파일:** `components/00_common/t_log/include/t_log.h`

**추가 위치:** T_LOG 매크로 그룹 하단 (라인 ~100)

**구현:**
```c
// ============================================================================
// Hex Dump Macros (ESP_LOG_BUFFER_HEXDUMP 대체)
// ============================================================================

#define T_LOG_BUFFER_HEX(level, tag, data, len) do { \
    if (level <= T_LOG_DEFAULT_LEVEL) { \
        const uint8_t* _data = (const uint8_t*)(data); \
        for (int i = 0; i < (len); i += 16) { \
            char _hex[64]; \
            int _pos = 0; \
            for (int j = 0; j < 16 && (i + j) < (len); j++) { \
                _pos += snprintf(_hex + _pos, sizeof(_hex) - _pos, "%02X ", _data[i + j]); \
            } \
            t_log_output(level, tag, "%04X: %s", i, _hex); \
        } \
    } \
} while(0)

// 편의 매크로 (레벨별)
#define T_LOGE_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_ERROR, tag, data, len)
#define T_LOGW_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_WARN, tag, data, len)
#define T_LOGI_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_INFO, tag, data, len)
#define T_LOGD_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_DEBUG, tag, data, len)
#define T_LOGV_HEX(tag, data, len) T_LOG_BUFFER_HEX(T_LOG_VERBOSE, tag, data, len)
```

### 4.2 u8g2_esp32_hal.c 수정 상세

**파일:** `components/05_hal/display_hal/u8g2_hal/u8g2_esp32_hal.c`

**변경 1: Include 수정**
```c
// 제거
#include "esp_log.h"

// 추가
#include "t_log.h"
```

**변경 2: ESP_ERROR_CHECK 제거 (라인 22-30)**
- 전체 블록 삭제

**변경 3: ESP_LOG_BUFFER_HEXDUMP 대체 (라인 ~142)**
```c
// 기존
ESP_LOG_BUFFER_HEXDUMP(TAG, data_ptr, arg_int, ESP_LOG_VERBOSE);

// 변경
T_LOGI_HEX(TAG, data_ptr, arg_int);
```

### 4.3 Include 제거 스크립트

**자동화 가능:**
```bash
# 미사용 esp_log.h include 검증 및 제거
for file in $(grep -l "esp_log.h" components/02_presentation/web_server/**/*.cpp \
                      components/05_hal/battery_hal/*.c \
                      components/05_hal/ws2812_hal/*.c \
                      components/05_hal/display_hal/*.c); do
    # ESP_LOG 매크로 사용 확인
    if ! grep -q "ESP_LOG" "$file"; then
        sed -i '/#include.*esp_log.h/d' "$file"
        echo "Removed esp_log.h from $file"
    fi
done
```

## 5. 검증 기준

### 5.1 정적 검증

- [ ] Grep "ESP_LOG" 결과: 0건 (T_LOG 제외)
- [ ] Grep "esp_log.h" 결과: 0건
- [ ] 컴파일 경고: 0건

### 5.2 동적 검증

- [ ] TX 모드 부팅 성공
- [ ] RX 모드 부팅 성공
- [ ] OLED 디스플레이 정상 작동
- [ ] 로그 출력 정확성 확인

### 5.3 코드 품질

- [ ] ruff linter 통과
- [ ] 코딩 표준 준수 (영문 로그, 한국어 주석)

## 6. 다음 단계

**명령어:**
```bash
# 구현 시작
/moai:2-run SPEC-REFACTOR-004

# 문서 동기화
/moai:3-sync SPEC-REFACTOR-004
```

**Git 워크플로우:**
```bash
# 브랜치 생성
git checkout -b feature/SPEC-REFACTOR-004

# 커밋 메시지
git commit -m "refactor(log): ESP_LOG 제거 및 t_log 표준화

- 커스텀 ESP_ERROR_CHECK 매크로 제거
- t_log.h hex dump 매크로 추가
- 미사용 esp_log.h include 제거 (10개 파일)

Ref: SPEC-REFACTOR-004"
```

---

*Plan 버전: 1.0.0*
*생성일: 2026-01-15*
