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

    while (1) {
        prod_tx_app_loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    prod_tx_app_deinit();
#else
    if (!prod_rx_app_init(NULL)) {
        T_LOGE(TAG, "RX app init failed");
        return;
    }
    prod_rx_app_start();

    while (1) {
        prod_rx_app_loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    prod_rx_app_deinit();
#endif
}
