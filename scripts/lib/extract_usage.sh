#!/bin/bash
# =============================================================================
# extract_usage.sh - 구조체 사용 패턴 추출 라이브러리
# =============================================================================

# 프로젝트 루트 경로 (source될 때도 올바른 경로 계산)
_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$_LIB_DIR/../.." && pwd)"

# ==============================================================================
# 구조체 사용 추출 함수
# ==============================================================================

# 특정 구조체 타입이 사용된 모든 위치 추출
# $1: 구조체 타입 이름 (예: lora_config_t)
find_type_usage() {
    local type_name="$1"

    find "$PROJECT_ROOT/components" "$PROJECT_ROOT/src" -name "*.c" -o -name "*.cpp" -o -name "*.h" | \
        xargs grep -n "$type_name" 2>/dev/null | \
        grep -v "typedef.*$type_name" | \
        grep -v "struct.*$type_name"
}

# 구조체가 사용되지 않았는지 확인
# $1: 구조체 타입 이름
is_unused_type() {
    local type_name="$1"
    local usage_count

    usage_count=$(find_type_usage "$type_name" | wc -l)
    echo "$usage_count"
}

# ==============================================================================
# 이벤트 발행/구독 추출
# ==============================================================================

# event_bus_publish 호출 추출
# $1?: 검색할 디렉토리 (기본: components)
extract_event_publish() {
    local search_dir="${1:-$PROJECT_ROOT/components}"

    find "$search_dir" -name "*.c" -o -name "*.cpp" | \
        xargs grep -n 'event_bus_publish' 2>/dev/null | \
        sed 's/^.*:/event_bus_publish:/'
}

# event_bus_subscribe 호출 추출
extract_event_subscribe() {
    local search_dir="${1:-$PROJECT_ROOT/components}"

    find "$search_dir" -name "*.c" -o -name "*.cpp" | \
        xargs grep -n 'event_bus_subscribe' 2>/dev/null | \
        sed 's/^.*:/event_bus_subscribe:/'
}

# 이벤트 발행 시 전달되는 데이터 크기 추출
# sizeof() 호출 찾기
extract_event_data_sizes() {
    local file="$1"

    grep -oP 'event_bus_publish\s*\([^,]+,\s*[^,]+,\s*sizeof\s*\(\s*\w+\s*\)' "$file" 2>/dev/null | \
        sed 's/.*sizeof(\(\w\+)).*/\1/'
}

# ==============================================================================
# 함수 인자 구조체 추출
# ==============================================================================

# 함수 선언에서 구조체 인자 추출
# $1: 헤더 파일 경로
extract_function_param_structs() {
    local file="$1"

    # 함수 포인터 콜백의 구조체 인자 추출
    grep -oP '\w+\s*\(\s*\w+_t\s*\*?\s*\w+\s*\)' "$file" 2>/dev/null | \
        grep -oP '\w+_t' | sort -u

    # 일반 함수의 구조체 인자 추출
    grep -oP '^\s*\w+\s+\w+\s*\([^;]*\w+_t[^;]*\);' "$file" 2>/dev/null | \
        grep -oP '\w+_t' | sort -u
}

# API 함수와 인자 구조체 매핑
# $1: 서비스/드라이버 헤더 파일
extract_api_struct_mapping() {
    local file="$1"
    local basename

    basename=$(basename "$file" .h)

    # extern "C" 함수 추출
    awk '/extern "C"/,/}/' "$file" 2>/dev/null | \
        grep 'esp_err_t\|void\|bool\|int' | \
        grep -oP '\w+\s*\([^)]*\)' | \
        while read -r func; do
            func_name=$(echo "$func" | grep -oP '^\w+(?=\()')
            params=$(echo "$func" | grep -oP '(?<=\().*(?=\))')
            struct_types=$(echo "$params" | grep -oP '\w+_t\*?' | tr '\n' ',')
            echo "$func_name|$struct_types"
        done
}

# ==============================================================================
# 드라이버 API 구조체 추출
# ==============================================================================

# 드라이버 헤더에서 노출된 구조체 타입 추출
extract_driver_types() {
    local driver_dir="$1"

    find "$driver_dir/include" -name "*.h" | while read -r file; do
        grep -oP 'typedef\s+(?:struct\s+)?\w+\s+\w+_t;' "$file" 2>/dev/null | \
            sed 's/.* \(\w\+_t\);/\1|'"$(basename "$file" .h)"'/'
    done
}

# 서비스 API 구조체 추출
extract_service_types() {
    local service_dir="$1"

    find "$service_dir/include" -name "*.h" | while read -r file; do
        grep -oP 'typedef\s+(?:struct\s+)?\w+\s+\w+_t;' "$file" 2>/dev/null | \
            sed 's/.* \(\w\+_t\);/\1|'"$(basename "$file" .h)"'/'
    done
}

# ==============================================================================
# CMakeLists.txt REQUIRES 추출
# ==============================================================================

# CMakeLists.txt의 REQUIRES 의존성 추출
extract_cmake_requires() {
    local cmakelists="$1"

    if [[ ! -f "$cmakelists" ]]; then
        return
    fi

    grep -oP 'REQUIRES\s*\K[^)]+' "$cmakelists" | tr ';' '\n' | grep -v '^\s*$' | sort -u
}

# 모든 컴포넌트의 REQUIRES 추출
extract_all_requires() {
    find "$PROJECT_ROOT/components" -name "CMakeLists.txt" | while read -r file; do
        local component
        component=$(echo "$file" | sed 's|.*/components/\([^/]*\)/.*|\1|')
        local requires
        requires=$(extract_cmake_requires "$file")
        if [[ -n "$requires" ]]; then
            echo "$component|$requires"
        fi
    done
}

# ==============================================================================
# 함수 내보내기
# ==============================================================================

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # 직접 실행 시 테스트
    echo "=== 사용 패턴 추출 테스트 ==="
    echo ""
    echo "1. lora_config_t 사용:"
    find_type_usage "lora_config_t" | head -5
    echo ""
    echo "2. 이벤트 발행:"
    extract_event_publish "$PROJECT_ROOT/components/03_service" | head -3
    echo ""
    echo "3. CMake REQUIRES:"
    extract_all_requires | head -3
fi
