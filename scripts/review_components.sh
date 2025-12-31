#!/bin/bash
# =============================================================================
# review_components.sh - 컴포넌트 구조체 리뷰 스크립트
#
# 사용법:
#   ./scripts/review_components.sh              # 전체 리뷰
#   ./scripts/review_components.sh --unused     # 사용하지 않는 구조체만
#   ./scripts/review_components.sh --events     # 이벤트 구조체만
#   ./scripts/review_components.sh --layers     # 레이어 의존성만
#   ./scripts/review_components.sh --help       # 도움말
# =============================================================================

set -e

# 프로젝트 루트 경로
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
REPORT_DIR="$SCRIPT_DIR/reports"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_FILE="$REPORT_DIR/struct_review_$TIMESTAMP.txt"

# 라이브러리 로드
source "$SCRIPT_DIR/lib/extract_structs.sh"
source "$SCRIPT_DIR/lib/extract_usage.sh"
source "$SCRIPT_DIR/lib/check_events.sh"
source "$SCRIPT_DIR/lib/check_layers.sh"

# ==============================================================================
# 색상 정의
# ==============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

success() { echo -e "${GREEN}✅ $1${NC}"; }
warning() { echo -e "${YELLOW}⚠️  $1${NC}"; }
error() { echo -e "${RED}❌ $1${NC}"; }
info() { echo -e "${BLUE}ℹ️  $1${NC}"; }

# ==============================================================================
# 도움말
# ==============================================================================

show_help() {
    cat << EOF
컴포넌트 구조체 리뷰 스크립트

사용법:
  $0 [옵션]

옵션:
  --unused      사용하지 않는 구조체만 검사
  --events      이벤트 구조체 검증만 실행
  --layers      레이어 의존성 검증만 실행
  --output FILE 리포트 파일 지정 (기본: reports/struct_review_TIMESTAMP.txt)
  --no-report   파일 리포트 생성 안 함 (터미널만)
  --help        이 도움말 표시

예시:
  $0                    # 전체 리뷰
  $0 --events           # 이벤트만 검사
  $0 --layers --no-report  # 레이어 검증만, 파일 출력 없음

EOF
}

# ==============================================================================
# 사용하지 않는 구조체 검사
# ==============================================================================

check_unused_structs() {
    echo "============================================"
    echo "   [1] 사용하지 않는 구조체"
    echo "============================================"
    echo ""

    local total_unused=0
    local tmpfile
    tmpfile=$(mktemp)

    # 모든 헤더 파일에서 구조체 추출
    find "$PROJECT_ROOT/components" -name "*.h" | while read -r header; do
        local rel_path
        rel_path="${header#$PROJECT_ROOT/}"

        # 구조체 타입 추출 (_t로 끝나는 typedef)
        grep -oP 'typedef\s+(?:struct\s+)?\w+\s+\w+_t;' "$header" 2>/dev/null | \
            sed 's/.* \(\w\+_t\);/\1/' | while read -r type_name; do

            # 사용 횟수 계산 (정의 라인 제외, .h 파일도 포함)
            local usage_count
            usage_count=$(find "$PROJECT_ROOT/components" -name "*.c" -o -name "*.cpp" -o -name "*.h" | \
                xargs grep -h "$type_name" 2>/dev/null | \
                grep -v "typedef.*$type_name" | \
                grep -v "struct.*$type_name" | \
                wc -l)

            if [[ "$usage_count" -eq 0 ]]; then
                echo "$rel_path|$type_name" >> "$tmpfile"
                ((total_unused++))
            fi
        done
    done

    # 결과 출력
    if [[ -s "$tmpfile" ]]; then
        while IFS='|' read -r path type; do
            error "$path: $type (정의만 있고 사용 없음)"
        done < "$tmpfile"
    else
        success "모든 구조체가 사용 중"
    fi

    rm -f "$tmpfile"

    echo ""
    echo "총 사용하지 않는 구조체: $total_unused개"
    echo ""

    return $total_unused
}

# ==============================================================================
# 서비스-드라이버 구조체 불일치 검사
# ==============================================================================

check_service_driver_mismatch() {
    echo "============================================"
    echo "   [2] 서비스 ↔ 드라이버 구조체 불일치"
    echo "============================================"
    echo ""

    local issues=0

    # lora_service → lora_driver
    check_api_pair "lora" "03_service/lora_service" "04_driver/lora_driver"
    ((issues += $?))

    # hardware_service → battery_driver
    check_api_pair "battery" "03_service/hardware_service" "04_driver/battery_driver"
    ((issues += $?))

    echo ""
    return $issues
}

# 단일 API 쌍 확인
check_api_pair() {
    local prefix="$1"
    local service_path="$2"
    local driver_path="$3"
    local issues=0

    echo "$prefix:"

    local service_header
    local driver_header

    service_header=$(find "$PROJECT_ROOT/components/$service_path/include" -name "${prefix}_*.h" 2>/dev/null | head -1)
    driver_header=$(find "$PROJECT_ROOT/components/$driver_path/include" -name "${prefix}_*.h" 2>/dev/null | head -1)

    if [[ -z "$driver_header" ]]; then
        warning "  드라이버 헤더 없음"
        return 0
    fi

    # 드라이버 구조체 추출
    local driver_structs
    driver_structs=$(grep -oP 'typedef\s+struct[^;]*'${prefix}'[^;]*_t;' "$driver_header" 2>/dev/null | \
        grep -oP '\w+_t;' | tr -d ';' | sort -u)

    # 서비스에서 드라이버 구조체 사용 확인
    if [[ -n "$service_header" ]]; then
        echo "$driver_structs" | while read -r d_struct; do
            if [[ -n "$d_struct" ]]; then
                # 서비스에서 include 하는지 확인
                if grep -q "$driver_header" "$service_header" 2>/dev/null; then
                    success "  $d_struct: 서비스에서 드라이버 include"
                else
                    warning "  $d_struct: 드라이버에만 정의"
                fi
            fi
        done
    fi

    echo ""
    return 0
}

# ==============================================================================
# 메인 함수
# =============================================================================

main() {
    local check_unused=false
    local check_events=false
    local check_layers_only=false
    local check_all=true
    local no_report=false
    local total_issues=0

    # 인자 파싱
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --unused)
                check_unused=true
                check_all=false
                shift
                ;;
            --events)
                check_events=true
                check_all=false
                shift
                ;;
            --layers)
                check_layers_only=true
                check_all=false
                shift
                ;;
            --no-report)
                no_report=true
                shift
                ;;
            --output)
                REPORT_FILE="$2"
                shift 2
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                echo "알 수 없는 옵션: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # 리포트 디렉토리 생성
    if [[ "$no_report" == false ]]; then
        mkdir -p "$REPORT_DIR"
    fi

    # 출력 리다이렉션 (파일 + 터미널)
    if [[ "$no_report" == false ]]; then
        echo "리포트 파일: $REPORT_FILE"
        echo ""

        # 터미널과 파일에 동시 출력
        {
            run_checks "$check_unused" "$check_events" "$check_layers_only" "$check_all"
        } | tee "$REPORT_FILE"
    else
        run_checks "$check_unused" "$check_events" "$check_layers_only" "$check_all"
    fi

    # 종료 코드 반환
    if [[ $total_issues -gt 0 ]]; then
        echo ""
        echo "총 $total_issues개의 문제가 발견되었습니다."
        exit 1
    else
        echo ""
        echo "리뷰 완료: 문제 없음"
        exit 0
    fi
}

# 검사 실행
run_checks() {
    local check_unused="$1"
    local check_events="$2"
    local check_layers_only="$3"
    local check_all="$4"
    local issues=0

    # 헤더 출력
    echo "============================================"
    echo "   Component Structure Review Report"
    echo "============================================"
    echo "프로젝트: $(basename "$PROJECT_ROOT")"
    echo "날짜: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "============================================"
    echo ""

    # 전체 검사 또는 특정 검사
    if [[ "$check_all" == true ]] || [[ "$check_unused" == true ]]; then
        check_unused_structs
        ((issues += $?))
    fi

    if [[ "$check_all" == true ]] || [[ "$check_unused" == true ]]; then
        check_service_driver_mismatch
        ((issues += $?))
    fi

    if [[ "$check_all" == true ]] || [[ "$check_events" == true ]]; then
        verify_event_sizes
        ((issues += $?))
        verify_service_event_mismatch
        ((issues += $?))
    fi

    if [[ "$check_all" == true ]] || [[ "$check_layers_only" == true ]]; then
        analyze_cmake_dependencies
        detect_circular_dependencies
        verify_service_driver_structs
    fi

    # 요약
    echo ""
    echo "============================================"
    echo "   요약"
    echo "============================================"
    if [[ $issues -eq 0 ]]; then
        success "모든 검사 통과"
    else
        error "$issues개의 문제 발견"
    fi
    echo "============================================"

    total_issues=$issues
}

# ==============================================================================
# 스크립트 진입점
# =============================================================================

main "$@"
