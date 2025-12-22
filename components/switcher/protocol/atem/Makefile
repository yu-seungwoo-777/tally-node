# atem Makefile
#
# ATEM UDP 라이브러리 (Blackmagic Design ATEM Switcher)

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
CFLAGS += -Iinclude

# 디버그 레벨 (0=없음, 1=에러, 2=경고, 3=정보, 4=상세)
DEBUG_LEVEL ?= 0
# 패킷 덤프 (0=비활성화, 1=활성화)
DEBUG_PACKET ?= 0

CFLAGS += -DATEM_DEBUG_LEVEL=$(DEBUG_LEVEL)
CFLAGS += -DATEM_DEBUG_PACKET=$(DEBUG_PACKET)

# 디렉토리
SRC_DIR = src
PLATFORM_DIR = platform
BUILD_DIR = build
LIB_DIR = lib
TEST_DIR = tests

# 소스 파일
SRCS = $(SRC_DIR)/atem_parser.c \
       $(SRC_DIR)/atem_client.c \
       $(SRC_DIR)/atem_debug.c \
       $(PLATFORM_DIR)/atem_platform_linux.c

# 오브젝트 파일
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

# 라이브러리
LIB_NAME = libatem.a
LIB_PATH = $(LIB_DIR)/$(LIB_NAME)

# 디버그 테스트
TEST_DEBUG_SRC = $(TEST_DIR)/test_debug.c
TEST_DEBUG_BIN = $(BUILD_DIR)/test_debug

# 기본 타겟
all: $(LIB_PATH)

# 정적 라이브러리 생성
$(LIB_PATH): $(OBJS)
	@mkdir -p $(LIB_DIR)
	$(AR) rcs $@ $^
	@echo "라이브러리 생성 완료: $@"

# 오브젝트 파일 컴파일
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 디버그 테스트 빌드
test: $(LIB_PATH) $(TEST_DEBUG_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_DEBUG_SRC) -L$(LIB_DIR) -latem -o $(TEST_DEBUG_BIN)
	@echo "디버그 테스트 빌드 완료: $(TEST_DEBUG_BIN)"
	@echo ""
	@echo "사용법: $(TEST_DEBUG_BIN) <ip> [port]"

# 클린
clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR)

# 디버그 빌드 (레벨 4 + 패킷 덤프)
debug: clean
	$(MAKE) all DEBUG_LEVEL=4 DEBUG_PACKET=1
	$(MAKE) test DEBUG_LEVEL=4 DEBUG_PACKET=1
	@echo "디버그 빌드 완료 (레벨=4, 패킷덤프=ON)"

.PHONY: all test clean debug
