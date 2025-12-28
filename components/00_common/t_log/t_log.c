/**
 * @file t_log.c
 * @brief Tally Node 전용 로그 시스템 구현
 *
 * LogConfig.h의 설정을 사용
 */

#include "t_log.h"
#include <stdarg.h>
#include <string.h>

// ============================================================
// 로그 레벨 문자
// ============================================================

static const char LOG_LEVEL_CHARS[] = {'N', 'E', 'W', 'I', 'D', 'V'};

// ============================================================
// 현재 로그 레벨
// ============================================================

t_log_level_t g_t_log_level = T_LOG_DEFAULT_LEVEL;

// ============================================================
// 런타임 출력 설정
// ============================================================

static int g_timestamp_enabled = T_LOG_TIMESTAMP_ENABLE;
static int g_level_char_enabled = T_LOG_LEVEL_CHAR_ENABLE;

// ============================================================
// LogConfig.h 구현
// ============================================================

void t_log_set_level(int level) {
    if (level >= T_LOG_NONE && level < T_LOG_MAX) {
        g_t_log_level = (t_log_level_t)level;
    }
}

int t_log_get_level(void) {
    return (int)g_t_log_level;
}

void t_log_set_timestamp(int enable) {
    g_timestamp_enabled = (enable != 0) ? 1 : 0;
}

int t_log_get_timestamp(void) {
    return g_timestamp_enabled;
}

void t_log_set_level_char(int enable) {
    g_level_char_enabled = (enable != 0) ? 1 : 0;
}

int t_log_get_level_char(void) {
    return g_level_char_enabled;
}

// ============================================================
// 타임스탬프
// ============================================================

uint32_t t_log_timestamp(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

// ============================================================
// 로그 출력 (스레드 안전, 재진입 가능)
// ============================================================

void t_log_output(t_log_level_t level, const char* tag, const char* fmt, ...) {
    if (level > g_t_log_level || level >= T_LOG_MAX) {
        return;
    }

    // 출력 버퍼 (지역 변수 - 재진입 가능)
    char log_buf[T_LOG_BUFFER_SIZE];
    int len = 0;

    // 로그 레벨 문자 (선택적)
    if (g_level_char_enabled) {
        char level_char = LOG_LEVEL_CHARS[level];
        len += snprintf(log_buf + len, sizeof(log_buf) - len,
                       "%c ", level_char);
    }

    // 타임스탬프 (선택적)
    if (g_timestamp_enabled) {
        uint32_t ts = t_log_timestamp();
        len += snprintf(log_buf + len, sizeof(log_buf) - len,
                       "(%lu) ", ts);
    }

    // 태그 처리 (한 번에 포맷, 버퍼 오버플로우 방지)
    char tag_buf[T_LOG_TAG_MAX_LEN + 2];  // [TAG]... + null

    if (tag == NULL) {
        tag = "?";
    }

    size_t tag_len = strlen(tag);
    size_t avail = T_LOG_TAG_MAX_LEN - 2;  // 괄호 2개 제외

    if (tag_len <= avail) {
        // 짧은 태그: [TAG] + 공백으로 정렬
        snprintf(tag_buf, sizeof(tag_buf),
                 "[%s]%*s", tag, (int)(avail - tag_len), "");
    } else {
        // 긴 태그: [LONGT...]
        snprintf(tag_buf, sizeof(tag_buf),
                 "[%.*s...]", (int)(avail - 3), tag);
    }

    // 태그 추가
    len += snprintf(log_buf + len, sizeof(log_buf) - len,
                   "%s ", tag_buf);

    if (len < 0 || len >= (int)sizeof(log_buf)) {
        return;  // 버퍼 부족
    }

    // 가변 인자 처리
    va_list args;
    va_start(args, fmt);

    // fmt null 체크
    if (fmt != NULL) {
        vsnprintf(log_buf + len, sizeof(log_buf) - len, fmt, args);
    } else {
        snprintf(log_buf + len, sizeof(log_buf) - len, "(null fmt)");
    }

    va_end(args);

    // 출력 (개행 보장)
    size_t log_len = strlen(log_buf);
    if (log_len > 0 && log_buf[log_len - 1] != '\n') {
        log_buf[log_len] = '\n';
        log_buf[log_len + 1] = '\0';
    }

    // 출력
    fputs(log_buf, stdout);
    fflush(stdout);
}
