# 구현 계획 (Implementation Plan)

## 개요 (Overview)

본 SPEC은 TX 모드 첫 페이지를 Tally 정보 + 연결 상태 하이브리드 페이지로 리팩토링합니다. 사용자가 전원 켜기 즉시 Tally 상태와 모든 연결 상태를 한눈에 파악할 수 있도록 설계되었습니다. IP 주소 표시를 제거하고, "STATUS DASHBOARD" 제목을 제거하여 공간을 확보합니다.

## 마일스톤 (Milestones)

### 1단계: 기존 코드 분석 및 이해 (Priority: High)

**목표**: 기존 TxPage 구조와 데이터 업데이트 메커니즘 이해

**작업**:
- 기존 TxPage.cpp의 draw_tally_page() 함수 분석
- network_service, switcher_service, tally_service, battery_driver 인터페이스 확인
- 이벤트 기반 업데이트 메커니즘 파악
- U8g2 좌표 시스템 및 폰트 렌더링 이해

**완료 기준**:
- 기존 페이지 렌더링 흐름 문서화
- 데이터 소스 및 업데이트 경로 매핑 완료

### 2단계: 아이콘 구현 (Priority: High)

**목표**: 체크마크(✓)와 엑스마크(✗) 아이콘 구현

**작업**:
- icons.c에 drawCheckMark() 함수 추가
- icons.c에 drawCrossMark() 함수 추가
- icons.h에 함수 선언 추가
- 8x8 픽셀 비트맵 정의

**완료 기준**:
- 아이콘이 128x64 디스플레이에서 올바르게 렌더링됨
- 기존 배터리/신호 아이콘과 스타일 일관성 유지

### 3단계: 하이브리드 데이터 구조 구현 (Priority: High)

**목표**: hybrid_dashboard_data_t 구조체 및 내부 상태 변수 정의

**작업**:
- TxPage.cpp에 hybrid_dashboard_data_t 구조체 정의
- 내부 정적 변수 선언 (s_hybrid_dashboard_data)
- 초기값 설정

**완료 기준**:
- 구조체가 컴파일되고 메모리에 할당됨
- 모든 필드가 기본값으로 초기화됨

### 4단계: 상태 업데이트 API 구현 (Priority: High)

**목표**: 외부 서비스에서 상태 업데이트를 위한 공개 API 구현

**작업**:
- tx_page_set_tally_status() 함수 구현 (PGM/PVW 채널)
- tx_page_set_ap_status() 함수 구현 (AP on/off 상태만)
- tx_page_set_wifi_sta_status() 함수 구현 (연결 상태만, IP 제거)
- tx_page_set_ethernet_status() 함수 구현 (LINK UP/DOWN 상태만, IP 제거)
- tx_page_set_s1_status() 함수 구현 (연결 상태 + 스위처 타입)
- tx_page_set_s2_status() 함수 구현 (연결 상태 + 스위처 타입)
- tx_page_set_dual_mode() 함수 수정 (기존 함수 재사용)

**데이터 타입 정의**:
- AP 상태: ap_status_t (AP_STATUS_INACTIVE, AP_STATUS_ACTIVE)
- 네트워크 상태: network_status_t (NET_STATUS_NOT_DETECTED, NET_STATUS_DISCONNECTED, NET_STATUS_CONNECTED)
- 스위처 타입: switcher_type_t (SWITCHER_TYPE_NONE, SWITCHER_TYPE_ATEM, SWITCHER_TYPE_OBS, SWITCHER_TYPE_VMIX)

**완료 기준**:
- 모든 API 함수가 TxPage.h에 선언됨
- 외부에서 상태 업데이트 가능
- IP 주소를 받지 않도록 시그니처 수정

### 5단계: draw_hybrid_dashboard_page() 함수 구현 (Priority: High)

**목표**: 하이브리드 대시보드 페이지 렌더링 함수 구현 (4-Line Layout)

**작업**:
- 헤더 그리기 (기존 draw_tx_header() 그대로 사용, 수정 없음)
- Line 1 그리기: PGM 정보 ("PGM: 1,2,3,4" 형식)
- Line 2 그리기: PVW 정보 ("PVW: 5" 형식)
- Line 3 그리기: AP, WiFi, ETH 상태 ("AP:[ON]  WiFi:[✓]  ETH:[✓]" 형식)
- Line 4 그리기: SINGLE/DUAL + 스위처 상태
- 체크마크/엑스마크/대시 아이콘 표시
- 듀얼 모드에 따른 Line 4 표시 변경

**레이아웃 전략 (4-Line Layout)**:
- 헤더 (y=0-11): 기존 draw_tx_header() 사용 (배터리 아이콘 + 페이지 번호)
- Line 1 (y=14): "PGM: 1,2,3,4" (PGM 채널 정보만)
- Line 2 (y=25): "PVW: 5" (PVW 채널 정보만)
- Line 3 (y=36): "AP:[ON]  WiFi:[✓]  ETH:[✓]" (AP, WiFi, ETH 상태)
- Line 4 (y=47): "SINGLE  ATEM:[✓]" 또는 "DUAL  ATEM1:[✓]  ATEM2:[✗]" (모드 + 스위처 상태)

**상태 표시 규칙**:
- AP: 활성화 [ON], 비활성화 [---] (연결 체크 없음)
- WiFi: 연결됨 [✓], 연결 안 됨 [✗], 미감지 [---]
- ETH: LINK UP [✓], LINK DOWN [✗], 미감지 [---]
- 스위처: 실제 타입 표시 (ATEM, OBS, vMix)
- IP 주소 미표시 (각 설정 페이지에서만 확인)

**완료 기준**:
- 모든 정보가 128x64 화면에 맞게 렌더링됨
- 텍스트가 잘리지 않고 읽기 쉬움
- IP 주소가 표시되지 않음
- 듀얼 모드에 따라 S2 상태 표시/숨김 동작

### 6단계: page_render() 함수 수정 (Priority: High)

**목표**: Page 1 호출 시 draw_hybrid_dashboard_page()가 실행되도록 수정

**작업**:
- page_render() 함수의 case 1 분기 수정
- draw_tally_page() → draw_hybrid_dashboard_page() 호출 변경
- 기존 draw_tally_page() 함수 유지 (다른 페이지에서 재사용 가능성)

**완료 기준**:
- Page 1 진입 시 하이브리드 대시보드가 표시됨
- 페이지 전환 시 올바른 페이지가 렌더링됨

### 7단계: 헤더 수정 (Priority: High)

**목표**: draw_tx_header() 함수에 "DASHBOARD" 텍스트 추가

**작업**:
- draw_tx_header() 함수 수정
- "DASHBOARD" 텍스트 추가 (x=4, y=10)
- 페이지 번호 위치 중앙으로 이동 (x=55, y=10)
- 배터리 아이콘 위치 유지 (x=105, y=3)

**완료 기준**:
- 헤더에 "DASHBOARD" 텍스트가 명확히 표시됨
- 페이지 번호가 중앙에 위치함
- 배터리 아이콘이 우측 상단에 위치함
- 헤더 형식: "DASHBOARD          1/6 [BAT]"

### 8단계: 이벤트 핸들러 연결 (Priority: Medium)

**목표**: 서비스 상태 변경 시 자동 업데이트

**작업**:
- EVT_NETWORK_STATUS_CHANGED 이벤트 핸들러 등록
- EVT_SWITCHER_STATUS_CHANGED 이벤트 핸들러 등록
- EVT_TALLY_STATUS_CHANGED 이벤트 핸들러 등록
- EVT_BATTERY_STATUS_CHANGED 이벤트 핸들러 등록
- 각 핸들러에서 tx_page_set_*_status() 호출

**완료 기준**:
- 상태 변경 즉시 화면에 반영됨
- 수동 업데이트 없이 자동 갱신됨
- IP 주소 정보를 전달하지 않음

### 9단계: 텍스트 크기 조절 최적화 (Priority: Medium)

**목표**: 공간 최적화를 위한 텍스트 축약 로직 구현

**작업**:
- PGM/PVW 채널 리스트 길이 제한 로직
- 채널 수 제한 (최대 4개) 또는 축약형 표시 ("P:"/"V:")
- 연결 상태 라벨 축약 ("WiFi STA" → "WiFi", "Ethernet" → "ETH")
- 동적 레이아웃 조정 (텍스트 길이에 따른 위치 계산)

**완료 기준**:
- 긴 채널 리스트가 화면을 넘치지 않음
- 텍스트가 잘리지 않고 완전히 표시됨
- 가독성이 유지됨

### 10단계: 테스트 및 검증 (Priority: High)

**목표**: 모든 요구사항 충족 확인 및 버그 수정

**작업**:
- 단위 테스트: 각 상태 항목 별도 테스트
- 통합 테스트: Tally + 연결 상태 동시 표시 테스트
- 엣지 케이스: 듀얼 모드 on/off, 긴 채널 리스트, 미감지 상태 등
- 렌더링 테스트: 128x64 경계 조건 확인
- IP 주소 미표시 검증

**완료 기준**:
- 모든 acceptance criteria 통과
- 버그 없는 안정적인 동작
- IP 주소가 표시되지 않음 검증
- 사용자 경험 개선 확인

## 기술적 접근 (Technical Approach)

### 코드 구조

```
TxPage.cpp (수정)
├── 내부 상태 변수
│   ├── s_hybrid_dashboard_data (신규)
│   ├── s_current_page (기존, 유지)
│   └── 기존 s_*_data 변수들 (다른 페이지용 유지)
│
├── 내부 함수
│   ├── draw_hybrid_dashboard_page() (신규)
│   ├── draw_tx_header() (수정, "DASHBOARD" 텍스트 추가)
│   ├── draw_tally_info() (신규, 헬퍼 함수)
│   ├── draw_connection_status() (신규, 헬퍼 함수)
│   ├── abbreviate_channel_list() (신규, 헬퍼 함수)
│   └── 기존 draw_*_page() 함수들 (다른 페이지용 유지)
│
└── 공개 API
    ├── tx_page_set_tally_status() (신규)
    ├── tx_page_set_wifi_sta_status() (수정, IP 제거)
    ├── tx_page_set_wifi_ap_status() (수정, IP 제거)
    ├── tx_page_set_ethernet_status() (수정, IP 제거)
    ├── tx_page_set_s1_status() (신규)
    ├── tx_page_set_s2_status() (신규)
    └── 기존 API 함수들 (다른 페이지용 유지)
```

### 렌더링 최적화

- **문자열 자르기**: 긴 채널 리스트는 최대 길이로 제한
- **텍스트 축약**: "PGM:" → "P:", "PVW:" → "V:" (공간 부족 시)
- **채널 수 제한**: 최대 4개 채널만 표시
- **캐싱**: 변경 시에만 framebuffer 업데이트 (U8g2 sendBuffer)

### 레이아웃 계산 알고리즘

```
1. PGM/PVW 텍스트 길이 계산
2. 연결 상태 텍스트 길이 계산
3. 두 텍스트 사이 간격 조정 (최소 2 공백)
4. x 좌표 동적 할당:
   - PGM/PVW: x=4 (고정)
   - 연결 상태: x=128 - 연결_상태_길이 - 4 (우측 정렬)
```

### 메모리 관리

- 정적 할당: hybrid_dashboard_data_t는 정적 변수로 할당
- 문자열 버퍼: 고정 크기 배열 사용 (동적 할당 없음)
- 아이콘 비트맵: 프로그램 메모리(ROM)에 상수로 저장
- 채널 리스트: 고정 크기 배열 (최대 16 채널)

## 위험 및 대응 계획 (Risks and Mitigation)

### 위험 1: 화면 공간 부족

**확률**: 높음
**영향**: 높음
**대응**:
- IP 주소 제거로 충분한 공간 확보
- 텍스트 축약 로직 ("PGM" → "P", "WiFi STA" → "WiFi")
- 채널 수 제한 (최대 4개)
- 듀얼 모드 비활성화 시 S2 항목 숨김

### 위험 2: 텍스트 잘림

**확률**: 중간
**영향**: 중간
**대응**:
- 동적 레이아웃 계산으로 텍스트 길이에 따른 위치 조정
- 문자열 자르기 함수로 최대 길이 제한
- 테스트 시나리오에서 최악의 경우 검증

### 위험 3: 이벤트 핸들러 과부하

**확률**: 낮음
**영향**: 중간
**대응**:
- 이벤트 핸들러에서 최소한의 작업만 수행
- 실제 렌더링은 메인 루프에서 delegate
- 상태 변경 플래그만 설정 후 즉시 반환

### 위험 4: 기존 기능 회귀

**확률**: 낮음
**영향**: 높음
**대응**:
- 기존 draw_tally_page() 함수 삭제하지 않고 유지
- 다른 페이지(2-6)는 변경 없음 유지
- 회귀 테스트 수행

## 구현 순서 권장사항

1. **1단계 → 2단계 → 3단계**: 분석 → 아이콘 → 데이터 순서로 구현
2. **4단계 → 5단계 → 6단계**: API → 렌더링 → page_render 수정 연속 진행
3. **7단계는 6단계와 병행**: 헤더 수정은 렌더링 함수와 같이 진행
4. **8단계는 선택적**: 이벤트 핸들러는 나중에 추가 가능
5. **9단계는 5단계 이후**: 텍스트 최적화는 기본 렌더링 완료 후 진행
6. **10단계는 지속적**: 각 단계 완료 시마다 부분 테스트 수행

## 주요 변경사항 요약

### 제거되는 기능
- IP 주소 표시 (WiFi STA, WiFi AP, Ethernet)
- "STATUS DASHBOARD" 제목 라인

### 추가되는 기능
- 연결 상태 표시 (WiFi, ETH, Switcher)
- 체크마크/엑스마크 아이콘
- Tally 정보와 연결 상태 하이브리드 레이아웃

### 수정되는 기능
- 헤더: draw_tx_header() 수정 ("DASHBOARD" 텍스트 추가, 페이지 번호 중앙 이동)
- Tally 정보 텍스트 크기 조절
- 레이아웃: 단일 목적 → 하이브리드 목적

## 다음 단계 (Next Steps)

1. 구현 시작 전 acceptance.md의 Given-When-Then 시나리오 검토
2. 기존 코드 분석 시 AHD (Architecture Header Diagram) 작성 권장
3. 각 단계 완료 시 git commit (단위 커밋 원칙)
4. 완료 후 /moai:3-sync SPEC-STATUS-DASHBOARD-001 실행으로 문서화
