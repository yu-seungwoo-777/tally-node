---
SPEC_ID: SPEC-DOCS-AUTO-001
Title: 자동 문서 생성 시스템
Created: 2026-01-19
Status: Planned
Priority: Medium
Assigned: workflow-spec
Related_SPECs: SPEC-DOCS-MANUAL-001, SPEC-DOCS-UI-001, SPEC-DOCS-CATALOG-001
---

# HISTORY

| 버전 | 날짜 | 변경사항 | 작성자 |
|------|------|----------|--------|
| 1.0.0 | 2026-01-19 | 초기 SPEC 작성 | Alfred |

---

# 자동 문서 생성 시스템

## 환경

### 대상 프로젝트
- 프로젝트명: Tally Node
- 하드웨어: EoRa-S3 (ESP32-S3 + SX1262 LoRa)
- 펌웨어: ESP-IDF 5.5.0 기반 C/C++
- 웹 UI: Alpine.js + TailwindCSS + DaisyUI

### 기존 문서
- `.moai/project/product.md`: 제품 개요
- `.moai/project/structure.md`: 프로젝트 구조
- `.moai/project/tech.md`: 기술 스택

### 문서 생성 대상
- 소스 코드 주석 파싱
- API 문서 자동 생성
- 컴포넌트 설명 추출

## 가정

### 선행 조건
- [A1] 소스 코드에 일관된 주석 스타일이 적용되어 있음
- [A2] Doxygen 또는 유사한 문서 생성 도구 사용 가능
- [A3] 기존 project 문서(product.md, structure.md, tech.md)가 존재함

### 기술 가정
- [T1] ESP-IDF 프로젝트 구조를 이해하고 있음
- [T2] C/C++ 코드 주석 파싱 가능
- [T3] Markdown 형식으로 문서 출력 가능

### 제약 사항
- [C1] 자동 생성된 문서는 수동 검토가 필요함
- [C2] 복잡한 로직 설명은 자동 생성에 한계가 있음

## 요구사항

### 1. 코드 주석 파싱 (Ubiquitous)

시스템은 항상 소스 코드에서 주석을 추출하고 구조화된 데이터로 변환해야 한다.

- **REQ-AUTO-001**: 시스템은 C/C++ 소스 파일에서 Doxygen 스타일 주석을 파싱할 수 있어야 한다
- **REQ-AUTO-002**: 시스템은 함수, 클래스, 구조체 주석을 추출할 수 있어야 한다
- **REQ-AUTO-003**: 시스템은 `@param`, `@return`, `@brief` 태그를 인식할 수 있어야 한다

### 2. API 문서 자동 생성 (Event-Driven)

**WHEN** 코드 변경이 감지되면, 시스템은 **THEN** API 문서를 자동으로 갱신해야 한다.

- **REQ-AUTO-004**: 소스 코드 변경 시 자동으로 문서를 재생성할 수 있어야 한다
- **REQ-AUTO-005**: 함수 시그니처 변경을 감지하고 문서를 갱신해야 한다
- **REQ-AUTO-006**: 새로운 컴포넌트 추가 시 자동으로 문서에 포함해야 한다

### 3. 컴포넌트 설명 추출 (State-Driven)

**IF** 컴포넌트가 5계층 구조를 따르는 경우, 시스템은 **THEN** 계층별 문서를 생성해야 한다.

- **REQ-AUTO-007**: 5계층(00_common ~ 05_hal) 각각의 문서를 생성할 수 있어야 한다
- **REQ-AUTO-008**: 계층 간 의존성을 문서화해야 한다
- **REQ-AUTO-009**: 컴포넌트별 책임과 역할을 명시해야 한다

### 4. 기존 문서 통합 (Optional)

가능하면 기존 project 문서와 자동 생성된 문서를 통합 제공해야 한다.

- **REQ-AUTO-010**: product.md, structure.md, tech.md 내용을 참조할 수 있어야 한다
- **REQ-AUTO-011**: 중복되는 내용을 자동으로 병합할 수 있어야 한다

### 5. 문서 형식 (Unwanted)

시스템은 일관되지 않은 형식으로 문서를 생성해서는 안 된다.

- **REQ-AUTO-012**: 생성된 모든 문서는 일관된 Markdown 형식을 따라야 한다
- **REQ-AUTO-013**: 코드 블록은 언어별 syntax highlighting이 적용되어야 한다
- **REQ-AUTO-014**: 섹션 구조는 표준화되어야 한다

## 명세

### 모듈 1: 주석 파서

**목적**: 소스 코드에서 주석을 추출하고 구조화

**입력**:
- C/C++ 소스 파일 (.cpp, .h)
- Doxygen 스타일 주석

**처리**:
1. 소스 파일 스캔
2. 주석 블록 추출
3. 태그 파싱 (@param, @return, @brief 등)
4. AST 또는 구조체로 변환

**출력**:
- JSON 또는 중간 데이터 구조
- 함수/클래스 메타데이터

### 모듈 2: 문서 생성기

**목적**: 파싱된 데이터에서 Markdown 문서 생성

**입력**:
- 주석 파서 출력 데이터
- 기존 project 문서 참조

**처리**:
1. 템플릿 적용
2. 카테고리별 분류 (API, 컴포넌트, 서비스 등)
3. 크로스레퍼런스 생성
4. Markdown 형식으로 렌더링

**출력**:
- API 문서 (functions.md, classes.md)
- 컴포넌트 문서 (components.md)
- 아키텍처 문서 (architecture.md)

### 모듈 3: 변경 감지기

**목적**: 코드 변경 시 자동 문서 갱신 트리거

**입력**:
- 파일 시스템 이벤트
- Git commit hook

**처리**:
1. 변경된 파일 목록 수집
2. 영향받는 컴포넌트 식별
3. 문서 재생성 트리거
4. 변경 로그 기록

**출력**:
- 갱신된 문서 파일
- 변경 보고서

### 모듈 4: 통합 엔진

**목적**: 자동 생성 문서와 기존 문서 통합

**입력**:
- 자동 생성된 문서
- 기존 project 문서

**처리**:
1. 중복 콘텐츠 감지
2. 내용 병합
3. 충돌 해결 (자동 생성 우선)
4. 최종 문서 조립

**출력**:
- 통합된 문서 세트
- 통합 로그

### 모듈 5: 검증기

**목적**: 생성된 문서 품질 검증

**입력**:
- 생성된 문서
- 검증 규칙

**처리**:
1. 링크 유효성 검사
2. 누락된 문서 감지
3. 일관성 검사
4. 스타일 가이드 준수 확인

**출력**:
- 검증 보고서
- 수정 제안

## 추적성 태그

### 기능 추적
- `FEAT-AUTO-PARSER`: 주석 파서 기능
- `FEAT-AUTO-GEN`: 문서 생성기 기능
- `FEAT-AUTO-DETECT`: 변경 감지 기능
- `FEAT-AUTO-INTEGRATE`: 통합 엔진 기능
- `FEAT-AUTO-VALIDATE`: 검증기 기능

### 품질 추적
- `QUAL-AUTO-CONSISTENCY`: 문서 일관성
- `QUAL-AUTO-COMPLETENESS`: 문서 완결성
- `QUAL-AUTO-TIMELINESS`: 변경 반영 시기

### 의존성 추적
- `DEP-AUTO-SOURCE`: 소스 코드 의존성
- `DEP-AUTO-PROJ-DOCS`: 기존 project 문서 의존성
- `DEP-AUTO-DOXYGEN`: Doxygen 도구 의존성
