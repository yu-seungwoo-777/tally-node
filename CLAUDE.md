# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## 필수 확인 사항

> **중요: 작업 시작 전 반드시 `claude_project.md` 파일을 확인하세요.**
> - 프로젝트 현재 상태
> - 진행 중인 작업
> - 개발 히스토리

---

## 언어 설정

- 모든 응답은 **한글**로 작성
- 코드 주석도 **한글**로 작성
- 변수명과 함수명은 **영어** 유지

---

## 프로젝트 개요

- **플랫폼:** ESP32-S3 (EoRa-S3 보드)
- **프레임워크:** ESP-IDF 5.5.0 (PlatformIO)
- **언어:** C / C++

---

## 5계층 아키텍처

```
01_app          → 앱 계층 (lora_test)
02_presentation → 프레젠테이션 계층 (예정)
03_service      → 서비스 계층 (button_service, lora_service)
04_driver       → 드라이버 계층 (button_poll, lora_driver)
05_hal          → HAL 계층 (lora_hal)
00_common       → 공통 (event_bus)
```

> **상세:** 의존성 규칙, event_bus 역할은 `docs/ARCHITECTURE.md` 참고

---

## 프로젝트 구조

```
/home/prod/tally-node/
├── CLAUDE.md              # AI 어시스턴트 지침 (이 파일)
├── claude_project.md      # 프로젝트 작업 관리 문서
├── platformio.ini         # PlatformIO 설정
├── CMakeLists.txt         # 최상위 CMake (컴포넌트 등록)
├── src/                   # 메인 소스
│   └── main.cpp
├── components/            # 컴포넌트 (5계층)
│   ├── 00_common/
│   ├── 01_app/
│   ├── 02_presentation/
│   ├── 03_service/
│   ├── 04_driver/
│   └── 05_hal/
├── docs/                  # 문서
│   └── ARCHITECTURE.md    # 아키텍처 문서
├── examples/              # 예제 코드
└── venv/                  # Python 가상환경
```

---

## 컴포넌트 구성 규칙

### 파일 배치

```
component_name/
├── file.c              # 소스 파일 (루트에 배치)
├── include/
│   └── file.h          # 헤더 파일
└── CMakeLists.txt
```

**중요:** `src/` 폴더를 사용하지 않음

### CMakeLists.txt 패턴

```cmake
idf_component_register(
    SRCS "file.c"                 # 루트에 있는 소스
    INCLUDE_DIRS "include"        # 헤더 폴더
    REQUIRES other_component      # 의존 컴포넌트
)
```

---

## 빌드 명령어

```bash
# 가상환경 활성화
source venv/bin/activate

# 빌드
pio run

# 업로드
pio run -t upload

# 클린 빌드
pio run -t clean && pio run
```

---

## 코딩 스타일

- 들여쓰기: 4 spaces
- 함수명: `snake_case`
- 상수: `UPPER_SNAKE_CASE`
- 구조체: `PascalCase`
- 주석: 한글

---

## 로그 시스템

**ESP-IDF Log 사용** (`esp_log.h`)

```c
#include "esp_log.h"

static const char* TAG = "COMPONENT_NAME";

ESP_LOGI(TAG, "정보 메시지");
ESP_LOGW(TAG, "경고 메시지");
ESP_LOGE(TAG, "에러 메시지");
```

---

## Git 규칙

### 커밋 메시지 형식

```
<type>: <subject>
```

### 커밋 타입

- `feat`: 새로운 기능
- `fix`: 버그 수정
- `refactor`: 코드 리팩토링
- `docs`: 문서 작성/수정
- `chore`: 빌드, 설정 등 기타

### 예시

```
feat: button_service 컴포넌트 추가

- 버튼 폴링 서비스 레이어 구현
- 단일 클릭/롱 프레스 이벤트 지원
```

---

## 개발 워크플로우

1. **빌드 테스트**: `pio run`
2. **Git 커밋**: 의미 있는 커밋 메시지
3. **문서 반영**: `docs/ARCHITECTURE.md` 또는 `claude_project.md` 업데이트

---

## 문서

| 문서 | 위치 | 용도 |
|------|------|------|
| ARCHITECTURE.md | docs/ | 컴포넌트 구조, 의존성 |
| claude_project.md | 루트 | 프로젝트 관리, 히스토리 |

---

## 레이어 등록 방법 (CMakeLists.txt)

### 루트 CMakeLists.txt

`/home/prod/tally-node/CMakeLists.txt`에서 레이어를 등록합니다:

```cmake
# 1. 레이어 폴더 등록
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/components/00_common"
    "${CMAKE_SOURCE_DIR}/components/01_app"
    "${CMAKE_SOURCE_DIR}/components/02_presentation"
    "${CMAKE_SOURCE_DIR}/components/03_service"
    "${CMAKE_SOURCE_DIR}/components/04_driver"
    "${CMAKE_SOURCE_DIR}/components/05_hal"
)

# 2. 하위 컴포넌트 자동 등록 (CMakeLists.txt가 있는 하위 폴더)
register_subcomponents("${CMAKE_SOURCE_DIR}/components/01_app" app_subcomps)
# ...

# 3. EXTRA_COMPONENT_DIRS에 추가
list(APPEND EXTRA_COMPONENT_DIRS ${app_subcomps} ...)
```

### 새 컴포넌트 추가 방법

```
components/03_service/my_component/
├── my_component.c      # 소스 (루트에 배치)
├── include/
│   └── my_component.h  # 헤더
└── CMakeLists.txt
```

```cmake
# CMakeLists.txt
idf_component_register(
    SRCS "my_component.c"
    INCLUDE_DIRS "include"
    REQUIRES other_component
)
```

**중요:** `src/` 폴더를 사용하지 않고, 소스 파일을 컴포넌트 루트에 배치합니다.
