# vmix

vMix TCP API 클라이언트 라이브러리

## 개요

vMix 스위처와 TCP 통신하기 위한 경량 라이브러리입니다.
Linux 개발 환경에서 테스트 가능하며, ESP-IDF (ESP32) 플랫폼으로 포팅 가능하도록 설계되었습니다.

## 특징

- 순수 C99 표준
- 플랫폼 추상화 레이어 (Linux, ESP-IDF 지원 예정)
- 실시간 Tally 상태 조회 (SUBSCRIBE TALLY)
- 제어 명령 전송 (Cut, Fade, PreviewInput, ActiveInput 등)
- ATEM 라이브러리와 동일한 API 패턴

## 프로토콜 정보

| 항목 | 값 |
|------|-----|
| **통신 방식** | TCP |
| **포트** | 8099 (고정) |
| **프로토콜** | vMix TCP API |
| **인코딩** | UTF-8 |
| **종료 문자** | `\r\n` |
| **최신 지원** | ✅ 2025년 현재 최신 방식 (vMix 29 호환) |

### 프로토콜 특징

vMix TCP API는 StudioCoast의 공식 API입니다:

- **공식 프로토콜**: vMix에서 공식 문서 제공
- **텍스트 기반**: ASCII/UTF-8 텍스트 명령어 사용
- **SMTP 유사**: 간단한 명령/응답 구조
- **확장 가능**: 하위 호환성 유지하며 기능 추가
- **SUBSCRIBE**: 실시간 Tally 업데이트 구독 지원

### API 버전

| vMix 버전 | TCP API 문서 |
|-----------|--------------|
| v29 (최신) | [help29/TCPAPI](https://www.vmix.com/help29/TCPAPI.html) |
| v28 | [help28/TCPAPI](https://www.vmix.com/help28/TCPAPI.html) |
| v27 | [help27/TCPAPI](https://www.vmix.com/help27/TCPAPI.html) |
| v26 | [help26/TCPAPI](https://www.vmix.com/help26/TCPAPI.html) |
| v25 | [help25/TCPAPI](https://www.vmix.com/help25/TCPAPI.html) |

### 참고 자료

- [vMix 공식 TCP API 문서](https://www.vmix.com/help29/TCPAPI.html)
- [vmixAPI.com](https://vmixapi.com/) - 비공식 API 레퍼런스

## vMix TCP API 상세

### 응답 형식

```
<command> <status> [response]\r\n
```

| 상태 | 설명 |
|------|------|
| OK | 성공 |
| ER | 오류 |
| 숫자 | 바이너리 데이터 길이 |

### Tally 상태 값

| 값 | 설명 |
|----|------|
| 0 | Off |
| 1 | Program (On Air) |
| 2 | Preview |

### 주요 명령어

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `TALLY` | Tally 상태 조회 | `TALLY` → `TALLY OK 01200...` |
| `SUBSCRIBE TALLY` | Tally 자동 수신 | 연결 시 전송 |
| `FUNCTION` | 기능 실행 | `FUNCTION Cut` |
| `XML` | XML 상태 조회 | 바이너리 응답 |
| `XMLTEXT` | XPath 값 조회 | `XMLTEXT vmix/preview` |

### FUNCTION 명령어

| 기능 | 명령어 |
|------|--------|
| Cut | `FUNCTION Cut` |
| Fade | `FUNCTION Fade` |
| Preview 변경 | `FUNCTION PreviewInput Input=2` |
| Program 변경 | `FUNCTION ActiveInput Input=2` |
| 오버레이 On | `FUNCTION OverlayInput1In Input=5` |
| 오버레이 Off | `FUNCTION OverlayInput1Out` |

## 빠른 시작

```c
#include "vmix_client.h"

vmix_client_t client;

// 초기화 및 연결
vmix_client_init(&client, "192.168.0.100", VMIX_DEFAULT_PORT);
vmix_client_connect(&client, VMIX_CONNECT_TIMEOUT_MS);

// 메인 루프
while (vmix_client_is_connected(&client)) {
    vmix_client_loop(&client);  // 필수!

    // Tally 조회
    uint8_t tally = vmix_client_get_tally_by_index(&client, 0);

    // 제어
    vmix_client_cut(&client);

    // 10ms 대기
    sw_platform_delay(10);
}

vmix_client_cleanup(&client);
```

## 빌드

```bash
# 라이브러리 빌드
make

# 테스트 프로그램 빌드
make test

# 디버그 빌드
make debug

# 정리
make clean
```

## 파일 구조

```
vmix/
├── include/
│   ├── vmix_client.h     # 클라이언트 API
│   ├── vmix_state.h      # 상태 구조체
│   └── vmix_protocol.h   # 프로토콜 상수
├── src/
│   └── vmix_client.c     # 클라이언트 구현
├── tests/
│   ├── test_connect.c    # 연결 테스트
│   └── test_monitor.c    # 실시간 모니터링
├── docs/
├── Makefile
└── README.md
```

## API 요약

### 연결 관리

| 함수 | 설명 |
|------|------|
| `vmix_client_init()` | 클라이언트 초기화 |
| `vmix_client_connect()` | 연결 시작 |
| `vmix_client_disconnect()` | 연결 종료 |
| `vmix_client_loop()` | 패킷 수신/처리 (필수) |
| `vmix_client_cleanup()` | 리소스 해제 |

### 상태 조회

| 함수 | 설명 |
|------|------|
| `vmix_client_get_tally_by_index()` | Tally 상태 (채널별) |
| `vmix_client_get_tally_packed()` | Tally 전체 (uint64_t) |
| `vmix_client_get_program_input()` | Program 입력 |
| `vmix_client_get_preview_input()` | Preview 입력 |

### 제어 명령

| 함수 | 설명 |
|------|------|
| `vmix_client_cut()` | Cut 실행 |
| `vmix_client_fade()` | Fade 트랜지션 |
| `vmix_client_set_preview_input()` | Preview 변경 |
| `vmix_client_set_program_input()` | Program 변경 |

## ATEM과의 차이점

| 항목 | ATEM | vMix |
|------|------|------|
| 프로토콜 | UDP | TCP |
| 포트 | 9910 | 8099 |
| 초기화 | 핸드셰이크 필요 | SUBSCRIBE 전송 |
| Tally 형식 | 바이너리 | ASCII 문자열 |
| ME 개념 | 있음 | 없음 |

## 참고 자료

- [vMix TCP API 문서](https://www.vmix.com/help25/TCPAPI.html)

## 라이선스

MIT License
