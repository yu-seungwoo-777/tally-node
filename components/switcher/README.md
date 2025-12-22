# switcher

스위처 통합 라이브러리 - 순수 C (C99)

## 개요

여러 종류의 비디오 스위처를 통합 인터페이스로 제어하기 위한 라이브러리

### 지원 스위처

| 스위처 | 상태 | 프로토콜 | 포트 |
|--------|------|----------|------|
| ATEM (Blackmagic Design) | ✅ 완료 | UDP | 9910 |
| vMix | ✅ 완료 | TCP | 8099 |
| OBS Studio | ✅ 완료 | WebSocket | 4455 |
| OSEE | 예정 | TBD | - |

### ESP32-S3 호환성

| 라이브러리 | 메모리 | ESP32 지원 |
|------------|--------|------------|
| ATEM | 3.9 KB | ✅ |
| vMix | 3.6 KB | ✅ |
| OBS | 5.7 KB | ✅ |

> 모든 클라이언트가 6KB 이하로 ESP32-S3 SRAM(512KB)에서 여유롭게 동작

## ⚠️ 연결 유지 (Connection Keepalive)

**중요**: 모든 라이브러리는 자동으로 연결을 유지합니다. `*_loop()` 함수를 주기적으로 호출해야 합니다!

| 스위처 | Keepalive 방식 | 간격 | 타임아웃 |
|--------|---------------|------|---------|
| ATEM | ACK 패킷 | 1초 | 5초 |
| vMix | TALLY 요청 | 10초 | 30초 |
| OBS | WebSocket Ping | 10초 | 30초 |

```c
// 메인 루프에서 반드시 호출
while (running) {
    switcher_loop(sw);  // 연결 유지 + 데이터 수신
    delay(10);          // 10ms 대기
}
```

연결이 끊기면 `on_disconnected` 콜백이 호출됩니다.

## 구조

```
switcher/
├── libs/              # 개별 스위처 라이브러리
│   ├── atem/          # ATEM UDP 라이브러리
│   ├── vmix/          # vMix TCP 라이브러리
│   └── obs/           # OBS WebSocket 라이브러리
├── common/            # 공통 플랫폼 추상화
│   ├── include/       # sw_platform.h
│   └── platform/      # 플랫폼별 구현
├── handler/           # 통합 핸들러
│   ├── include/       # switcher.h
│   └── src/           # 구현
└── tests/             # 테스트 프로그램
```

## 빌드

```bash
# 전체 빌드
make

# 테스트 빌드
make test

# 클린
make clean

# 디버그 빌드
make debug
```

## 사용법

### 통합 핸들러 (권장)

```c
#include "switcher.h"

// 콜백
void on_tally_changed(uint64_t tally, void* user_data) {
    for (int i = 0; i < 8; i++) {
        uint8_t t = switcher_tally_get(tally, i);
        // SWITCHER_TALLY_OFF, SWITCHER_TALLY_PROGRAM, SWITCHER_TALLY_PREVIEW, SWITCHER_TALLY_BOTH
    }
}

int main() {
    // 스위처 생성
    switcher_t* sw = switcher_create(SWITCHER_TYPE_ATEM, "192.168.0.240", 0);

    // 콜백 설정
    switcher_callbacks_t callbacks = {
        .on_connected = NULL,
        .on_disconnected = NULL,
        .on_tally_changed = on_tally_changed,
        .on_state_changed = NULL,
        .user_data = NULL
    };
    switcher_set_callbacks(sw, &callbacks);

    // 연결
    switcher_connect(sw, 5000);
    switcher_wait_init(sw, 10000);

    // 메인 루프
    while (switcher_is_connected(sw)) {
        switcher_loop(sw);  // 필수!

        // 상태 조회
        uint16_t pgm = switcher_get_program(sw);
        uint16_t pvw = switcher_get_preview(sw);

        // 제어
        switcher_cut(sw);
        switcher_auto(sw);
        switcher_set_preview(sw, 2);
    }

    // 정리
    switcher_destroy(sw);
}
```

### 개별 라이브러리 (ATEM)

```c
#include "atem_client.h"

atem_client_t client;

atem_client_init(&client, "192.168.0.240", ATEM_DEFAULT_PORT);
atem_client_connect(&client, 5000);
atem_client_wait_init(&client, 10000);

while (atem_client_is_connected(&client)) {
    atem_client_loop(&client);

    uint8_t tally = atem_client_get_tally_by_index(&client, 0);
    atem_client_cut(&client, 0);
}

atem_client_cleanup(&client);
```

## 테스트

```bash
# 대화형 통합 테스트 (권장)
cd tests && make
./build/test_interactive

# 기본 테스트
./tests/build/test_switcher atem 192.168.0.240
./tests/build/test_switcher vmix 192.168.0.100
./tests/build/test_switcher obs 192.168.0.50
```

## API 요약

### 통합 핸들러

| 함수 | 설명 |
|------|------|
| `switcher_create()` | 스위처 생성 |
| `switcher_create_with_password()` | 스위처 생성 (OBS 비밀번호 지원) |
| `switcher_destroy()` | 스위처 해제 |
| `switcher_connect()` | 연결 |
| `switcher_disconnect()` | 연결 해제 |
| `switcher_wait_init()` | 초기화 대기 |
| `switcher_loop()` | 루프 처리 (필수) |
| `switcher_get_program()` | Program 입력 조회 |
| `switcher_get_preview()` | Preview 입력 조회 |
| `switcher_get_tally()` | Tally 상태 조회 |
| `switcher_cut()` | Cut 실행 |
| `switcher_auto()` | Auto 트랜지션 |
| `switcher_set_preview()` | Preview 변경 |
| `switcher_set_callbacks()` | 콜백 설정 |

### Tally 상태

| 값 | 설명 |
|----|------|
| `SWITCHER_TALLY_OFF` (0) | Off |
| `SWITCHER_TALLY_PROGRAM` (1) | Program (On Air) |
| `SWITCHER_TALLY_PREVIEW` (2) | Preview |
| `SWITCHER_TALLY_BOTH` (3) | Program + Preview |

## 라이선스

MIT License
