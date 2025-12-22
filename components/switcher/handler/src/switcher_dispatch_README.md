# Switcher Dispatch 매크로

## 개요

Handler의 반복되는 `switch/case` 패턴을 매크로로 추상화하여 코드 중복을 제거합니다.

## 구조

```
switcher_dispatch.h
├── SWITCHER_GET_ATEM(sw)     - ATEM 클라이언트 포인터
├── SWITCHER_GET_VMIX(sw)     - vMix 클라이언트 포인터
├── SWITCHER_GET_OBS(sw)      - OBS 클라이언트 포인터
├── SWITCHER_DISPATCH_INT()   - int 반환 함수 분기
├── SWITCHER_DISPATCH_UINT8() - uint8_t 반환 함수 분기
├── SWITCHER_DISPATCH_UINT16() - uint16_t 반환 함수 분기
├── SWITCHER_DISPATCH_UINT64() - uint64_t 반환 함수 분기
└── SWITCHER_DISPATCH_BOOL()  - bool 반환 함수 분기
```

## 사용 예시

### Before (반복 코드)

```c
int switcher_loop(switcher_t* sw)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
#ifdef SWITCHER_ENABLE_ATEM
        case SWITCHER_TYPE_ATEM:
            return atem_client_loop(&sw->backend.atem);
#endif
#ifdef SWITCHER_ENABLE_VMIX
        case SWITCHER_TYPE_VMIX:
            return vmix_client_loop(&sw->backend.vmix);
#endif
#ifdef SWITCHER_ENABLE_OBS
        case SWITCHER_TYPE_OBS:
            return obs_client_loop(&sw->backend.obs);
#endif
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}
```

### After (매크로 사용)

```c
int switcher_loop(switcher_t* sw)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;
    SWITCHER_DISPATCH_INT(sw, loop);
}
```

**코드 라인: 17줄 → 3줄 (약 82% 감소)**

## 장점

### 1. **코드 간결화**
- 20+ 함수에서 반복되던 switch/case 제거
- 가독성 향상

### 2. **유지보수성**
- 새 프로토콜 추가 시 매크로만 수정
- 일관된 에러 처리

### 3. **컴파일 타임 최적화**
- 매크로 확장 → 인라인 최적화 가능
- 런타임 오버헤드 없음

### 4. **조건부 컴파일 지원**
- `#ifdef SWITCHER_ENABLE_ATEM` 자동 처리
- 불필요한 프로토콜 제외 가능

## 적용된 함수 목록

### 연결/루프
- `switcher_loop()` ✅
- `switcher_is_connected()` ✅

### Tally 조회
- `switcher_get_tally()` ✅
- `switcher_get_tally_packed()` ✅

### 제어 명령
- (추가 예정: `switcher_cut()`, `switcher_auto()` 등)

## 매크로 규칙

### 함수명 매핑
```
SWITCHER_DISPATCH_INT(sw, loop)
→ atem_client_loop(SWITCHER_GET_ATEM(sw))
→ vmix_client_loop(SWITCHER_GET_VMIX(sw))
→ obs_client_loop(SWITCHER_GET_OBS(sw))
```

### 인자 전달
```
SWITCHER_DISPATCH_UINT8(sw, get_tally_by_index, index)
→ atem_client_get_tally_by_index(SWITCHER_GET_ATEM(sw), index)
```

## 한계 및 고려사항

### 1. **함수 시그니처 통일 필요**
모든 프로토콜의 함수가 **동일한 이름과 인자**를 가져야 함
```c
// ✅ 가능
atem_client_loop(atem_client_t*)
vmix_client_loop(vmix_client_t*)
obs_client_loop(obs_client_t*)

// ❌ 불가능 (인자 다름)
atem_client_set_program(atem_client_t*, uint16_t, uint8_t me)
vmix_client_set_program(vmix_client_t*, uint16_t)
```

### 2. **복잡한 로직은 수동 작성**
Protocol별 특수 처리가 필요한 경우 매크로 사용 불가
```c
// 수동 switch/case 유지
int switcher_set_program(switcher_t* sw, uint16_t input) {
    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return atem_client_set_program_input(&sw->backend.atem, input, 0);
        case SWITCHER_TYPE_OBS:
            // OBS는 0-based, input은 1-based
            if (input == 0) return SWITCHER_ERROR_INVALID_PARAM;
            return obs_client_set_program_scene(&sw->backend.obs, input - 1);
    }
}
```

### 3. **디버깅**
매크로 확장 코드는 GDB에서 보기 어려움
- 빌드 시 `-E` 플래그로 전처리 결과 확인 가능

## 참고

- 작성일: 2025-12-01
- 파일: `components/switcher/handler/src/switcher_dispatch.h`
- 적용: `components/switcher/handler/src/switcher.c`
