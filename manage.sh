#!/bin/bash

# ==============================================================================
# Tally-Node Project Manager Script
# Author: Claude AI Assistant
# Date: 2025-12-23
#
# Description:
# This script provides a menu to manage the Tally-Node project.
# 1. Pulls/updates project from remote server.
# 2. Builds and uploads TX environment (no clean).
# 3. Builds and uploads RX environment (no clean).
# 4. Clean and build TX/RX (no upload).
# 5. Device monitor.
# 6. Exits the script.
# ==============================================================================

# --- 설정 변수 (Configuration Variables) ---
# 원격 서버 정보
REMOTE_IP="whatumong-server.gobongs.com"
REMOTE_PORT="17140"
REMOTE_USER="root"
REMOTE_PASSWORD="Tjsfyddl1**!@"  # ⚠️ 비밀번호 하드코딩 - 보안을 위해 사용 후 변경하세요
# 스크립트가 있는 현재 폴더를 프로젝트 폴더로 사용
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REMOTE_PATH="/home/prod/tally-node"

# PlatformIO 환경 설정
ENV_TX="eora_s3_tx"
ENV_RX="eora_s3_rx"

# 가상환경 설정
VENV_DIR="venv"

# 포트 설정
PORT_AUTO="/dev/ttyACM0"
PORT_SPEED="921600"

# 스크립트 이름
SCRIPT_NAME="manage.sh"

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# --- 도우미 함수 (Helper Functions) ---

# 로그 출력 함수
log_info() {
    echo -e "${CYAN}>>> $1${NC}"
}

log_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

log_error() {
    echo -e "${RED}✗ $1${NC}"
}

# 메뉴 구분선 출력
print_separator() {
    echo "------------------------------------------------------"
}

# --- 동기화 함수 ---

# expect를 사용한 rsync 실행 함수
run_rsync_with_expect() {
    local rsync_command="$1"

    # expect가 설치되어 있는지 확인
    if ! command -v expect &> /dev/null; then
        log_error "'expect'가 설치되지 않았습니다."
        log_info "설치를 위해 아래 명령어를 실행하세요:"
        echo "sudo apt-get install expect"
        return 1
    fi

    log_info "rsync를 실행합니다..."
    expect << EOF
spawn sh -c "$rsync_command"
expect {
    "password:" {
        send "$REMOTE_PASSWORD\r"
        exp_continue
    }
    "yes/no" {
        send "yes\r"
        exp_continue
    }
    eof
}
EOF

    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        return 0
    else
        return 1
    fi
}

# 원격 서버에서 프로젝트 다운로드
sync_from_remote() {
    log_info "원격 서버에서 프로젝트를 가져옵니다..."

    # 다운로드 제외 목록
    EXCLUDE_LIST=(
        --exclude='.pio'
        --exclude='examples'
        --exclude='*.pyc'
        --exclude='__pycache__'
        --exclude='.DS_Store'
        --exclude='*.log'
    )

    # rsync 명령어 구성 (--delete 추가: 원격에 없는 로컬 파일 삭제)
    RSYNC_CMD="rsync -avz --delete --progress ${EXCLUDE_LIST[@]} -e 'ssh -p $REMOTE_PORT -o StrictHostKeyChecking=no' $REMOTE_USER@$REMOTE_IP:$REMOTE_PATH/ $PROJECT_DIR/"

    # expect를 사용하여 rsync 실행
    if run_rsync_with_expect "$RSYNC_CMD"; then
        log_success "프로젝트를 가져왔습니다."
    else
        log_error "가져오기 중 오류가 발생했습니다."
        return 1
    fi
}

# --- PlatformIO 실행 함수 ---

# 가상환경 확인 및 설치
check_venv() {
    if [ ! -d "$PROJECT_DIR/$VENV_DIR" ]; then
        log_warning "가상환경이 없습니다. 생성합니다..."
        python3 -m venv "$VENV_DIR"
        source "$VENV_DIR/bin/activate"
        pip install -U platformio
        log_success "가상환경을 생성하고 PlatformIO를 설치했습니다."
    else
        log_info "가상환경을 활성화합니다..."
        source "$VENV_DIR/bin/activate"
    fi
}

# 클린 후 빌드 (업로드 안함)
clean_build_only() {
    log_info "빌드 캐시를 정리하고 $ENV_TX/$ENV_RX를 빌드합니다..."

    # 프로젝트 폴더로 이동
    cd "$PROJECT_DIR" || exit 1

    # 가상환경 확인 및 설치
    if [ ! -d "$PROJECT_DIR/$VENV_DIR" ]; then
        log_warning "가상환경이 없습니다. 생성합니다..."
        python3 -m venv "$VENV_DIR"
        source "$PROJECT_DIR/$VENV_DIR/bin/activate"
        pip install -U platformio
        log_success "가상환경을 생성하고 PlatformIO를 설치했습니다."
    else
        source "$PROJECT_DIR/$VENV_DIR/bin/activate"
    fi

    # 빌드 캐시 정리
    rm -rf .pio/

    # TX 빌드
    log_info "$ENV_TX 환경 빌드 중..."
    pio run -e "$ENV_TX"

    if [ $? -eq 0 ]; then
        log_success "$ENV_TX 빌드가 완료되었습니다."
    else
        log_error "$ENV_TX 빌드에 실패했습니다."
        return 1
    fi

    # RX 빌드
    log_info "$ENV_RX 환경 빌드 중..."
    pio run -e "$ENV_RX"

    if [ $? -eq 0 ]; then
        log_success "$ENV_RX 빌드가 완료되었습니다."
        log_success "모든 빌드가 성공적으로 완료되었습니다."
    else
        log_error "$ENV_RX 빌드에 실패했습니다."
        return 1
    fi
}

# 빌드 및 업로드
build_upload() {
    local env=$1
    local env_name=$2

    log_info "$env_name 환경 빌드 및 업로드를 시작합니다..."

    # 프로젝트 폴더로 이동
    cd "$PROJECT_DIR" || exit 1

    # 가상환경 확인 및 설치
    if [ ! -d "$PROJECT_DIR/$VENV_DIR" ]; then
        log_warning "가상환경이 없습니다. 생성합니다..."
        python3 -m venv "$VENV_DIR"
        source "$PROJECT_DIR/$VENV_DIR/bin/activate"
        pip install -U platformio
        log_success "가상환경을 생성하고 PlatformIO를 설치했습니다."
    else
        source "$PROJECT_DIR/$VENV_DIR/bin/activate"
    fi

    # 포트 점유 해제 후 빌드 및 업로드
    log_info "포트 점유 해체..."
    fuser -k $PORT_AUTO 2>/dev/null; sleep 1

    # 빌드 및 업로드
    pio run -e "$env" -t upload

    if [ $? -eq 0 ]; then
        log_success "$env_name 빌드 및 업로드가 완료되었습니다."
        log_info "3초 후 디바이스 모니터를 시작합니다... (종료: Ctrl+A, K)"
        sleep 3
        device_monitor
    else
        log_error "$env_name 빌드 또는 업로드에 실패했습니다."
    fi
}

# 디바이스 모니터
device_monitor() {
    log_info "디바이스 모니터를 시작합니다... (종료: Ctrl+A, K)"

    cd "$PROJECT_DIR" || exit 1

    # 가상환경 확인
    if [ ! -d "$PROJECT_DIR/$VENV_DIR" ]; then
        log_error "가상환경이 없습니다. 먼저 빌드를 실행하세요."
        return 1
    fi

    # 서브셸에서 가상환경 활성화 후 pio 실행
    (
        source "$PROJECT_DIR/$VENV_DIR/bin/activate"
        pio device monitor
    )
}


# 빌드 캐시 정리
clean_build() {
    log_info "빌드 캐시를 정리합니다..."

    cd "$PROJECT_DIR" || exit 1
    check_venv

    # .pio 디렉토리 완전 삭제
    rm -rf .pio/

    log_success "모든 빌드 캐시를 정리했습니다."
}

# --- 메인 메뉴 함수 ---
show_menu() {
    clear
    echo "======================================================"
    echo "           Tally-Node Project Manager v2.5          "
    echo "======================================================"
    echo "  프로젝트 경로: $PROJECT_DIR"
    echo "  원격 경로: $REMOTE_USER@$REMOTE_IP:$REMOTE_PATH"
    echo "------------------------------------------------------"
    echo "  1. 원격 서버에서 프로젝트 가져오기 (Pull)"
    echo "  2. 디바이스 모니터 시작"
    echo "  3. TX 환경 빌드 및 업로드 (클린 안함)"
    echo "  4. RX 환경 빌드 및 업로드 (클린 안함)"
    echo "  5. 클린 후 TX/RX 빌드 (업로드 안함)"
    echo "  6. 종료"
    echo "======================================================"
}

# --- 메인 루프 ---
main() {
    # 스크립트 실행 권한 확인
    if [ ! -x "$0" ]; then
        log_warning "스크립트에 실행 권한이 없습니다. 권한을 추가합니다..."
        chmod +x "$0"
        log_success "실행 권한을 추가했습니다. 다시 실행해주세요."
        exit 1
    fi

    # 메뉴 루프
    while true; do
        show_menu
        echo ""
        read -p "원하는 작업을 선택하세요 (1-6): " choice
        echo ""

        case $choice in
            1)
                sync_from_remote
                print_separator
                read -p "엔터를 누르면 메뉴로 돌아갑니다..."
                ;;
            2)
                device_monitor
                print_separator
                read -p "엔터를 누르면 메뉴로 돌아갑니다..."
                ;;
            3)
                build_upload "$ENV_TX" "TX"
                print_separator
                read -p "엔터를 누르면 메뉴로 돌아갑니다..."
                ;;
            4)
                build_upload "$ENV_RX" "RX"
                print_separator
                read -p "엔터를 누르면 메뉴로 돌아갑니다..."
                ;;
            5)
                clean_build_only
                print_separator
                read -p "엔터를 누르면 메뉴로 돌아갑니다..."
                ;;
            6)
                log_info "스크립트를 종료합니다."
                exit 0
                ;;
            *)
                log_error "잘못된 입력입니다. 1-6 중에서 선택하세요."
                print_separator
                read -p "엔터를 누르면 메뉴로 돌아갑니다..."
                ;;
        esac
    done
}

# 스크립트 시작
main