# atemudp

순수 C 언어로 작성된 ATEM UDP 클라이언트 라이브러리

## 개요

Blackmagic Design ATEM 스위처와 UDP 통신하기 위한 경량 라이브러리입니다.
Linux 개발 환경에서 테스트 가능하며, ESP-IDF (ESP32) 플랫폼으로 포팅 가능하도록 설계되었습니다.

## 프로토콜 정보

| 항목 | 값 |
|------|-----|
| **통신 방식** | UDP |
| **포트** | 9910 (고정) |
| **프로토콜** | Blackmagic 독자 프로토콜 (비공식) |
| **최신 지원** | ✅ 2025년 현재 최신 방식 |

### 프로토콜 특징

ATEM은 Blackmagic Design의 독자적인 UDP 기반 프로토콜을 사용합니다:

- **비공식 프로토콜**: Blackmagic은 공식 프로토콜 문서를 제공하지 않음
- **리버스 엔지니어링**: [OpenSwitcher](https://docs.openswitcher.org/), [SKAARHOJ](https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering) 등의 프로젝트에서 분석
- **TCP-over-UDP**: 시퀀스 번호, 재전송, ACK를 구현한 신뢰성 있는 UDP
- **3-Way Handshake**: TCP와 유사한 연결 수립 과정 (SYN → SYN-ACK → ACK)
- **12바이트 헤더**: 모든 패킷에 고정 헤더 포함
- **Keepalive**: 주기적인 ACK 패킷으로 연결 유지

### 참고 자료

- [OpenSwitcher Documentation](https://docs.openswitcher.org/udptransport.html) - 최신 프로토콜 문서
- [node-atem Specification](https://github.com/miyukki/node-atem/blob/master/specification.md)

## 특징

- 순수 C99 표준
- 플랫폼 추상화 레이어 (Linux, ESP-IDF 지원 예정)
- 실시간 상태 조회 (Program, Preview, Tally, USK, DSK)
- 제어 명령 전송 (Cut, Auto, 입력 변경, DSK, USK)

## 빠른 시작

```c
#include "atem_client.h"

atem_client_t client;

// 초기화 및 연결
atem_client_init(&client, "192.168.0.240", ATEM_DEFAULT_PORT);
atem_client_connect(&client, ATEM_CONNECT_TIMEOUT_MS);
atem_client_wait_init(&client, ATEM_INIT_TIMEOUT_MS);

// 메인 루프
while (atem_client_is_connected(&client)) {
    atem_client_loop(&client);  // 필수!

    // Tally 조회
    uint8_t tally = atem_client_get_tally_by_index(&client, 0);

    // 제어
    atem_client_cut(&client, 0);

    atem_platform_delay(10);
}

atem_client_cleanup(&client);
```

## 문서

| 문서 | 설명 |
|------|------|
| [CONNECTION.md](docs/CONNECTION.md) | 연결 설정 및 관리 |
| [TALLY.md](docs/TALLY.md) | 실시간 Tally 정보 조회 |
| [CONTROL.md](docs/CONTROL.md) | 스위처 제어 (Cut, Auto, DSK, USK) |

## 빌드

```bash
# 라이브러리 빌드
make

# 테스트 프로그램 빌드
make test          # 연결 테스트
make test-control  # 제어 명령 테스트
make test-topology # 토폴로지 확인
make test-monitor  # 실시간 모니터링

# 디버그 빌드 (상세 로그 + 패킷 덤프)
make debug

# 정리
make clean
```

## 테스트 프로그램

```bash
# 연결 테스트
./build/test_connect 192.168.0.240

# 토폴로지 확인
./build/test_topology 192.168.0.240

# 제어 테스트
./build/test_control 192.168.0.240

# 실시간 모니터링 (Tally packed 값 확인)
./build/test_monitor 192.168.0.240
```

## 파일 구조

```
atemudp/
├── include/              # 헤더 파일
│   ├── atem_client.h     # 클라이언트 API
│   ├── atem_state.h      # 상태 구조체
│   ├── atem_protocol.h   # 프로토콜 상수
│   ├── atem_parser.h     # 명령 파서
│   ├── atem_debug.h      # 디버그 시스템
│   ├── atem_platform.h   # 플랫폼 추상화
│   └── atem_buffer.h     # 버퍼 유틸리티
├── src/                  # 소스 파일
│   ├── atem_client.c     # 클라이언트 구현
│   ├── atem_parser.c     # 파서 구현
│   └── atem_debug.c      # 디버그 구현
├── platform/             # 플랫폼별 구현
│   └── atem_platform_linux.c
├── tests/                # 테스트 프로그램
│   ├── test_connect.c    # 연결 테스트
│   ├── test_topology.c   # 토폴로지 확인
│   ├── test_control.c    # 제어 테스트
│   └── test_monitor.c    # 실시간 모니터링
├── docs/                 # 상세 문서
│   ├── CONNECTION.md     # 연결 사용법
│   ├── TALLY.md          # Tally 사용법
│   └── CONTROL.md        # 제어 사용법
├── Makefile
└── README.md
```

## API 요약

### 연결 관리

| 함수 | 설명 |
|------|------|
| `atem_client_init()` | 클라이언트 초기화 |
| `atem_client_connect()` | 연결 시작 |
| `atem_client_wait_init()` | 초기화 완료 대기 |
| `atem_client_loop()` | 패킷 수신/처리 (필수) |
| `atem_client_cleanup()` | 리소스 해제 |

### 상태 조회

| 함수 | 설명 |
|------|------|
| `atem_client_get_tally_by_index()` | Tally 상태 (채널별) |
| `atem_client_get_tally_packed()` | Tally 전체 (uint64_t) |
| `atem_client_get_program_input()` | Program 소스 |
| `atem_client_get_preview_input()` | Preview 소스 |
| `atem_client_is_in_transition()` | 트랜지션 중 여부 |

### 제어 명령

| 함수 | 설명 |
|------|------|
| `atem_client_cut()` | Cut 실행 |
| `atem_client_auto()` | Auto 트랜지션 |
| `atem_client_set_preview_input()` | Preview 변경 |
| `atem_client_set_dsk_on_air()` | DSK On/Off |
| `atem_client_set_keyer_on_air()` | USK On/Off |

## 디버그

```bash
# 디버그 빌드
make debug

# 커스텀 레벨
make DEBUG_LEVEL=2                # 경고까지만
make DEBUG_LEVEL=3 DEBUG_PACKET=1 # 정보 + 패킷덤프
```

디버그 레벨:
- `0`: 없음 (기본값)
- `1`: 에러만
- `2`: 경고 + 에러
- `3`: 정보 + 경고 + 에러
- `4`: 모든 로그

## 테스트 결과 (ATEM Mini)

```
제품명: ATEM Mini
ME 수: 1
소스 수: 14
카메라 수: 4
DSK 수: 1
USK 수: 1

테스트: 12/12 통과
```

## 라이선스

MIT License
