# vmix Makefile
#
# vMix TCP API 라이브러리

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
CFLAGS += -Iinclude
CFLAGS += -I../../common/include

# 디버그 레벨 (0=없음, 1=에러, 2=경고, 3=정보, 4=상세)
DEBUG_LEVEL ?= 0

CFLAGS += -DVMIX_DEBUG_LEVEL=$(DEBUG_LEVEL)

# 디렉토리
SRC_DIR = src
BUILD_DIR = build
LIB_DIR = lib
TEST_DIR = tests

# 소스 파일
SRCS = $(SRC_DIR)/vmix_client.c

# 오브젝트 파일
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

# 라이브러리
LIB_NAME = libvmix.a
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
	$(CC) $(CFLAGS) $(TEST_DEBUG_SRC) -L$(LIB_DIR) -L../../common/lib -lvmix -lsw_common -o $(TEST_DEBUG_BIN)
	@echo "디버그 테스트 빌드 완료: $(TEST_DEBUG_BIN)"
	@echo ""
	@echo "사용법: $(TEST_DEBUG_BIN) <ip> [port]"

# 클린
clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR)

# 디버그 빌드 (레벨 4)
debug: clean
	$(MAKE) all DEBUG_LEVEL=4
	$(MAKE) test DEBUG_LEVEL=4
	@echo "디버그 빌드 완료 (레벨=4)"

.PHONY: all test clean debug
