# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 필수 확인 사항
> **중요: 작업 시작 전 반드시 `claude_project.md` 파일을 확인하세요.**
> - 프로젝트 현재 상태
> - 진행 중인 작업
> - 문서 구조
> - 개발 히스토리

## 언어 설정
- 모든 응답은 **한글**로 작성
- 코드 주석도 **한글**로 작성
- 에러 메시지나 설명도 **한글**로 번역
- 변수명과 함수명은 **영어** 유지

## 프로젝트 개요
- **플랫폼:** ESP32-S3 (EoRa-S3 보드)
- **프레임워크:** ESP-IDF 5.5.0 (PlatformIO)
- **언어:** C

## 개발 방법론: TDD (Test-Driven Development)
1. **Red:** 실패하는 테스트 먼저 작성
2. **Green:** 테스트를 통과하는 최소한의 코드 작성
3. **Refactor:** 코드 리팩토링

## 개발 워크플로우

### 1. 작업 시작
```
1. claude_project.md 확인
2. 현재 상태 및 진행 중인 작업 파악
3. TODO 리스트 업데이트
```

### 2. 코드 개발 (TDD)
```
1. 테스트 코드 작성 (test/ 디렉토리)
2. 실패 확인
3. 구현 코드 작성 (src/ 디렉토리)
4. 테스트 통과 확인
5. 리팩토링
```

### 3. 작업 완료
```
1. 코드 빌드 및 테스트 확인
2. Git 커밋 (의미 있는 커밋 메시지)
3. claude_project.md 업데이트
   - History에 작업 내용 추가
   - 문서 생성 시 Documents 섹션 업데이트
```

## Git 규칙

### 커밋 메시지 형식
```
<type>: <subject>

<body>
```

### 커밋 타입
- `feat`: 새로운 기능
- `fix`: 버그 수정
- `refactor`: 코드 리팩토링
- `test`: 테스트 추가/수정
- `docs`: 문서 작성/수정
- `chore`: 빌드, 설정 등 기타

### 예시
```
feat: LoRa 송수신 기능 구현

- SX1262 초기화 함수 추가
- 패킷 송신/수신 함수 구현
- 주파수 설정 기능 추가
```

## 프로젝트 구조
```
/home/dev/esp-idf/
├── CLAUDE.md              # AI 어시스턴트 지침 (이 파일)
├── claude_project.md      # 프로젝트 작업 관리 문서
├── platformio.ini         # PlatformIO 설정
├── boards/                # 보드 설정
│   └── EoRa_S3.json
├── src/                   # 소스 코드
│   └── main.c
├── include/               # 헤더 파일
├── lib/                   # 프로젝트 라이브러리
├── test/                  # 테스트 코드
├── docs/                  # 문서
└── venv/                  # Python 가상환경
```

## 빌드 및 업로드 명령어
```bash
# 가상환경 활성화
source venv/bin/activate

# 빌드
pio run

# 업로드
pio run -t upload

# 클린 빌드
pio run -t clean

# 테스트 실행
pio test

# 특정 테스트만 실행
pio test -f <test_name>
```

## 업로드 (포트 점유 해제 포함)
> 포트가 다른 프로세스에 점유되어 업로드 실패 시 사용
```bash
fuser -k /dev/ttyACM0 2>/dev/null; sleep 1 && source venv/bin/activate && pio run -t upload
```

## 시리얼 모니터 (Claude Code 환경용)
> TTY가 없는 환경에서 Python pyserial 사용

### 리셋 + 부팅 로그 캡처 (5초)
```bash
source venv/bin/activate && python3 -c "
import serial,time
s=serial.Serial('/dev/ttyACM0',921600,timeout=0.1)
s.dtr=False;s.rts=True;time.sleep(0.1);s.rts=False
t=time.time()
while time.time()-t<5:
 d=s.read(500)
 if d:print(d.decode(errors='ignore'),end='')
s.close()"
```

### 실시간 모니터링 (10초)
```bash
source venv/bin/activate && python3 -c "
import serial,time
s=serial.Serial('/dev/ttyACM0',921600,timeout=0.1)
t=time.time()
while time.time()-t<10:
 d=s.read(500)
 if d:print(d.decode(errors='ignore'),end='')
s.close()"
```

### 터미널에서 직접 사용 시

#### screen 사용 (재부팅 없이 모니터링, 권장)
```bash
screen /dev/ttyACM0 921600
```
- 종료: `Ctrl+A` → `K` → `y`
- 스크롤: `Ctrl+A` → `ESC` (화살표로 이동, ESC로 종료)

#### PlatformIO 모니터
```bash
source venv/bin/activate && pio device monitor
```
> 주의: ESP32-S3 연결 시 재부팅될 수 있음

## 문서 작성 규칙
1. 새 문서 생성 시 **반드시** `claude_project.md`의 Documents 섹션에 등록
2. 문서에는 **위치**와 **역할** 명시
3. 문서 형식: Markdown (.md)

## 코딩 스타일
- 들여쓰기: 4 spaces
- 함수명: snake_case
- 상수: UPPER_SNAKE_CASE
- 구조체: PascalCase
- 주석: 한글로 작성

## 로거 시스템

### Simple Log (유일한 로거)
- **위치**: `/components/simple_log`
- **특징**: 경량 로거, 2단계 레벨 (0, 1)
- **사용**: 모든 컴포넌트에서 사용

```c
#include "log.h"
#include "log_tags.h"

// 레벨 0: 항상 출력
LOG_0(TAG_BUTTON, "버튼 눌림");

// 레벨 1: 디버그 정보
LOG_1(TAG_NETWORK, "상태: %d", status);

// 로그 레벨 설정
log_set_level(LOG_LEVEL_1);
```

### ESP-IDF Log (참고용)
- **사용 목적**: 빌드 에러 방지용으로만 포함
- **설정**: `sdkconfig.defaults`에서 완전 비활성화됨
- **실제 출력**: 없음 (LOG_LEVEL=0)
- **참고**: 일부 저수준 라이브러리에서 include만 함

### 로그 태그
- **위치**: `/components/simple_log/include/log_tags.h`
- **규칙**: 컴포넌트별 태그 중앙 관리 (최대 10자)
- **예시**: `TAG_MAIN`, `TAG_BUTTON`, `TAG_DISPLAY`

### 사용 가이드
1. 새 컴포넌트: `log_tags.h`에 태그 추가
2. 기본 정보: `LOG_0` 사용
3. 디버그: `LOG_1` 사용
4. 태그는 `log_tags.h`의 정의된 값 사용
5. ESP_LOG* 함수는 사용하지 말 것 (출력되지 않음)
