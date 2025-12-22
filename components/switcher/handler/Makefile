# Switcher Handler Makefile
#
# 통합 스위처 핸들러 라이브러리

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
CFLAGS += -Iinclude
CFLAGS += -I../common/include
CFLAGS += -I../protocol/atem/include
CFLAGS += -I../protocol/vmix/include
CFLAGS += -I../protocol/obs/include

# 스위처 지원 활성화
CFLAGS += -DSWITCHER_ENABLE_ATEM
CFLAGS += -DSWITCHER_ENABLE_VMIX
CFLAGS += -DSWITCHER_ENABLE_OBS

# 디렉토리
SRC_DIR = src
BUILD_DIR = build
LIB_DIR = lib

# 소스 파일
SRCS = $(SRC_DIR)/switcher.c

# 오브젝트 파일
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

# 라이브러리
LIB_NAME = libswitcher.a
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
