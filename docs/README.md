# 자동 문서 생성 시스템 (Automatic Documentation Generation System)

Tally Node 프로젝트의 C/C++ 소스 코드에서 자동으로 기술 문서를 생성하는 시스템입니다.

## 개요

이 시스템은 Doxygen을 사용하여 소스 코드에서 주석을 추출하고, Python 스크립트로 파싱하여 Markdown 형식의 문서를 자동 생성합니다.

## 기능

- **C/C++ 주석 파싱**: Doxygen 스타일 주석 자동 추출
- **5계층 아키텍처 문서화**: 0_common ~ 4_infrastructure 계층별 문서 생성
- **API 문서 생성**: 함수 및 클래스 레퍼런스 자동 생성
- **Markdown 출력**: 일관된 형식의 Markdown 문서 생성

## 설치

### 의존성

- Doxygen 1.9.8+
- Python 3.12+
- Python 패키지: jinja2, pyyaml, lxml, pytest

### 설치 방법

```bash
# Doxygen 설치 (Ubuntu/Debian)
sudo apt-get install doxygen graphviz

# Python 가상 환경 생성
python3 -m venv docs/venv
source docs/venv/bin/activate

# Python 패키지 설치
pip install -r requirements.txt
```

## 사용법

### 기본 사용

```bash
# 문서 생성 (전체 프로젝트)
cd /home/prod/tally-node
python3 docs/tools/generate_docs.py examples/2/components

# 또는 직접 실행
cd docs
./tools/generate_docs.py ../examples/2/components -o output
```

### 옵션

```
usage: generate_docs.py [-h] [-o OUTPUT] source_root

positional arguments:
  source_root           소스 코드 루트 디렉토리

optional arguments:
  -h, --help            도움말 표시
  -o OUTPUT, --output OUTPUT
                        출력 디렉토리 (기본값: docs/output)
  --skip-doxygen        Doxygen 실행 건너뛰기 (이미 XML이 있는 경우)
```

## 디렉토리 구조

```
docs/
├── tools/                      # 문서 생성 도구
│   ├── models.py              # 데이터 모델
│   ├── parser.py              # Doxygen XML 파서
│   ├── generator.py           # Markdown 문서 생성기
│   ├── scan_project.py        # 프로젝트 스캐너
│   └── generate_docs.py       # 메인 실행 스크립트
├── templates/                  # Jinja2 템플릿 (옵션)
├── tests/                      # 테스트
│   ├── test_parser.py
│   ├── test_generator.py
│   └── test_integration.py
├── output/                     # 생성된 문서 (기본 출력 위치)
│   ├── markdown/
│   │   ├── README.md
│   │   ├── api/
│   │   ├── layers/
│   │   └── components/
│   └── doxygen_xml/
└── README.md
```

## 5계층 아키텍처

시스템은 다음 5계층 구조를 인식하고 문서화합니다:

1. **0_common**: 공통 유틸리티 및 기반 기능
2. **1_presentation**: UI/표현 계층 (Display, Web)
3. **2_application**: 애플리케이션 비즈니스 로직
4. **3_domain**: 도메인 모델 및 핵심 로직
5. **4_infrastructure**: 인프라 계층 (Network, LoRa 드라이버)

## 출력 형식

생성되는 문서:

- `README.md`: 메인 인덱스
- `api/functions.md`: 함수 API 레퍼런스
- `api/classes.md`: 클래스 API 레퍼런스
- `layers/*.md`: 계층별 문서 (0_common.md, 1_presentation.md, etc.)

## 테스트

```bash
# 모든 테스트 실행
pytest docs/tests/ -v

# 커버리지 확인
pytest docs/tests/ --cov=docs/tools --cov-report=html

# 빠른 테스트 (slow 테스트 제외)
pytest docs/tests/ -k "not slow" -v
```

## 예제

### 함수 문서 예제

```cpp
/**
 * @brief Calculate the sum of two numbers
 *
 * This function adds two integers and returns the result.
 *
 * @param a First number
 * @param b Second number
 * @return Sum of a and b
 */
int calculateSum(int a, int b);
```

생성되는 Markdown:

```markdown
### calculateSum

Calculate the sum of two numbers

```cpp
int calculateSum(int a, int b)
```

**Parameters:**
- `int a`: First number
- `int b`: Second number

**Returns:** Sum of a and b
```

## 요구사항 충족

이 시스템은 다음 요구사항을 충족합니다:

- ✅ **REQ-AUTO-001**: Doxygen 스타일 주석 파싱
- ✅ **REQ-AUTO-002**: 함수, 클래스, 구조체 추출
- ✅ **REQ-AUTO-003**: @param, @return, @brief 태그 인식
- ✅ **REQ-AUTO-007**: 5계층 문서 생성
- ✅ **REQ-AUTO-008**: 계층 간 의존성 문서화
- ✅ **REQ-AUTO-012**: 일관된 Markdown 형식
- ✅ **REQ-AUTO-013**: Syntax highlighting 적용

## 라이선스

MIT License

## 버전

Version 1.0.0 - 2026-01-19
