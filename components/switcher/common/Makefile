# Common Platform Makefile
#
# 공통 플랫폼 추상화 레이어

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
CFLAGS += -Iinclude

# 디렉토리
PLATFORM_DIR = platform
BUILD_DIR = build
LIB_DIR = lib

# 소스 파일
SRCS = $(PLATFORM_DIR)/sw_platform_linux.c

# 오브젝트 파일
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

# 라이브러리
LIB_NAME = libsw_common.a
LIB_PATH = $(LIB_DIR)/$(LIB_NAME)

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

# 클린
clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR)

.PHONY: all clean
