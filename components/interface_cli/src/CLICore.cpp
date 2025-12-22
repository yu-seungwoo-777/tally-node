/**
 * @file CLICore.cpp
 * @brief CLI Core 구현
 */

#include "CLICore.h"
#include "log.h"
#include "log_tags.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"

static const char* TAG = TAG_CLI;

// 정적 멤버 초기화
bool CLICore::s_initialized = false;

esp_err_t CLICore::init()
{
    if (s_initialized) {
        LOG_0(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    // USB CDC 또는 UART 초기화
    // ESP32-S3는 USB CDC를 사용하므로 별도 초기화 불필요
    // (USB CDC는 자동으로 /dev/cdcacm0으로 마운트됨)

    // esp_console 설정
    esp_console_config_t console_config = {};
    console_config.max_cmdline_length = 256;
    console_config.max_cmdline_args = 32;
#if CONFIG_LOG_COLORS
    console_config.hint_color = atoi(LOG_COLOR_CYAN);
#endif

    esp_err_t ret = esp_console_init(&console_config);
    if (ret != ESP_OK) {
        LOG_0(TAG, "esp_console 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // linenoise 설정
    linenoiseSetMultiLine(1);
    linenoiseSetCompletionCallback(nullptr);
    linenoiseSetHintsCallback(nullptr);
    linenoiseHistorySetMaxLen(100);

    s_initialized = true;
    return ESP_OK;
}

esp_err_t CLICore::registerCommand(const esp_console_cmd_t* cmd)
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_console_cmd_register(cmd);
    if (ret != ESP_OK) {
        LOG_0(TAG, "명령어 등록 실패 (%s): %s", cmd->command, esp_err_to_name(ret));
        return ret;
    }

    LOG_1(TAG, "명령어 등록: %s", cmd->command);
    return ESP_OK;
}

esp_err_t CLICore::startREPL()
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return ESP_FAIL;
    }

    LOG_0(TAG, "REPL 시작 (Ctrl+] 로 종료)");

    const char* prompt = "eora> ";
    printf("\n"
           "=====================================\n"
           "  EoRa-S3 CLI\n"
           "=====================================\n"
           "  'help' 명령어를 입력하세요\n"
           "\n");

    char* line;
    while ((line = linenoise(prompt)) != NULL) {
        // 빈 줄은 무시
        if (strlen(line) == 0) {
            linenoiseFree(line);
            continue;
        }

        // 히스토리에 추가
        linenoiseHistoryAdd(line);

        // 명령어 실행
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("명령어를 찾을 수 없습니다: %s\n", line);
        } else if (err == ESP_ERR_INVALID_ARG) {
            printf("명령어 인자가 잘못되었습니다\n");
        } else if (err != ESP_OK) {
            printf("명령어 실행 실패: %s\n", esp_err_to_name(err));
        }

        linenoiseFree(line);
    }

    LOG_0(TAG, "REPL 종료");
    return ESP_OK;
}

esp_err_t CLICore::runCommand(const char* cmdline)
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return ESP_FAIL;
    }

    int ret;
    esp_err_t err = esp_console_run(cmdline, &ret);
    if (err != ESP_OK) {
        LOG_0(TAG, "명령어 실행 실패 (%s): %s", cmdline, esp_err_to_name(err));
    }

    return err;
}
