#!/bin/bash
# =============================================================================
# extract_structs.sh - 구조체 정의 추출 라이브러리
# =============================================================================

# 프로젝트 루트 경로 (source될 때도 올바른 경로 계산)
_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$_LIB_DIR/../.." && pwd)"

# ==============================================================================
# 구조체 정의 추출 함수
# ==============================================================================

# 헤더 파일에서 모든 구조체 정의 추출
# $1: 검색할 디렉토리 경로
extract_all_structs() {
    local search_dir="$1"
    find "$search_dir" -name "*.h" -type f | while read -r file; do
        extract_structs_from_file "$file"
    done
}

# 단일 파일에서 구조체 추출
# $1: 파일 경로
extract_structs_from_file() {
    local file="$1"
    local rel_path="${file#$PROJECT_ROOT/}"

    # typedef struct 이름 추출
    grep -oP 'typedef\s+(?:struct\s+)?(?:\w+\s+)?(?:struct\s+)?\w+\s+\w+_t;' "$file" 2>/dev/null | \
        sed 's/typedef[^;]* \(\w\+_t\);/\1/' | while read -r type_name; do
            echo "$rel_path|$type_name|typedef"
    done

    # struct xxx { 형태 추출
    grep -oP 'struct\s+\w+\s*\{' "$file" 2>/dev/null | \
        sed 's/struct \(\w\+) {.*/\1/' | while read -r struct_name; do
            # typedef으로 정의되지 않은 struct만
            if ! grep -q "typedef.*$struct_name.*_t" "$file"; then
                echo "$rel_path|$struct_name|struct"
            fi
    done

    # typedef struct __attribute__\(\(packed\)\) 추출
    grep -oP 'typedef\s+struct\s+__attribute__\(\(packed\)\)\s*\w+\s+\w+_t;' "$file" 2>/dev/null | \
        sed 's/.* \(\w\+_t\);/\1|packed/' | while read -r line; do
            echo "$rel_path|$line|typedef_packed"
    done
}

# 구조체 크기 계산 (필드 분석 기반)
# $1: 구조체 타입 이름
# $2: 검색할 파일
estimate_struct_size() {
    local type_name="$1"
    local file="$2"

    # 구조체 정의 블록 추출
    awk "/typedef struct.*$type_name/,/$type_name;/" "$file" 2>/dev/null | \
        grep -E '(int8_t|int16_t|int32_t|int64_t|uint8_t|uint16_t|uint32_t|uint64_t|float|double|bool|char\s*\[)' | \
        awk '{
            size = 0
            for (i=1; i<=NF; i++) {
                if ($i ~ /int8_t|uint8_t|bool/) size += 1
                else if ($i ~ /int16_t|uint16_t/) size += 2
                else if ($i ~ /int32_t|uint32_t|float/) size += 4
                else if ($i ~ /int64_t|uint64_t|double/) size += 8
                else if ($i ~ /char\s*\[/) {
                    match($i, /[0-9]+/, arr)
                    if (arr[0] > 0) size += arr[0]
                    else size += 1
                }
            }
            print size
        }' | tail -1
}

# ==============================================================================
# 이벤트 구조체 추출 (event_bus.h 전용)
# ==============================================================================

# 이벤트 버스에 정의된 모든 이벤트 데이터 구조체 추출
extract_event_structs() {
    local event_bus="$PROJECT_ROOT/components/00_common/event_bus/include/event_bus.h"

    if [[ ! -f "$event_bus" ]]; then
        echo ""
        return
    fi

    # 이벤트 데이터 구조체 (_event_t, _request_t, _status_t 등)
    grep -oP 'typedef\s+struct\s+(?:__attribute__\(\(packed\)\)\s+)?\w*\s*\w+_t;' "$event_bus" | \
        sed 's/typedef[^;]* \(\w\+_t\);/\1/' | sort
}

# 이벤트 타입과 해당 데이터 구조체 매핑 추출
extract_event_type_mapping() {
    local event_bus="$PROJECT_ROOT/components/00_common/event_bus/include/event_bus.h"

    if [[ ! -f "$event_bus" ]]; then
        echo ""
        return
    fi

    # EVT_xxx와 해당 data 타입 추출 (주석에서)
    grep 'EVT_\w\+.*data:' "$event_bus" | \
        sed 's/.*EVT_\w\+.*<data: \(\w\+_t\)>/\1/' | grep -v '^$'
}

# ==============================================================================
# 함수 내보내기
# ==============================================================================

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # 직접 실행 시 테스트
    echo "=== 구조체 추출 테스트 ==="
    extract_all_structs "$PROJECT_ROOT/components/00_common/event_bus/include"
fi
