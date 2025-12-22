/**
 * @file CLICore.h
 * @brief CLI (Command Line Interface) Core API
 *
 * Core API 원칙:
 * - 하드웨어 추상화 (USB CDC / UART)
 * - 상태 최소화 (esp_console 래퍼)
 * - 단일 책임 (CLI 명령어 실행)
 */

#pragma once

#include "esp_err.h"
#include "esp_console.h"

/**
 * @brief CLI Core API
 *
 * 설계 원칙:
 * - 상태: esp_console 내부 상태만 사용
 * - 스레드 안전성: esp_console이 보장
 * - 성능: Cold Path (사용자 입력 대기)
 */
class CLICore {
public:
    /**
     * @brief 초기화
     *
     * USB CDC 또는 UART를 통한 시리얼 콘솔을 초기화합니다.
     */
    static esp_err_t init();

    /**
     * @brief 명령어 등록
     *
     * @param cmd esp_console_cmd_t 구조체
     */
    static esp_err_t registerCommand(const esp_console_cmd_t* cmd);

    /**
     * @brief REPL (Read-Eval-Print Loop) 시작
     *
     * 이 함수는 blocking이며, Ctrl+] 로 종료 가능합니다.
     */
    static esp_err_t startREPL();

    /**
     * @brief 명령어 1줄 실행
     *
     * @param cmdline 명령어 라인 (예: "help")
     */
    static esp_err_t runCommand(const char* cmdline);

private:
    // 싱글톤 패턴
    CLICore() = delete;
    ~CLICore() = delete;
    CLICore(const CLICore&) = delete;
    CLICore& operator=(const CLICore&) = delete;

    // 상태 변수
    static bool s_initialized;
};
