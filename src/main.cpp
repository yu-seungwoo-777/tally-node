/**
 * @file main.cpp
 * @brief Tally Node 메인
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "t_log.h"

// 앱 헤더
#ifdef DEVICE_MODE_TX
#include "prod_tx_app.h"
#else
#include "prod_rx_app.h"
#endif

static const char* TAG = "main";

extern "C" void app_main(void)
{
#ifdef DEVICE_MODE_TX
    if (!prod_tx_app_init(NULL)) {
        T_LOGE(TAG, "TX app init failed");
        return;
    }
    prod_tx_app_start();

    // 각 서비스가 자체 태스크에서 실행되므로 메인 루프 없음
    // 메인 태스크 정지 (삭제하지 않고 일시 중지)
    vTaskSuspend(NULL);

    prod_tx_app_deinit();
#else
    if (!prod_rx_app_init(NULL)) {
        T_LOGE(TAG, "RX app init failed");
        return;
    }
    prod_rx_app_start();

    // 각 서비스가 자체 태스크에서 실행되므로 메인 루프 없음
    // 메인 태스크 정지 (삭제하지 않고 일시 중지)
    vTaskSuspend(NULL);

    prod_rx_app_deinit();
#endif
}
