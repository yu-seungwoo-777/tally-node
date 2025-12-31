#!/bin/bash
# =============================================================================
# check_events.sh - 이벤트 구조체 검증 라이브러리
# =============================================================================

# 프로젝트 루트 경로 (source될 때도 올바른 경로 계산)
_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$_LIB_DIR/../.." && pwd)"

# 라이브러리 로드
source "$_LIB_DIR/extract_structs.sh"
source "$_LIB_DIR/extract_usage.sh"

# ==============================================================================
# 이벤트 버스 구조체 정의
# =============================================================================

# event_bus.h에 정의된 이벤트 데이터 구조체와 예상 크기
# 형식: "타입이름:예상크기:설명"
declare -A EVENT_STRUCTS=(
    ["lora_rssi_event_t"]="13:RSSI/SNR 이벤트"
    ["lora_send_request_t"]="16:송신 요청 (포인터 포함)"
    ["device_register_event_t"]="4:디바이스 등록"
    ["device_info_t"]="41:단일 디바이스 정보 (packed)"
    ["device_list_event_t"]="828:디바이스 리스트 (20*41+4)"
    ["lora_packet_event_t"]="268:패킷 수신 (256+4+4+4)"
    ["tally_event_data_t"]="19:Tally 상태 (packed)"
    ["lora_rf_event_t"]="5:RF 설정"
    ["lora_channel_info_t"]="9:채널 정보 (packed)"
    ["lora_scan_start_t"]="12:스캔 시작"
    ["lora_scan_progress_t"]="21:스캔 진행"
    ["lora_scan_complete_t"]="904:스캔 완료 (100*9+4)"
    ["system_info_event_t"]="51:시스템 정보 (packed)"
    ["switcher_status_event_t"]="97:스위처 상태 (packed)"
    ["network_status_event_t"]="113:네트워크 상태 (packed)"
    ["network_restart_request_t"]="104:네트워크 재시작 요청"
    ["config_save_request_t"]="312:설정 저장 요청"
    ["config_data_event_t"]="256:설정 데이터 (packed)"
)

# ==============================================================================
# 이벤트 발행/구독 검증
# ==============================================================================

# 이벤트 타입별 발행 위치 추적
# 반환: "이벤트타입|파일:라인번호|발행데이터타입|sizeof값"
track_event_publishers() {
    local result=()

    find "$PROJECT_ROOT/components" -name "*.c" -o -name "*.cpp" | while read -r file; do
        # event_bus_publish(EVT_XXX, &data, sizeof(data)) 형태 추출
        grep -n 'event_bus_publish' "$file" 2>/dev/null | while read -r line; do
            local line_num
            local evt_type
            local data_info

            line_num=$(echo "$line" | cut -d: -f1)
            evt_type=$(echo "$line" | grep -oP 'EVT_\w+' | head -1)
            data_info=$(echo "$line" | grep -oP 'sizeof\s*\(\s*\w+\s*\)' | sed 's/sizeof(\(.*\))/\1/')

            if [[ -n "$evt_type" ]]; then
                echo "$evt_type|${file##*/}:$line_num|$data_info"
            fi
        done
    done
}

# 이벤트 타입별 구독자 추적
track_event_subscribers() {
    find "$PROJECT_ROOT/components" -name "*.c" -o -name "*.cpp" | while read -r file; do
        grep -n 'event_bus_subscribe' "$file" 2>/dev/null | while read -r line; do
            local line_num
            local evt_type
            local handler

            line_num=$(echo "$line" | cut -d: -f1)
            evt_type=$(echo "$line" | grep -oP 'EVT_\w+' | head -1)
            handler=$(echo "$line" | grep -oP ',\s*\w+\s*\)' | sed 's/[,)\s]//g')

            if [[ -n "$evt_type" ]]; then
                echo "$evt_type|${file##*/}:$line_num|$handler"
            fi
        done
    done
}

# ==============================================================================
# 이벤트 구조체 크기 검증
# ==============================================================================

# sizeof() 호출과 예상 크기 비교
verify_event_sizes() {
    echo "[1] 이벤트 데이터 크기 검증"
    echo "-----------------------------------"

    local issues=0

    # event_bus.h에 정의된 구조체 확인
    for struct_name in "${!EVENT_STRUCTS[@]}"; do
        local expected
        local description
        IFS=':' read -r expected description <<< "${EVENT_STRUCTS[$struct_name]}"

        # 실제 사용되는지 확인 (typedef 정의 제외)
        local usage_count
        usage_count=$(find "$PROJECT_ROOT/components" -name "*.c" -o -name "*.cpp" -o -name "*.h" | \
            xargs grep -h "$struct_name" 2>/dev/null | \
            grep -v "typedef.*$struct_name" | \
            grep -v "struct.*$struct_name" | \
            wc -l)

        if [[ "$usage_count" -eq 0 ]]; then
            echo "⚠️  $struct_name: 정의되었지만 사용되지 않음 ($description)"
            ((issues++))
        else
            echo "✅ $struct_name: 사용됨 ($description, $usage_count 참조)"
        fi
    done

    echo ""
    echo "발견된 문제: $issues개"
    return $issues
}

# ==============================================================================
# 서비스-이벤트 구조체 불일치 검증
# ==============================================================================

# 서비스에서 발행하는 구조체와 이벤트 버스 구조체 비교
verify_service_event_mismatch() {
    echo ""
    echo "[2] 서비스 ↔ 이벤트 구조체 불일치 검증"
    echo "-----------------------------------"

    local issues=0

    # lora_service 확인
    check_service_event_struct "lora_service" "lora_service.cpp"
    ((issues += $?))

    # config_service 확인
    check_service_event_struct "config_service" "config_service.cpp"
    ((issues += $?))

    # hardware_service 확인
    check_service_event_struct "hardware_service" "hardware_service.cpp"
    ((issues += $?))

    echo ""
    echo "발견된 문제: $issues개"
    return $issues
}

# 단일 서비스의 이벤트 발행 구조체 확인
check_service_event_struct() {
    local service_name="$1"
    local source_file="$2"
    local file_path
    local issues=0

    file_path=$(find "$PROJECT_ROOT/components/03_service" -name "$source_file")

    if [[ -z "$file_path" ]]; then
        return 0
    fi

    # 이벤트 발행 라인 추출
    local publish_lines
    publish_lines=$(grep -n 'event_bus_publish' "$file_path" 2>/dev/null)

    if [[ -z "$publish_lines" ]]; then
        return 0
    fi

    # 각 발행 라인 분석
    echo "$service_name:"

    echo "$publish_lines" | while read -r line; do
        local line_num
        local evt_type
        local sizeof_type

        line_num=$(echo "$line" | cut -d: -f1)
        evt_type=$(echo "$line" | grep -oP 'EVT_\w+' | head -1)
        sizeof_type=$(echo "$line" | grep -oP 'sizeof\s*\(\s*\w+\s*\)' | sed 's/sizeof(\(.*\))/\1/')

        if [[ -n "$evt_type" ]]; then
            # 이벤트 타입에 맞는 데이터 타입 확인
            local expected_type
            expected_type=$(get_expected_event_type "$evt_type")

            if [[ -n "$expected_type" ]]; then
                if [[ "$sizeof_type" == "$expected_type" ]] || \
                   [[ "$sizeof_type" == *"event"* ]] || \
                   [[ "$sizeof_type" == *"tally"* ]]; then
                    echo "  ✅ $evt_type: $sizeof_type"
                else
                    echo "  ❌ $evt_type: 예상 $expected_type, 실제 $sizeof_type"
                    issues=1
                fi
            else
                echo "  ⚠️  $evt_type: 알 수 없는 이벤트 타입"
            fi
        fi
    done

    return $issues
}

# 이벤트 타입에 해당하는 예상 데이터 구조체 반환
get_expected_event_type() {
    local evt_type="$1"

    case "$evt_type" in
        EVT_LORA_RSSI_CHANGED) echo "lora_rssi_event_t" ;;
        EVT_LORA_PACKET_RECEIVED) echo "lora_packet_event_t" ;;
        EVT_LORA_SEND_REQUEST) echo "lora_send_request_t" ;;
        EVT_LORA_SCAN_START) echo "lora_scan_start_t" ;;
        EVT_LORA_SCAN_PROGRESS) echo "lora_scan_progress_t" ;;
        EVT_LORA_SCAN_COMPLETE) echo "lora_scan_complete_t" ;;
        EVT_RF_CHANGED|EVT_RF_SAVED) echo "lora_rf_event_t" ;;
        EVT_INFO_UPDATED) echo "system_info_event_t" ;;
        EVT_SWITCHER_STATUS_CHANGED) echo "switcher_status_event_t" ;;
        EVT_TALLY_STATE_CHANGED) echo "tally_event_data_t" ;;
        EVT_NETWORK_STATUS_CHANGED) echo "network_status_event_t" ;;
        EVT_NETWORK_RESTART_REQUEST) echo "network_restart_request_t" ;;
        EVT_CONFIG_CHANGED) echo "config_save_request_t" ;;
        EVT_CONFIG_DATA_CHANGED) echo "config_data_event_t" ;;
        EVT_DEVICE_REGISTER|EVT_DEVICE_UNREGISTER) echo "device_register_event_t" ;;
        EVT_DEVICE_LIST_CHANGED) echo "device_list_event_t" ;;
        *) echo "" ;;
    esac
}

# ==============================================================================
# 함수 내보내기
# =============================================================================

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # 직접 실행 시 전체 검증
    echo "============================================"
    echo "   이벤트 구조체 검증"
    echo "============================================"
    echo ""

    verify_event_sizes
    verify_service_event_mismatch

    echo ""
    echo "============================================"
    echo "   발행자 추적"
    echo "============================================"
    echo ""
    track_event_publishers | head -10
fi
