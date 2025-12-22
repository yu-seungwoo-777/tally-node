#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// 전역 로그 레벨
static log_level_t g_log_level = LOG_LEVEL_0;

// 태그 포맷팅 설정
#define MAX_TAG_LENGTH 10       // 최대 태그 표시 길이
#define TAG_FIELD_WIDTH 12      // 전체 태그 필드 너비 ([태그] + 공백)

// 로그 시스템 초기화
void log_init(log_level_t default_level) {
    g_log_level = default_level;
    // UART 초기화는 ESP-IDF가 자동으로 처리
    // 초기화 메시지 제거 (불필요)
}

// 로그 레벨 설정
void log_set_level(log_level_t level) {
    if (level == LOG_LEVEL_0 || level == LOG_LEVEL_1) {
        g_log_level = level;
    }
}

// 로그 레벨 가져오기
log_level_t log_get_level(void) {
    return g_log_level;
}

// 태그 포맷팅 함수
// 태그를 최대 MAX_TAG_LENGTH 길이로 제한하고, 메시지 시작 위치를 고정
static void print_formatted_tag(const char *tag) {
    int tag_len = strlen(tag);

    if (tag_len > MAX_TAG_LENGTH) {
        // 태그가 최대 길이를 초과하면 "...Tagnam" 형태로 생략
        int overflow = tag_len - MAX_TAG_LENGTH;
        printf("[");
        for (int i = 0; i < MAX_TAG_LENGTH - 2; i++) {
            printf("%c", tag[overflow + i]);
        }
        printf("..]");
        // 나머지 공백 출력
        for (int i = 0; i < TAG_FIELD_WIDTH - MAX_TAG_LENGTH - 2; i++) {
            printf(" ");
        }
    } else {
        // 태그 출력: [태그]
        printf("[%s]", tag);
        // 메시지 시작 위치를 맞추기 위한 공백 출력
        int spaces_needed = TAG_FIELD_WIDTH - tag_len - 2; // -2는 [ ]
        for (int i = 0; i < spaces_needed; i++) {
            printf(" ");
        }
    }
}

// 로그 출력 (레벨 0 - 기본)
void log_0(const char *tag, const char *format, ...) {
    // 포맷팅된 태그 출력
    print_formatted_tag(tag);

    // 포맷 문자열과 가변 인자 출력
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

// 로그 출력 (레벨 1 - 상세)
void log_1(const char *tag, const char *format, ...) {
    // 현재 로그 레벨이 1 이상일 때만 출력
    if (g_log_level < LOG_LEVEL_1) {
        return;
    }

    // 포맷팅된 태그 출력
    print_formatted_tag(tag);

    // 포맷 문자열과 가변 인자 출력
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

// 로그 버퍼 비우기
void log_flush(void) {
    fflush(stdout);
}
