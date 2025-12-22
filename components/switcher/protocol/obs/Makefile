# OBS WebSocket Client Makefile
#
# OBS Studio WebSocket 클라이언트 라이브러리 (obs-websocket v5.x)

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
CFLAGS += -Iinclude
CFLAGS += -Isrc
CFLAGS += -I../../common/include

# 디버그 레벨 (0: 없음, 1: 오류, 2: 정보, 3: 상세)
OBS_DEBUG_LEVEL ?= 0
CFLAGS += -DOBS_DEBUG_LEVEL=$(OBS_DEBUG_LEVEL)

# 디렉토리
SRC_DIR = src
BUILD_DIR = build
LIB_DIR = lib
TEST_DIR = tests

# 소스 파일
SRCS = $(SRC_DIR)/obs_client.c \
       $(SRC_DIR)/obs_websocket.c \
       $(SRC_DIR)/obs_json.c \
       $(SRC_DIR)/obs_sha256.c \
       $(SRC_DIR)/obs_base64.c

# 오브젝트 파일
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

# 라이브러리
LIB_NAME = libobs.a
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
	$(CC) $(CFLAGS) $(TEST_DEBUG_SRC) -L$(LIB_DIR) -L../../common/lib -lobs -lsw_common -o $(TEST_DEBUG_BIN)
	@echo "디버그 테스트 빌드 완료: $(TEST_DEBUG_BIN)"
	@echo ""
	@echo "사용법: $(TEST_DEBUG_BIN) <ip> [port] [password]"

# 디버그 빌드
debug: clean
	$(MAKE) all OBS_DEBUG_LEVEL=3
	$(MAKE) test OBS_DEBUG_LEVEL=3
	@echo "디버그 빌드 완료 (레벨=3)"

# 클린
clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR)

.PHONY: all test debug clean
