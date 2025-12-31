#!/bin/bash
# =============================================================================
# check_layers.sh - 레이어 의존성 검증 라이브러리
# =============================================================================

# 프로젝트 루트 경로 (source될 때도 올바른 경로 계산)
_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$_LIB_DIR/../.." && pwd)"

# 라이브러리 로드
source "$_LIB_DIR/extract_usage.sh"

# ==============================================================================
# 레이어 정의
# =============================================================================

# 레이어 번호 반환 (0-5, 0=알 수 없음)
get_layer_number() {
    local path="$1"

    if [[ "$path" =~ 00_common ]]; then
        echo "0"
    elif [[ "$path" =~ 01_app ]]; then
        echo "1"
    elif [[ "$path" =~ 02_presentation ]]; then
        echo "2"
    elif [[ "$path" =~ 03_service ]]; then
        echo "3"
    elif [[ "$path" =~ 04_driver ]]; then
        echo "4"
    elif [[ "$path" =~ 05_hal ]]; then
        echo "5"
    else
        echo "0"
    fi
}

# 레이어 이름 반환
get_layer_name() {
    local num="$1"

    case "$num" in
        0) echo "common" ;;
        1) echo "app" ;;
        2) echo "presentation" ;;
        3) echo "service" ;;
        4) echo "driver" ;;
        5) echo "hal" ;;
        *) echo "unknown" ;;
    esac
}

# ==============================================================================
# include 의존성 분석
# ==============================================================================

# 헤더 파일의 include 의존성 추출
extract_include_dependencies() {
    local file="$1"
    local file_layer
    local issues=0

    file_layer=$(get_layer_number "$file")

    # #include "..." 형태의 프로젝트 내부 include 추출
    grep -oP '#include\s+"[^"]+"' "$file" 2>/dev/null | while read -r include_line; do
        local header
        local header_path
        local header_layer

        header=$(echo "$include_line" | sed 's/#include "\(.*\)"/\1/')

        # 헤더 파일 경로 찾기
        header_path=$(find "$PROJECT_ROOT/components" -name "$header" 2>/dev/null | head -1)

        if [[ -z "$header_path" ]]; then
            header_path=$(find "$PROJECT_ROOT/include" -name "$header" 2>/dev/null | head -1)
        fi

        if [[ -n "$header_path" ]]; then
            header_layer=$(get_layer_number "$header_path")

            # 상위 레이어를 참조하는지 확인
            if [[ "$header_layer" -lt "$file_layer" ]] && [[ "$header_layer" -ne 0 ]]; then
                echo "❌ VIOLATION: $file (layer $file_layer) includes $header (layer $header_layer)"
                ((issues++))
            fi
        fi
    done

    return $issues
}

# ==============================================================================
# CMake REQUIRES 의존성 분석
# ==============================================================================

# CMakeLists.txt의 REQUIRES 의존성 분석
analyze_cmake_dependencies() {
    echo ""
    echo "[3] CMakeLists.txt REQUIRES 의존성 분석"
    echo "-----------------------------------"

    local total_violations=0

    find "$PROJECT_ROOT/components" -name "CMakeLists.txt" | sort | while read -r cmake_file; do
        local component
        local component_layer
        local component_dir

        component_dir=$(dirname "$cmake_file")
        component_layer=$(get_layer_number "$component_dir")
        component=$(basename "$component_dir")

        # REQUIRES 추출
        local requires
        requires=$(extract_cmake_requires "$cmake_file")

        if [[ -z "$requires" ]]; then
            continue
        fi

        # 각 의존성 확인
        echo "$requires" | tr ',' '\n' | while read -r dep; do
            dep=$(echo "$dep" | xargs)  # trim

            if [[ -z "$dep" ]]; then
                continue
            fi

            # 의존성 레이어 확인
            local dep_dir
            local dep_layer

            dep_dir=$(find "$PROJECT_ROOT/components" -type d -name "$dep" 2>/dev/null | head -1)

            if [[ -n "$dep_dir" ]]; then
                dep_layer=$(get_layer_number "$dep_dir")

                # 상위 레이어 의존성 확인
                if [[ "$dep_layer" -lt "$component_layer" ]] && [[ "$dep_layer" -ne 0 ]]; then
                    echo "❌ $component (L$component_layer) → $dep (L$dep_layer): 상위 레이어 의존"
                    ((total_violations++))
                elif [[ "$dep_layer" -eq "$component_layer" ]]; then
                    echo "⚠️  $component (L$component_layer) → $dep (L$dep_layer): 동일 레이어"
                else
                    echo "✅ $component (L$component_layer) → $dep (L$dep_layer)"
                fi
            fi
        done
    done

    echo ""
    echo "총 의존성 위반: $total_violations개"
}

# ==============================================================================
# 순환 의존성 검출
# ==============================================================================

# 순환 의존성 검출
detect_circular_dependencies() {
    echo ""
    echo "[4] 순환 의존성 검출"
    echo "-----------------------------------"

    # 간단한 A→B, B→A 패턴 검출
    local deps_file
    deps_file=$(mktemp)

    # 모든 의존성 수집
    find "$PROJECT_ROOT/components" -name "CMakeLists.txt" | while read -r cmake_file; do
        local component
        local component_dir

        component_dir=$(dirname "$cmake_file")
        component=$(basename "$component_dir")

        extract_cmake_requires "$cmake_file" | tr ',' '\n' | while read -r dep; do
            dep=$(echo "$dep" | xargs)
            if [[ -n "$dep" ]]; then
                echo "$component:$dep"
            fi
        done
    done > "$deps_file"

    # 순환 검출
    local found=0
    while read -r dep_line; do
        local from
        local to
        from=$(echo "$dep_line" | cut -d: -f1)
        to=$(echo "$dep_line" | cut -d: -f2)

        # 역방향 확인
        if grep -q "^$to:$from$" "$deps_file"; then
            echo "⚠️  순환 의존성: $from ↔ $to"
            found=1
        fi
    done < "$deps_file"

    rm -f "$deps_file"

    if [[ $found -eq 0 ]]; then
        echo "✅ 순환 의존성 없음"
    fi
}

# ==============================================================================
# 서비스-드라이버 API 구조체 일치성 검증
# ==============================================================================

# 서비스와 드라이버 간 API 구조체 일치 확인
verify_service_driver_structs() {
    echo ""
    echo "[5] 서비스 ↔ 드라이버 API 구조체 일치성"
    echo "-----------------------------------"

    local issues=0

    # lora_service → lora_driver
    check_api_struct_match \
        "components/03_service/lora_service" \
        "components/04_driver/lora_driver" \
        "lora"
    ((issues += $?))

    # hardware_service → battery_driver
    check_api_struct_match \
        "components/03_service/hardware_service" \
        "components/04_driver/battery_driver" \
        "battery"
    ((issues += $?))

    echo ""
    echo "발견된 문제: $issues개"
    return $issues
}

# 단일 서비스-드라이버 쌍의 구조체 일치 확인
check_api_struct_match() {
    local service_dir="$PROJECT_ROOT/$1"
    local driver_dir="$PROJECT_ROOT/$2"
    local prefix="$3"
    local issues=0

    local service_header
    local driver_header

    service_header=$(find "$service_dir/include" -name "${prefix}_*.h" 2>/dev/null | head -1)
    driver_header=$(find "$driver_dir/include" -name "${prefix}_*.h" 2>/dev/null | head -1)

    if [[ -z "$service_header" ]] || [[ -z "$driver_header" ]]; then
        return 0
    fi

    echo "$prefix:"

    # 서비스 구조체 추출
    local service_structs
    service_structs=$(grep -oP 'typedef\s+struct[^;]*'${prefix}'[^;]*_t;' "$service_header" 2>/dev/null | \
        grep -oP '\w+_t;' | tr -d ';' | sort -u)

    # 드라이버 구조체 추출
    local driver_structs
    driver_structs=$(grep -oP 'typedef\s+struct[^;]*'${prefix}'[^;]*_t;' "$driver_header" 2>/dev/null | \
        grep -oP '\w+_t;' | tr -d ';' | sort -u)

    # 공통 구조체 확인
    echo "$service_structs" | while read -r s_struct; do
        if echo "$driver_structs" | grep -q "^${s_struct}$"; then
            echo "  ✅ $s_struct: 공통 정의"
        else
            # 서비스 전용 구조체 (내부용)
            if [[ "$s_struct" =~ _service_ ]] || [[ "$s_struct" =~ _status_ ]]; then
                echo "  ℹ️  $s_struct: 서비스 내부용"
            else
                echo "  ⚠️  $s_struct: 서비스에만 정의"
            fi
        fi
    done

    return 0
}

# ==============================================================================
# 전체 레이어 검증
# ==============================================================================

run_layer_checks() {
    echo "============================================"
    echo "   레이어 의존성 검증"
    echo "============================================"
    echo ""

    # CMake 의존성 분석
    analyze_cmake_dependencies

    # 순환 의존성 검출
    detect_circular_dependencies

    # 서비스-드라이버 API 검증
    verify_service_driver_structs

    return 0
}

# ==============================================================================
# 함수 내보내기
# =============================================================================

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # 직접 실행 시 전체 검증
    run_layer_checks
fi
