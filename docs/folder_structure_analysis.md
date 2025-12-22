# 폴더 구조 분석 및 개선 제안

## 현재 구조 분석

### ✅ 잘 구성된 부분

#### 1. Core Service Layer
```
components/
├── info/                   # InfoManager - 중앙 정보 관리
│   ├── include/
│   │   └── info/           # C++ 헤더 그룹화
│   └── src/                # 구현 파일
├── system/
│   ├── monitor/           # SystemMonitor - 시스템 상태 모니터링
│   └── config/            # ConfigCore - 설정 관리
```

#### 2. Domain Manager Layer
```
components/
├── display/               # DisplayManager - 디스플레이 관리
├── lora/
│   ├── manager/          # LoRaManager - LoRa 상위 관리
│   ├── communication/    # CommunicationManager - 통신 조율
│   ├── core/            # LoRaCore - 하드웨어 추상
│   └── packet/          # LoRa 패킷 정의
├── network/
│   └── manager/         # NetworkManager - 네트워크 관리
└── interface_*
    ├── web/             # WebServer - 웹 인터페이스
    └── cli/             # CLI - 명령줄 인터페이스
```

### ⚠️ 개선이 필요한 부분

#### 1. Switcher 컴포넌트
현재 구조:
```
components/switcher/
├── common/              # 플랫폼 공통 코드
├── handler/             # 프로토콜 핸들러
├── manager/             # 스위처 매니저
└── protocol/            # 각 프로토콜 구현
    ├── atem/
    ├── obs/
    └── vmix/
```

문제점:
- `handler`와 `protocol`의 경계가 모호함
- `common`이 너무 광범위한 이름

개선 제안:
```
components/switcher/
├── manager/             # SwitcherManager (유지)
├── core/               # 공합 인터페이스
├── drivers/            # 프로토콜 드라이버
│   ├── atem/
│   ├── obs/
│   └── vmix/
└── platform/           # 플랫폼 종속 코드
    ├── esp32/
    └── common/
```

#### 2. System 컴포넌트
현재 구조:
```
components/system/
├── button_poll/        # 버튼 폴링
├── common/            # 시스템 공통 코드
├── config/            # 설정 관리
└── monitor/           # 시스템 모니터링
```

개선 제안:
```
components/system/
├── config/            # ConfigCore (유지)
├── monitor/           # SystemMonitor (유지)
├── input/             # 입력 장치 관리
│   └── button/        # 버튼 관리
└── utils/             # 시스템 유틸리티
```

#### 3. 테스트 구조
현재 상황:
- `info/test`만 존재
- 대부분 컴포넌트에 테스트 폴더 부족

권장 구조:
```
components/[component]/
├── include/
├── src/
└── test/               # 각 컴포넌트별 테스트
    ├── unity/
    └── mocks/
```

## 권장 최종 구조

```
components/
├── core/                           # Core Service Layer
│   ├── info/                       # InfoManager
│   ├── system/                     # 시스템 서비스
│   │   ├── monitor/               # SystemMonitor
│   │   ├── config/                # ConfigCore
│   │   └── utils/                 # 공통 유틸리티
│   └── logging/                   # 로깅 (simple_log 이관)
│
├── managers/                       # Domain Manager Layer
│   ├── display/                   # DisplayManager
│   ├── network/                   # NetworkManager
│   ├── communication/             # CommunicationManager
│   └── switcher/                  # SwitcherManager
│
├── infrastructure/                # Infrastructure Layer
│   ├── lora/
│   │   ├── core/                  # LoRaCore
│   │   ├── manager/               # LoRaManager
│   │   └── drivers/               # LoRa 드라이버
│   ├── switcher/
│   │   ├── core/                  # 공합 인터페이스
│   │   └── drivers/               # 프로토콜 드라이버
│   │       ├── atem/
│   │       ├── obs/
│   │       └── vmix/
│   ├── display/
│   │   ├── u8g2/                  # U8g2 라이브러리
│   │   └── driver/                # 디스플레이 드라이버
│   ├── input/
│   │   └── button/                # 버튼 드라이버
│   └── led/
│       └── ws2812/                # LED 드라이버
│
└── interfaces/                    # 애플리케이션 인터페이스
    ├── web/                       # WebServer
    ├── cli/                       # CLI
    └── api/                       # REST API
```

## 이전 계획

### 1단계: switcher 컴포넌트 재구성 (1일)
- `handler` → `drivers`로 이동
- `common` → `platform`으로 리네임
- `core` 생성하여 공합 인터페이스 정의

### 2단계: system 컴포넌트 정리 (0.5일)
- `button_poll` → `input/button`으로 이동
- `common` → `utils`로 리네임 및 정리

### 3단계: 테스트 폴더 구조 확장 (0.5일)
- 주요 컴포넌트에 test 폴더 추가
- 공통 mock/유틸리티 테스트 코드 작성

## 주의사항

1. **점진적 이전**: 한 번에 모든 것을 변경하지 말고 단계적으로 진행
2. **호환성 유지**: CMakeLists.txt와 include 경로 주의
3. **테스트 검증**: 각 단계별 빌드 테스트 필수
4. **문서 갱신**: 구조 변경 시 관련 문서 업데이트

## 기대 효과

1. **명확한 경계**: 각 레이어와 컴포넌트의 역할이 명확해짐
2. **쉬운 탐색**: 폴더 이름으로 기능 유추 가능
3. **확장성**: 새로운 기능 추가 시 구조적 가이드 제공
4. **유지보수성**: 관련 코드가 한곳에 모여 관리 용이