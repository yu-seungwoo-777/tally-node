# OBS WebSocket Client

순수 C 언어로 작성된 OBS WebSocket 클라이언트 라이브러리

## 개요

OBS Studio와 WebSocket 프로토콜(obs-websocket v5.x)을 통해 통신하기 위한 경량 라이브러리입니다.
외부 라이브러리 의존성 없이 순수 C로 구현되어 임베디드 환경에서도 사용 가능합니다.

## 프로토콜 정보

| 항목 | 값 |
|------|-----|
| **통신 방식** | WebSocket (RFC 6455) |
| **포트** | 4455 (기본값, 변경 가능) |
| **프로토콜** | obs-websocket 5.x |
| **메시지 포맷** | JSON |
| **인증** | SHA256 기반 |
| **최신 지원** | ✅ 2025년 현재 최신 방식 (rpcVersion 1) |

### 프로토콜 버전

| obs-websocket 버전 | OBS Studio 버전 | 상태 |
|--------------------|-----------------|------|
| 5.5.x (현재) | OBS 30.x - 31.x | ✅ 지원 |
| 5.0.x - 5.4.x | OBS 28.x - 29.x | ✅ 호환 |
| 4.9.x (레거시) | OBS 27.x 이하 | ❌ 미지원 |

> **Note**: obs-websocket 5.x는 OBS Studio 28.0부터 기본 내장되어 있습니다.
> 4.x 프로토콜과는 호환되지 않습니다.

### 프로토콜 특징

obs-websocket 5.x는 OBS Project의 공식 원격 제어 프로토콜입니다:

- **공식 프로토콜**: OBS Studio에 기본 내장 (v28+)
- **WebSocket 기반**: RFC 6455 표준 WebSocket
- **JSON 메시지**: 텍스트 기반 JSON 포맷
- **OpCode 기반**: Hello(0), Identify(1), Event(5), Request(6) 등
- **이벤트 구독**: 실시간 Scene/Studio Mode 변경 알림

### 참고 자료

- [obs-websocket GitHub](https://github.com/obsproject/obs-websocket)
- [Protocol Documentation](https://github.com/obsproject/obs-websocket/blob/master/docs/generated/protocol.md)
- [OBS WebSocket Releases](https://github.com/obsproject/obs-websocket/releases)

## 특징

- 순수 C99 표준 (외부 의존성 없음)
- obs-websocket v5.x 프로토콜 지원 (OBS v28+)
- SHA256 기반 인증 지원
- 실시간 상태 조회 (Program, Preview, Tally)
- 제어 명령 전송 (Scene 변경, Cut, Auto)
- 플랫폼 추상화 레이어 (Linux, ESP-IDF 지원)
- **ESP32 최적화**: 메모리 ~5.7KB (PSRAM 불필요)

## 메모리 사용량

| 구조체 | 크기 | 설명 |
|--------|------|------|
| `obs_client_t` | 5,688 bytes | 전체 클라이언트 |
| `ws_client_t` | 3,824 bytes | WebSocket 레이어 |

### 버퍼 크기 (ESP32 최적화)

| 버퍼 | 크기 | 용도 |
|------|------|------|
| `WS_RECV_BUFFER` | 2KB | WebSocket 수신 |
| `WS_FRAME_BUFFER` | 1KB | 분할 메시지 조립 |
| `WS_SEND_BUFFER` | 512B | WebSocket 전송 |

### 제한값

| 항목 | 값 | 설명 |
|------|-----|------|
| `OBS_MAX_SCENES` | 16 | 최대 Scene 수 |
| `OBS_SCENE_NAME_MAX` | 64 | Scene 이름 최대 길이 |

## 빠른 시작

```c
#include "obs_client.h"

obs_client_t client;

// 초기화 및 연결
obs_client_init(&client, "192.168.0.3", OBS_DEFAULT_PORT, "password");
obs_client_connect(&client, OBS_CONNECT_TIMEOUT_MS);

// 메인 루프
while (obs_client_is_connected(&client)) {
    obs_client_loop(&client);  // 필수!

    // Tally 조회
    uint8_t tally = obs_client_get_tally_by_index(&client, 0);

    // 제어
    obs_client_set_program_scene(&client, 1);

    sw_platform_delay(10);
}

obs_client_cleanup(&client);
```

## 빌드

```bash
# 라이브러리 빌드
make

# 테스트 프로그램 빌드
make test          # 연결 테스트
make test-monitor  # 실시간 모니터링
make test-control  # 제어 명령 테스트

# 디버그 빌드 (상세 로그)
OBS_DEBUG_LEVEL=2 make

# 정리
make clean
```

## 테스트 프로그램

```bash
# 연결 테스트
./build/test_connect 192.168.0.3 4455 password

# 실시간 모니터링 (Tally 변경 감시)
./build/test_monitor 192.168.0.3 4455 password

# 제어 테스트 (Cut, Auto, Scene 변경)
./build/test_control 192.168.0.3 4455 password
```

## 파일 구조

```
obs/
├── include/              # 공개 헤더
│   ├── obs_client.h      # 클라이언트 API
│   ├── obs_protocol.h    # 프로토콜 상수
│   └── obs_state.h       # 상태 구조체
├── src/                  # 소스 파일
│   ├── obs_client.c      # 클라이언트 구현
│   ├── obs_websocket.c   # WebSocket 클라이언트 (RFC 6455)
│   ├── obs_websocket.h
│   ├── obs_json.c        # JSON 파서/빌더
│   ├── obs_json.h
│   ├── obs_sha256.c      # SHA256 (인증용)
│   ├── obs_sha256.h
│   ├── obs_base64.c      # Base64 인코딩
│   └── obs_base64.h
├── tests/                # 테스트 프로그램
│   ├── test_connect.c    # 연결 테스트
│   ├── test_monitor.c    # 실시간 모니터링
│   └── test_control.c    # 제어 테스트
├── Makefile
└── README.md
```

## API 요약

### 연결 관리

| 함수 | 설명 |
|------|------|
| `obs_client_init()` | 클라이언트 초기화 |
| `obs_client_connect()` | OBS에 연결 (인증 포함) |
| `obs_client_loop()` | 메시지 수신/처리 (필수) |
| `obs_client_disconnect()` | 연결 종료 |
| `obs_client_cleanup()` | 리소스 해제 |
| `obs_client_is_connected()` | 연결 상태 확인 |

### 상태 조회

| 함수 | 설명 |
|------|------|
| `obs_client_get_program_scene()` | Program Scene 인덱스 (0-based) |
| `obs_client_get_preview_scene()` | Preview Scene 인덱스 (Studio Mode) |
| `obs_client_get_scene_count()` | Scene 개수 |
| `obs_client_get_scene_name()` | Scene 이름 |
| `obs_client_get_tally_by_index()` | Tally 상태 (Scene별) |
| `obs_client_get_tally_packed()` | Tally 전체 (uint64_t) |
| `obs_client_is_studio_mode()` | Studio Mode 상태 |

### 제어 명령

| 함수 | 설명 |
|------|------|
| `obs_client_set_program_scene()` | Program Scene 변경 (인덱스) |
| `obs_client_set_program_scene_by_name()` | Program Scene 변경 (이름) |
| `obs_client_set_preview_scene()` | Preview Scene 변경 (Studio Mode) |
| `obs_client_cut()` | Cut 실행 (Preview → Program) |
| `obs_client_auto()` | Auto 트랜지션 (Studio Mode) |
| `obs_client_set_studio_mode()` | Studio Mode On/Off |

### 콜백 설정

| 함수 | 설명 |
|------|------|
| `obs_client_set_on_connected()` | 연결 완료 콜백 |
| `obs_client_set_on_disconnected()` | 연결 해제 콜백 |
| `obs_client_set_on_scene_changed()` | Scene 변경 콜백 |

## Tally 시스템

OBS는 Scene 기반 Tally를 제공합니다:

```c
// Scene 인덱스로 Tally 조회
uint8_t tally = obs_client_get_tally_by_index(&client, scene_index);
// OBS_TALLY_OFF (0), OBS_TALLY_PROGRAM (1), OBS_TALLY_PREVIEW (2)

// Packed Tally (32채널 × 2비트 = 64비트)
uint64_t packed = obs_client_get_tally_packed(&client);
```

Tally Packed 포맷 (ATEM과 동일):
- 채널당 2비트
- bit 0: Program
- bit 1: Preview
- 채널 N의 위치: `(packed >> (N * 2)) & 0x03`

## 디버그

```bash
# 디버그 레벨 설정
OBS_DEBUG_LEVEL=0 make  # 없음 (기본값)
OBS_DEBUG_LEVEL=1 make  # 에러만
OBS_DEBUG_LEVEL=2 make  # 정보 + 에러
OBS_DEBUG_LEVEL=3 make  # 상세 로그 (JSON 포함)
```

## ATEM과의 비교

| 기능 | ATEM | OBS |
|------|------|-----|
| 프로토콜 | UDP | WebSocket |
| 인증 | 없음 | SHA256 |
| Program/Preview | ME별 지원 | 단일 출력 |
| Tally | 소스 기반 | Scene 기반 |
| 토폴로지 | 복잡 (ME, DSK, USK) | 단순 (Scene만) |
| 입력 수 | 모델별 고정 | 무제한 (Scene) |

## 테스트 결과 (OBS v32.0.2)

```
OBS WebSocket: 5.6.3
Scene 개수: 7
Studio Mode: Yes

테스트: 8/8 통과
```

## 라이선스

MIT License
