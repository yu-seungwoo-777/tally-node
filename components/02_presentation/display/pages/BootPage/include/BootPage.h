/**
 * @file BootPage.h
 * @brief 부팅 화면 페이지
 */

#ifndef BOOT_PAGE_H
#define BOOT_PAGE_H

#include "DisplayManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BootPage 초기화 및 등록
 * @return true 성공, false 실패
 */
bool boot_page_init(void);

/**
 * @brief 부팅 메시지 설정
 * @param message 표시할 메시지
 */
void boot_page_set_message(const char* message);

/**
 * @brief 진행률 설정
 * @param progress 진행률 (0-100)
 */
void boot_page_set_progress(uint8_t progress);

#ifdef __cplusplus
}
#endif

#endif // BOOT_PAGE_H
