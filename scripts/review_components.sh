#!/bin/bash
# 컴포넌트 리뷰 스크립트 v2
# 레이어별로 Claude CLI를 병렬로 실행하여 컴포넌트를 심층 리뷰합니다.

set -e

# ============================================================================
# 설정
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ARCH_DOC="$PROJECT_DIR/docs/ARCHITECTURE.md"
TMP_DIR="$PROJECT_DIR/docs/tmp"
COMPONENT_DIR="$PROJECT_DIR/components"

# 레이어 목록 (순서 중요 - 의존성 방향)
LAYERS=("00_common" "05_hal" "04_driver" "03_service" "02_presentation" "01_app")

# 레이어별 설명
declare -A LAYER_DESC
LAYER_DESC[00_common]="공통 유틸리티 및 타입 정의 (의존 없음)"
LAYER_DESC[01_app]="애플리케이션 계층 (최상위)"
LAYER_DESC[02_presentation]="프레젠테이션 계층 (디스플레이, 웹, LED)"
LAYER_DESC[03_service]="서비스 계층 (비즈니스 로직)"
LAYER_DESC[04_driver]="드라이버 계층 (하드웨어 추상화)"
LAYER_DESC[05_hal]="HAL 계층 (저수준 하드웨어 접근)"

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# ============================================================================
# 도우미 함수
# ============================================================================

log_info() { echo -e "${CYAN}>>> $1${NC}"; }
log_success() { echo -e "${GREEN}✓ $1${NC}"; }
log_warning() { echo -e "${YELLOW}⚠ $1${NC}"; }
log_error() { echo -e "${RED}✗ $1${NC}"; }

# ============================================================================
# 사전 체크
# ============================================================================

if ! command -v claude &> /dev/null; then
    log_error "Claude CLI를 찾을 수 없습니다."
    exit 1
fi

mkdir -p "$TMP_DIR"

if [ ! -f "$ARCH_DOC" ]; then
    log_error "ARCHITECTURE.md 파일을 찾을 수 없습니다: $ARCH_DOC"
    exit 1
fi

# ============================================================================
# 컴포넌트 정보 수집 함수
# ============================================================================

# 컴포넌트의 CMakeLists.txt에서 REQUIRES 추출
get_requires() {
    local cmake_file="$1"
    if [ ! -f "$cmake_file" ]; then
        echo "(N/A)"
        return
    fi
    grep -E "^\s*REQUIRES" "$cmake_file" | sed 's/.*REQUIRES//' | tr '\n' ' ' | sed 's/  / /g'
}

# 컴포넌트의 소스 파일 목록
get_sources() {
    local comp_dir="$1"
    find "$comp_dir" -maxdepth 1 -type f \( -name "*.c" -o -name "*.cpp" \) 2>/dev/null | xargs -I{} basename "{}" | tr '\n' ' '
}

# 컴포넌트의 헤더 파일 목록
get_headers() {
    local comp_dir="$1"
    find "$comp_dir/include" -maxdepth 1 -type f \( -name "*.h" -o -name "*.hpp" \) 2>/dev/null | xargs -I{} basename "{}" | tr '\n' ' '
}

# src/ 폴더 사용 여부 확인
has_src_folder() {
    local comp_dir="$1"
    if [ -d "$comp_dir/src" ]; then
        # src 폴더에 소스 파일이 있는지 확인
        if [ -n "$(find "$comp_dir/src" -type f \( -name "*.c" -o -name "*.cpp" \) 2>/dev/null)" ]; then
            echo "true"
        else
            echo "false"
        fi
    else
        echo "false"
    fi
}

# 헤더 파일의 extern "C" 사용 여부
has_extern_c() {
    local header_file="$1"
    if [ ! -f "$header_file" ]; then
        echo "unknown"
        return
    fi
    if grep -q 'extern "C"' "$header_file"; then
        echo "true"
    else
        echo "false"
    fi
}

# 파일 주석 비율 계산 (간단)
calc_comment_ratio() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "0"
        return
    fi
    local total_lines=$(wc -l < "$file")
    local comment_lines=$(grep -c "^\s*//" "$file" 2>/dev/null || echo 0)
    local block_comments=$(grep -c "^\s*\*" "$file" 2>/dev/null || echo 0)
    local total_comments=$((comment_lines + block_comments))
    if [ $total_lines -gt 0 ]; then
        echo "$((total_comments * 100 / total_lines))"
    else
        echo "0"
    fi
}

# ============================================================================
# 레이어별 컴포넌트 상세 정보 수집
# ============================================================================

collect_layer_info() {
    local layer="$1"
    local layer_path="$COMPONENT_DIR/$layer"

    if [ ! -d "$layer_path" ]; then
        echo "## 레이어 폴더 없음"
        return
    fi

    echo "## 컴포넌트 상세 정보"
    echo ""

    # 각 컴포넌트 분석
    find "$layer_path" -mindepth 2 -maxdepth 5 -type d -name "include" 2>/dev/null | \
        sed "s|$layer_path/||" | sed 's|/include||' | sort | \
        while read -r comp; do
            [ -z "$comp" ] && continue

            local comp_dir="$layer_path/$comp"
            local cmake_file="$comp_dir/CMakeLists.txt"
            local comp_name=$(basename "$comp")

            echo "### $comp"
            echo ""

            # CMakeLists.txt 정보
            if [ -f "$cmake_file" ]; then
                echo "- **CMakeLists.txt**: 존재"
                requires=$(get_requires "$cmake_file")
                echo "  - \`REQUIRES\`: $requires"
            else
                echo "- **CMakeLists.txt**: ❌ 없음"
            fi

            # 소스 파일
            sources=$(get_sources "$comp_dir")
            if [ -n "$sources" ]; then
                echo "  - **SRCS**: $sources"
            else
                echo "  - **SRCS**: (없음 - 메타 컴포넌트?)"
            fi

            # 헤더 파일
            headers=$(get_headers "$comp_dir")
            if [ -n "$headers" ]; then
                echo "  - **HEADERS**: $headers"
            fi

            # src/ 폴더 사용 여부
            if [ "$(has_src_folder "$comp_dir")" = "true" ]; then
                echo "  - **src/ 폴더**: ⚠️ 사용 중 (규칙 위반)"
            fi

            # 첫 번째 헤더의 extern "C" 확인
            first_header=$(find "$comp_dir/include" -maxdepth 1 -type f -name "*.h" 2>/dev/null | head -1)
            if [ -n "$first_header" ]; then
                extern_c=$(has_extern_c "$first_header")
                if [ "$extern_c" = "true" ]; then
                    echo "  - **extern \"C\"**: ✅ 사용"
                else
                    echo "  - **extern \"C\"**: ❌ 미사용"
                fi
            fi

            echo ""
        done
}

# ============================================================================
# ARCHITECTURE.md 섹션 추출
# ============================================================================

get_arch_section() {
    local layer=$1
    local layer_name="${layer#*_}"

    case "$layer_name" in
        common)    layer_title="00_common (공통)" ;;
        app)       layer_title="01_app (앱)" ;;
        presentation) layer_title="02_presentation (프레젠테이션)" ;;
        service)   layer_title="03_service (서비스)" ;;
        driver)    layer_title="04_driver (드라이버)" ;;
        hal)       layer_title="05_hal (HAL)" ;;
        *)         layer_title="$layer" ;;
    esac

    awk "
        /## 5계층 아키텍처/ { in_arch = 1 }
        in_arch && /$layer_title/ { in_layer = 1; print; next }
        in_layer && /^│/ { print; next }
        in_layer && /^└/ { print; next }
        in_layer && /^$/ { print; next }
        in_layer && /──/ { in_layer = 0 }
    " "$ARCH_DOC"
}

# ============================================================================
# 전체 의존성 맵 수집
# ============================================================================

collect_dependency_map() {
    echo "## 전체 의존성 맵"
    echo ""
    echo "| 컴포넌트 | REQUIRES |"
    echo "|----------|----------|"

    for layer_path in "$COMPONENT_DIR"/*; do
        layer=$(basename "$layer_path")
        [[ "$layer" =~ ^(00|01|02|03|04|05)_ ]] || continue

        find "$layer_path" -mindepth 2 -maxdepth 5 -name "CMakeLists.txt" -type f 2>/dev/null | \
            while read -r cmake_file; do
                comp_dir=$(dirname "$cmake_file")
                comp_path="${comp_dir#$COMPONENT_DIR/}"
                requires=$(grep -E "^\s*REQUIRES" "$cmake_file" 2>/dev/null | sed 's/.*REQUIRES//' | tr '\n' ',' | sed 's/,$//')
                if [ -n "$requires" ]; then
                    echo "| $comp_path | $requires |"
                fi
            done
    done
    echo ""
}

# ============================================================================
# 리뷰 프롬프트 생성
# ============================================================================

generate_prompt() {
    local layer=$1
    local layer_desc=$2
    local arch_content=$3
    local layer_info=$4
    local dep_map=$5

    cat << EOF
작업: ${layer} 레이어 컴포넌트 심층 리뷰

---
## 레이어 정보
- **레이어**: ${layer}
- **설명**: ${layer_desc}

---
## ARCHITECTURE.md 내용 (관련 섹션)
\`\`\`
${arch_content}
\`\`\`

---
## 실제 컴포넌트 분석
${layer_info}

---
## 전체 의존성 맵 (참고용)
${dep_map}

---
## 리뷰 지시사항

${layer} 레이어 컴포넌트를 다음 항목으로 **철저히** 리뷰하세요. 실제 데이터를 바탕으로 분석해야 합니다.

### 1. 구조 일치성 검사
- ARCHITECTURE.md에 기술된 컴포넌트 목록 vs 실제 폴더 구조
- 일치/누락/추가된 컴포넌트 구체적 명시
- 메타 컴포넌트(소스 없는 컴포넌트) 구분

### 2. 의존성 분석 (중요)
- 각 컴포넌트의 REQUIRES 항목 실제 확인
- **5계층 규칙 위반 검사**:
  - 상위 계층이 하위 계층을 의존하는 것은 정상
  - 하위 계층이 상위 계층을 의존하면 위반
  - 예: 03_service가 01_app 의존 → ❌ 위반
- **순환 의존성 검사**:
  - A → B이고 B → A인 경우
  - REQUIRES 목록을 추적하여 순환 발견 시 보고
- 계층 간 의존 방향 시각화

### 3. 인터페이스 검사
- 헤더 파일(include/*.h) 존재 여부
- extern "C" 사용 여부 (C++ 컴포넌트의 C 인터페이스 제공)
- 공개 API 명확성
- 불필요한 include 제거 여부

### 4. 코드 품질 기초 검사
- 한글 주석 사용 여부 (파일 헤더, 함수 주석)
- 네이밍 규칙 준수 (snake_case, UPPER_SNAKE_CASE)
- 파일 배치 규칙 (소스는 루트, 헤더는 include/)
- **src/ 폴더 미사용 규칙** 위반 여부

### 4.1 구조체 초기화 검사 (중요)
- C++ 파일(.cpp)에서 구조체 초기화 방식 확인
- **Designated Initializer (C 스타일)** 사용 시 문제:
  \`\`\`cpp
  // ❌ C++에서 비권장 - 컴파일러 호환성 문제
  lora_cmd_brightness_t cmd = {
      .header = LORA_HDR_SET_BRIGHTNESS,
      .brightness = brightness,
  };
  \`\`\`
- **개별 할당 (C++ 스타일)** 권장:
  \`\`\`cpp
  // ✅ C++ 호환 방식
  lora_cmd_brightness_t cmd;
  cmd.header = LORA_HDR_SET_BRIGHTNESS;
  cmd.brightness = brightness;
  \`\`\`
- 구조체가 extern "C" 헤더에 정의된 경우 designated initializer 사용 가능
- .cpp 파일 내에서 정의된 구조체는 개별 할당 사용 권장

### 5. CMakeLists.txt 검사
- idf_component_register 존재 여부
- SRCS에 지정된 파일 실제 존재 여부
- INCLUDE_DIRS "include" 사용 여부
- REQUIRES 항목의 실제 컴포넌트 존재 여부

### 6. 개선 제안
- 우선순위별 (🔴높음 / 🟡중간 / 🟢낮음)로 정리
- 구체적인 리팩토링 방안 제시
- ARCHITECTURE.md 수정이 필요한 경우 명시

---
## 출력 형식

다음 형식을 엄격하게 준수하여 리뷰 결과를 출력:

# ${layer} 레이어 컴포넌트 리뷰

## 1. 구조 일치성

### 문서 vs 실제
| 컴포넌트 | 문서 | 실제 | 상태 |
|---------|------|------|------|
| 예: event_bus | ✅ | ✅ | 일치 |
| ... | ... | ... | ... |

### 요약
- 일치: N개
- 누락됨: N개
- 추가됨: N개

---

## 2. 의존성 분석

### 의존성 그래프
\`\`\`
[Mermaid 또는 ASCII로 의존성 그래프 작성]
\`\`\`

### 문제 발견
#### 순환 의존성
- [있음/없음]
  - A → B, B → A 구체적 명시

#### 계층 위반
- [있음/없음]
  - 위반 사항 구체적 명시

### 정상 의존성
[정상적인 의존성 관계 요약]

---

## 3. 인터페이스 검사

| 컴포넌트 | 헤더 파일 | extern "C" | 공개 API | 상태 |
|----------|-----------|------------|----------|------|
| ... | ... | ... | ... | ... |

---

## 4. CMakeLists.txt 검사

| 컴포넌트 | idf_component_register | SRCS 유효 | INCLUDE_DIRS | 상태 |
|----------|------------------------|-----------|---------------|------|
| ... | ... | ... | ... | ... |

---

## 5. 코드 품질

| 컴포넌트 | 주석 | 네이밍 | src/ 사용 | 상태 |
|----------|------|--------|-----------|------|
| ... | ... | ... | ... | ... |

### 구조체 초기화 검사

| 파일 | 초기화 방식 | 상태 | 비고 |
|------|------------|------|------|
| ... | designated/개별할당 | ✅/⚠️/❌ | |

- **Designated Initializer (.field = value)**: C 스타일, C++에서 호환성 문제 가능
- **개별 할당**: C++ 호환 방식 권장

---

## 6. 아키텍처 규칙 준수

| 규칙 | 상태 | 비고 |
|------|------|------|
| src/ 미사용 | ✅/⚠️/❌ | |
| 단일 책임 | ✅/⚠️/❌ | |
| 적정 계층 배치 | ✅/⚠️/❌ | |
| 의존 방향 준수 | ✅/⚠️/❌ | |

---

## 7. 개선 제안

### 🔴 높음 우선순위
1. **[문제점]** - 구체적 설명
   - 해결 방안: ...

### 🟡 중간 우선순위
1. ...

### 🟢 낮음 우선순위
1. ...

---

## 8. ARCHITECTURE.md 수정 제안

[문서 수정이 필요한 경우 구체적 내용]

---

위 형식을 따르지 않거나, 실제 데이터를 무시하고 분석한 결과는 부정적으로 평가됩니다.
EOF
}

# ============================================================================
# 병렬 리뷰 실행
# ============================================================================

log_info "컴포넌트 심층 리뷰 시작 (병렬 실행)"
log_info "리뷰 결과: $TMP_DIR/"
echo ""

# 전체 의존성 맵 수집 (모든 레이어 공통)
DEP_MAP=$(collect_dependency_map)

# 백그라운드 프로세스 ID 저장
PIDS=()

# 각 레이어별로 병렬 실행
for layer in "${LAYERS[@]}"; do
    layer_desc="${LAYER_DESC[$layer]}"
    output_file="$TMP_DIR/${layer}_review.md"
    log_file="$TMP_DIR/${layer}_log.txt"

    # 레이어 정보 수집
    layer_info=$(collect_layer_info "$layer")

    # ARCHITECTURE.md 섹션
    arch_section=$(get_arch_section "$layer")

    # 프롬프트 생성
    prompt=$(generate_prompt "$layer" "$layer_desc" "$arch_section" "$layer_info" "$DEP_MAP")

    # 백그라운드로 실행
    (
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ${layer} 리뷰 시작..." > "$log_file"
        result=$(echo "$prompt" | claude 2>&1)
        echo "$result" > "$output_file"
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ${layer} 리뷰 완료" >> "$log_file"
    ) &

    PIDS+=($!)
    log_info "[${layer}] 리뷰 태스크 시작 (PID: ${PIDS[-1]})..."
done

echo ""
log_info "모든 리뷰 태스크가 백그라운드에서 실행 중입니다..."
log_info "진행 상황 확인: tail -f $TMP_DIR/*_log.txt"
echo ""

# ============================================================================
# 진행 상황 모니터링
# ============================================================================

completed=0
total=${#LAYERS[@]}

while [ $completed -lt $total ]; do
    sleep 2
    completed=0

    for layer in "${LAYERS[@]}"; do
        log_file="$TMP_DIR/${layer}_log.txt"
        output_file="$TMP_DIR/${layer}_review.md"

        if grep -q "리뷰 완료" "$log_file" 2>/dev/null; then
            completed=$((completed + 1))
            if [ -f "$output_file" ]; then
                size=$(wc -c < "$output_file")
                printf "   ${GREEN}✓${NC} %-20s : 완료 (%d bytes)\n" "$layer" "$size"
            fi
        else
            printf "   ${YELLOW}○${NC} %-20s : 진행 중...\n" "$layer"
        fi
    done

    if [ $completed -lt $total ]; then
        printf "\033[%dA" "$total"
    fi
done

echo ""
log_success "모든 리뷰 완료!"
echo ""

# ============================================================================
# 결과 요약
# ============================================================================

log_info "리뷰 결과 파일:"
echo ""

for layer in "${LAYERS[@]}"; do
    output_file="$TMP_DIR/${layer}_review.md"
    if [ -f "$output_file" ]; then
        size=$(wc -l < "$output_file")
        printf "  %-20s → %4d lines\n" "$layer" "$size"
    fi
done

echo ""
log_info "전체 리뷰 보기: cat $TMP_DIR/*_review.md"
log_info "특정 레이어 보기: cat $TMP_DIR/{레이어}_review.md"
echo ""
