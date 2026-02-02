/**
 * @file EthernetHal.c
 * @brief W5500 Ethernet HAL 구현 (ESP-IDF 5.5.0)
 */

#include "ethernet_hal.h"
#include "error_macros.h"
#include "PinConfig.h"
#include "t_log.h"
#include <string.h>
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_eth_driver.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

static const char* TAG = "05_Ethernet";

// ============================================================================
// 정적 변수
// ============================================================================

static bool s_initialized = false;
static bool s_started = false;
static bool s_detected = false;          // W5500 칩 감지 여부
static bool s_link_up = false;           // 링크 상태 추적
static bool s_recovering = false;        // 링크 복구 중 플래그
static bool s_restarting = false;        // 재시작 중 플래그 (중복 재시작 방지)
static ethernet_hal_state_t s_state = ETHERNET_HAL_STATE_IDLE;
static esp_eth_handle_t s_eth_handle = NULL;
static esp_netif_t* s_netif = NULL;
static void* s_eth_glue = NULL;          // netif glue 핸들 (레퍼런스 관리용)
static esp_eth_mac_t* s_eth_mac = NULL;  // MAC 핸들 (SPI 디바이스 정리용)
static esp_eth_phy_t* s_eth_phy = NULL;  // PHY 핸들 (리소스 정리용)
static TaskHandle_t s_recovery_task = NULL;  // 복구 태스크 핸들 (정리용)
static ethernet_hal_event_callback_t s_event_callback = NULL;
static EventGroupHandle_t s_event_group = NULL;

/** ESP-IDF 5.5.0: 이벤트 핸들러 인스턴스 */
static esp_event_handler_instance_t s_eth_event_instance = NULL;
static esp_event_handler_instance_t s_ip_event_instance = NULL;

// 이벤트 비트
#define ETH_HAL_STARTED_BIT    BIT0
#define ETH_HAL_STOPPED_BIT    BIT1
#define ETH_HAL_GOT_IP_BIT     BIT2

// 링크 복구 설정
#define ETH_HAL_LINK_RECOVERY_DELAY_MS    2000  // 링크 다운 후 복구 대기 시간 (ms) - 빠른 복구
#define ETH_HAL_LINK_RECOVERY_STACK_SIZE  4096  // 복구 태스크 스택 크기

// ============================================================================
// 정적 함수 선언
// ============================================================================

static void eth_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);
static void ip_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);
static void link_recovery_task(void* arg);

// ============================================================================
// 링크 복구 태스크
// ============================================================================

/**
 * @brief 링크 복구 태스크
 *
 * 링크 다운 후 지정된 시간 동안 대기한 후 이더넷을 재시작합니다.
 *
 * @param arg 사용자 데이터 (미사용)
 */
static void link_recovery_task(void* arg)
{
    T_LOGW(TAG, "recovery:start:wait_ms:%d", ETH_HAL_LINK_RECOVERY_DELAY_MS);

    // 지정된 시간 동안 대기 (링크가 자연적으로 복구될 수 있도록)
    vTaskDelay(pdMS_TO_TICKS(ETH_HAL_LINK_RECOVERY_DELAY_MS));

    // 대기 후에도 링크가 다운인 경우 재시작
    if (!s_link_up && s_started && !s_restarting) {
        T_LOGW(TAG, "recovery:restart");
        ethernet_hal_restart();  // 함수 내부에서 s_restarting 관리
    } else {
        T_LOGD(TAG, "recovery:cancel:link_up=%d,started=%d,restarting=%d",
               s_link_up, s_started, s_restarting);
    }

    // 복구 플래그 및 핸들 해제
    s_recovering = false;
    s_recovery_task = NULL;

    // 태스크 자가 삭제
    vTaskDelete(NULL);
}

// ============================================================================
// 이벤트 핸들러
// ============================================================================

/**
 * @brief Ethernet 이벤트 핸들러
 *
 * ESP-IDF Ethernet 이벤트를 처리하고 상태를 업데이트합니다.
 *
 * @param arg 사용자 데이터 (콜백 전달용)
 * @param event_base 이벤트 베이스 (ETH_EVENT)
 * @param event_id 이벤트 ID
 * @param event_data 이벤트 데이터
 */
static void eth_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == ETH_EVENT) {
        switch (event_id) {
            case ETHERNET_EVENT_START:
                T_LOGD(TAG, "evt:start");
                if (s_event_group) {
                    xEventGroupSetBits(s_event_group, ETH_HAL_STARTED_BIT);
                }
                break;

            case ETHERNET_EVENT_STOP:
                T_LOGD(TAG, "evt:stop");
                s_started = false;
                s_link_up = false;
                if (s_event_group) {
                    xEventGroupSetBits(s_event_group, ETH_HAL_STOPPED_BIT);
                }
                break;

            case ETHERNET_EVENT_CONNECTED:
                T_LOGD(TAG, "evt:link_up");
                s_link_up = true;
                s_recovering = false;  // 링크 복구 완료, 플래그 해제
                break;

            case ETHERNET_EVENT_DISCONNECTED:
                T_LOGE(TAG, "evt:link_down");
                s_link_up = false;

                // 링크 복구 태스크 시작 (이미 복구 중이거나 재시작 중이 아닐 때만)
                if (!s_recovering && !s_restarting && s_started) {
                    s_recovering = true;
                    BaseType_t ret = xTaskCreate(
                        link_recovery_task,
                        "link_rec",
                        ETH_HAL_LINK_RECOVERY_STACK_SIZE,
                        NULL,
                        5,  // 우선순위
                        &s_recovery_task  // 핸들 저장 (정리 시 사용)
                    );
                    if (ret != pdPASS) {
                        T_LOGE(TAG, "fail:recovery_task");
                        s_recovering = false;
                        s_recovery_task = NULL;
                    }
                }
                break;

            default:
                break;
        }
    }

    if (s_event_callback) {
        s_event_callback(arg, event_base, event_id, event_data);
    }
}

/**
 * @brief IP 이벤트 핸들러
 *
 * ESP-IDF IP 이벤트를 처리하고 상태를 업데이트합니다.
 *
 * @param arg 사용자 데이터 (콜백 전달용)
 * @param event_base 이벤트 베이스 (IP_EVENT)
 * @param event_id 이벤트 ID
 * @param event_data 이벤트 데이터
 */
static void ip_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_ETH_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            T_LOGD(TAG, "evt:got_ip:" IPSTR, IP2STR(&event->ip_info.ip));
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, ETH_HAL_GOT_IP_BIT);
            }
        }
    }

    if (s_event_callback) {
        s_event_callback(arg, event_base, event_id, event_data);
    }
}

// ============================================================================
// 초기화/정리
// ============================================================================

/**
 * @brief Ethernet HAL 초기화
 *
 * W5500 하드웨어 리셋을 수행하고 이벤트 그룹을 생성합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ethernet_hal_init(void)
{
    T_LOGD(TAG, "init");

    if (s_initialized) {
        T_LOGD(TAG, "ok:already");
        return ESP_OK;
    }

    // 이벤트 그룹 생성
    s_event_group = xEventGroupCreate();
    if (!s_event_group) {
        T_LOGE(TAG, "fail:evtgrp");
        return ESP_ERR_NO_MEM;
    }

    // W5500 하드웨어 리셋
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << EORA_S3_W5500_RST);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(EORA_S3_W5500_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(EORA_S3_W5500_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    s_initialized = true;
    s_state = ETHERNET_HAL_STATE_IDLE;

    T_LOGD(TAG, "ok");
    return ESP_OK;
}

/**
 * @brief Ethernet HAL 해제
 *
 * Ethernet을 정지하고 리소스를 정리합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ethernet_hal_deinit(void)
{
    T_LOGD(TAG, "deinit");

    RETURN_ERR_IF_NOT_INIT(s_initialized);

    ethernet_hal_stop();
    vEventGroupDelete(s_event_group);
    s_event_group = NULL;

    s_initialized = false;
    s_state = ETHERNET_HAL_STATE_STOPPED;

    T_LOGD(TAG, "ok");
    return ESP_OK;
}

// ============================================================================
// 제어
// ============================================================================

/**
 * @brief Ethernet 시작
 *
 * W5500 Ethernet 칩을 초기화하고 시작합니다.
 * SPI 통신, MAC/PHY 드라이버, netif를 설정합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ethernet_hal_start(void)
{
    T_LOGD(TAG, "start");

    RETURN_ERR_IF_NOT_INIT(s_initialized);

    if (s_started) {
        T_LOGD(TAG, "ok:already");
        return ESP_OK;
    }

    s_detected = false;

    // netif 초기화
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "fail:netif:0x%x", ret);
        return ret;
    }

    // 이벤트 루프 생성
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "fail:evtloop:0x%x", ret);
        return ret;
    }

    // SPI 버스 설정
    spi_bus_config_t buscfg = {
        .mosi_io_num = EORA_S3_W5500_MOSI,
        .miso_io_num = EORA_S3_W5500_MISO,
        .sclk_io_num = EORA_S3_W5500_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,  // HTTPS 대용량 전송을 위한 버퍼 크기
    };

    ret = spi_bus_initialize(EORA_S3_W5500_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "fail:spi:0x%x", ret);
        return ret;
    }

    // ========================================================================
    // W5500 칩 유무 감지 (SPI 통해 칩 ID 확인)
    // ========================================================================
    {
        // 임시 SPI 디바이스 설정 (간단한 읽기 전용)
        spi_device_interface_config_t temp_devcfg = {
            .command_bits = 16,
            .address_bits = 8,
            .dummy_bits = 8,
            .mode = 0,
            .clock_speed_hz = 2 * 1000 * 1000,  // 2MHz (안정적인 감지)
            .queue_size = 1,
            .spics_io_num = EORA_S3_W5500_CS,
            .flags = 0,
        };

        spi_device_handle_t temp_handle;
        ret = spi_bus_add_device(EORA_S3_W5500_SPI_HOST, &temp_devcfg, &temp_handle);
        if (ret != ESP_OK) {
            T_LOGE(TAG, "fail:spi_add:0x%x", ret);
            return ret;
        }

        // W5500 칩 ID 레지스터(0x0000) 읽기
        // W5500 SPI 형식: [Opcode(1byte)][Address(2bytes)][Data(1byte)][Dummy(1byte)]
        // Read: 0x00 + Address[15:0]
        uint32_t tx_data = (0x00 << 16) | 0x0000;  // Read opcode + address 0
        uint32_t rx_data = 0;

        spi_transaction_t trans = {
            .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
            .length = 32,  // 32비트 전송
            .tx_data[0] = (tx_data >> 24) & 0xFF,
            .tx_data[1] = (tx_data >> 16) & 0xFF,
            .tx_data[2] = (tx_data >> 8) & 0xFF,
            .tx_data[3] = tx_data & 0xFF,
        };

        ret = spi_device_polling_transmit(temp_handle, &trans);

        // 임시 디바이스 제거
        spi_bus_remove_device(temp_handle);

        if (ret != ESP_OK) {
            T_LOGW(TAG, "w5500:not_detected:spi_fail");
            spi_bus_free(EORA_S3_W5500_SPI_HOST);
            return ESP_ERR_NOT_FOUND;
        }

        // 수신 데이터에서 칩 ID 추출 (하위 바이트)
        rx_data = ((uint32_t)trans.rx_data[0] << 24) |
                  ((uint32_t)trans.rx_data[1] << 16) |
                  ((uint32_t)trans.rx_data[2] << 8) |
                  trans.rx_data[3];

        uint8_t chip_id = rx_data & 0xFF;

        if (chip_id != 0x04) {
            // W5500 칩 ID가 아니면 칩 없음
            T_LOGW(TAG, "w5500:not_detected:chip_id:0x%02x", chip_id);
            spi_bus_free(EORA_S3_W5500_SPI_HOST);
            return ESP_ERR_NOT_FOUND;
        }

        T_LOGD(TAG, "w5500:detected:chip_id:0x%02x", chip_id);
    }

    // SPI 디바이스 설정
    spi_device_interface_config_t spi_devcfg = {
        .command_bits = 16,
        .address_bits = 8,
        .mode = 0,
        .clock_speed_hz = 8 * 1000 * 1000,  // 8MHz (HTTPS 안정성을 위해 낮춤)
        .queue_size = 32,
        .spics_io_num = EORA_S3_W5500_CS,
    };

    // W5500 MAC 설정
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(EORA_S3_W5500_SPI_HOST, &spi_devcfg);
    w5500_config.int_gpio_num = EORA_S3_W5500_INT;

    // INT 핀 연결 여부 감지 (W5500 INT는 active-low, 유휴 시 High 출력)
    // 풀다운 모드로 설정하여 W5500 연결 여부 확인
    if (w5500_config.int_gpio_num >= 0) {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << w5500_config.int_gpio_num);
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;  // 풀다운
        io_conf.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io_conf);

        // W5500가 연결되어 있으면 INT 핀은 High (유휴 상태)
        // 연결되어 있지 않으면 핀은 Low (풀다운에 의해)
        vTaskDelay(pdMS_TO_TICKS(10));  // 안정화 대기
        int int_level = gpio_get_level(w5500_config.int_gpio_num);

        if (int_level == 1) {
            // High 감지 = W5500 INT 연결됨
            T_LOGD(TAG, "int:connected");
        } else {
            // Low 감지 = INT 핀 미연결 (폴링 모드 사용)
            w5500_config.int_gpio_num = -1;
            T_LOGD(TAG, "int:not_connected");
        }
    }

    // INT GPIO가 없거나 감지되지 않으면 폴링 모드 사용
    if (w5500_config.int_gpio_num < 0) {
        w5500_config.poll_period_ms = 20;  // 20ms (인터럽트 부하와 응답 속도 균형)
    }

    // MAC/PHY 설정
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.sw_reset_timeout_ms = 500;

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = -1;

    // MAC과 PHY 생성 (핸들 저장 필수 - 정리 시 사용)
    s_eth_mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    s_eth_phy = esp_eth_phy_new_w5500(&phy_config);

    if (!s_eth_mac || !s_eth_phy) {
        T_LOGE(TAG, "fail:mac_phy");
        if (s_eth_mac) {
            s_eth_mac->del(s_eth_mac);
            s_eth_mac = NULL;
        }
        if (s_eth_phy) {
            s_eth_phy->del(s_eth_phy);
            s_eth_phy = NULL;
        }
        return ESP_FAIL;
    }

    // Ethernet 드라이버 설치
    esp_eth_config_t eth_config = {
        .mac = s_eth_mac,
        .phy = s_eth_phy,
        .check_link_period_ms = 2000,
    };

    ret = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:driver:0x%x", ret);
        s_detected = false;
        if (s_eth_mac) {
            s_eth_mac->del(s_eth_mac);
            s_eth_mac = NULL;
        }
        if (s_eth_phy) {
            s_eth_phy->del(s_eth_phy);
            s_eth_phy = NULL;
        }
        return ret;
    }

    s_detected = true;

    // MAC 주소 설정
    uint8_t base_mac[6];
    esp_efuse_mac_get_default(base_mac);
    uint8_t local_mac[6];
    esp_derive_local_mac(local_mac, base_mac);
    esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, local_mac);

    // 이벤트 핸들러 등록 (ESP-IDF 5.5.0: instance API 사용)
    ret = esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL, &s_eth_event_instance);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:evt_hdlr:0x%x", ret);
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL, &s_ip_event_instance);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:ip_hdlr:0x%x", ret);
        return ret;
    }

    // netif 생성
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif = esp_netif_new(&netif_cfg);

    // DNS 설정 (ESP-IDF 5.5.0: esp_netif_set_dns_info 사용)
    esp_netif_dns_info_t dns_info;
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;
    dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");

    esp_netif_set_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns_info);

    dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton("1.1.1.1");
    esp_netif_set_dns_info(s_netif, ESP_NETIF_DNS_BACKUP, &dns_info);

    // netif 연결 (glue 핸들 저장 필수 - 정리 시 사용)
    s_eth_glue = esp_eth_new_netif_glue(s_eth_handle);
    ret = esp_netif_attach(s_netif, s_eth_glue);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:attach:0x%x", ret);
        return ret;
    }

    // PHY 설정: 10Mbps Full Duplex 고정 (특정 공유기 호환성)
    // 드라이버 시작 전에 설정해야 함 (esp_eth_ioctl은 정지 상태에서만 동작)
    // Auto-negotiation 비활성화
    bool autoneg = false;
    ret = esp_eth_ioctl(s_eth_handle, ETH_CMD_S_AUTONEGO, &autoneg);
    if (ret != ESP_OK) {
        T_LOGW(TAG, "warn:autoneg:0x%x", ret);
    } else {
        T_LOGD(TAG, "phy:autoneg_off");
    }

    // Speed 10Mbps 설정 (Auto-negotiation 비활성화 후에 설정 가능)
    eth_speed_t speed = ETH_SPEED_10M;
    ret = esp_eth_ioctl(s_eth_handle, ETH_CMD_S_SPEED, &speed);
    if (ret != ESP_OK) {
        T_LOGW(TAG, "warn:speed:0x%x", ret);
    } else {
        T_LOGD(TAG, "phy:10mbps");
    }

    // Duplex Full 설정
    eth_duplex_t duplex = ETH_DUPLEX_FULL;
    ret = esp_eth_ioctl(s_eth_handle, ETH_CMD_S_DUPLEX_MODE, &duplex);
    if (ret != ESP_OK) {
        T_LOGW(TAG, "warn:duplex:0x%x", ret);
    } else {
        T_LOGD(TAG, "phy:full_duplex");
    }

    // 시작
    ret = esp_eth_start(s_eth_handle);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:start:0x%x", ret);
        return ret;
    }

    s_started = true;
    s_state = ETHERNET_HAL_STATE_STARTED;

    T_LOGD(TAG, "ok");
    return ESP_OK;
}

/**
 * @brief Ethernet 정지
 *
 * Ethernet을 정지하고 리소스를 해제합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ethernet_hal_stop(void)
{
    T_LOGD(TAG, "stop");

    if (!s_initialized || !s_started) {
        T_LOGE(TAG, "fail:invalid_state");
        return ESP_ERR_INVALID_STATE;
    }

    // 1. 이벤트 핸들러 등록 해제 (레퍼런스 제거 최우선)
    if (s_eth_event_instance) {
        esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, s_eth_event_instance);
        s_eth_event_instance = NULL;
    }
    if (s_ip_event_instance) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, s_ip_event_instance);
        s_ip_event_instance = NULL;
    }

    // 1-1. 복구 태스크 대기 (실행 중이면 완료될 때까지 대기)
    if (s_recovery_task != NULL) {
        T_LOGD(TAG, "wait:recovery_task");
        // 복구 태스크가 자가 삭제될 때까지 최대 3초 대기
        for (int i = 0; i < 30 && s_recovery_task != NULL; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (s_recovery_task != NULL) {
            T_LOGW(TAG, "warn:recovery_task_timeout");
            s_recovery_task = NULL;  // 타임아웃 후 강제 초기화
        }
        s_recovering = false;
    }

    // 2. Ethernet 정지 (netif 해제 전 필수)
    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
        vTaskDelay(pdMS_TO_TICKS(200));  // 정지 완료 대기
    }

    // 3. netif 해제 (glue 삭제 전 필수)
    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
        vTaskDelay(pdMS_TO_TICKS(100));  // netif 정리 완료 대기
    }

    // 4. netif glue 해제 (드라이버 레퍼런스 제거)
    if (s_eth_glue) {
        esp_eth_del_netif_glue(s_eth_glue);
        s_eth_glue = NULL;
    }

    // 5. 드라이버 언인스톨 (MAC/PHY 삭제 전 필수)
    if (s_eth_handle) {
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
        vTaskDelay(pdMS_TO_TICKS(200));  // 언인스톨 완료 대기
    }

    // 6. MAC 삭제 (SPI 디바이스 제거)
    if (s_eth_mac) {
        s_eth_mac->del(s_eth_mac);
        s_eth_mac = NULL;
    }

    // 7. PHY 삭제
    if (s_eth_phy) {
        s_eth_phy->del(s_eth_phy);
        s_eth_phy = NULL;
    }

    // 8. SPI 버스 해제 (재시작 시 중복 초기화 방지)
    esp_err_t ret = spi_bus_free(EORA_S3_W5500_SPI_HOST);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGW(TAG, "warn:spi_free:0x%x", ret);
    }

    s_started = false;
    s_state = ETHERNET_HAL_STATE_STOPPED;

    T_LOGD(TAG, "ok");
    return ESP_OK;
}

/**
 * @brief Ethernet 재시작
 *
 * Ethernet을 정지했다가 다시 시작합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ethernet_hal_restart(void)
{
    T_LOGD(TAG, "restart");

    // 재시작 중 플래그 설정 (중복 재시작 및 복구 태스크 방지)
    if (s_restarting) {
        T_LOGW(TAG, "warn:already_restarting");
        return ESP_ERR_INVALID_STATE;
    }

    s_restarting = true;
    esp_err_t ret = ethernet_hal_stop();
    if (ret != ESP_OK) {
        s_restarting = false;
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    ret = ethernet_hal_start();
    s_restarting = false;
    return ret;
}

// ============================================================================
// IP 설정
// ============================================================================

/**
 * @brief DHCP 모드 활성화
 *
 * Ethernet 인터페이스에서 DHCP를 사용하여 IP를 할당받습니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ethernet_hal_enable_dhcp(void)
{
    T_LOGD(TAG, "dhcp_on");

    if (!s_netif) {
        T_LOGE(TAG, "fail:no_netif");
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_dhcp_status_t dhcp_status;
    esp_netif_dhcpc_get_status(s_netif, &dhcp_status);
    if (dhcp_status == ESP_NETIF_DHCP_STARTED) {
        T_LOGD(TAG, "ok:already");
        return ESP_OK;
    }

    esp_netif_dhcpc_stop(s_netif);
    esp_err_t ret = esp_netif_dhcpc_start(s_netif);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:0x%x", ret);
    }
    return ret;
}

/**
 * @brief Static IP 모드 활성화
 *
 * Ethernet 인터페이스에 고정 IP를 설정합니다.
 *
 * @param ip IP 주소 문자열
 * @param netmask 서브넷 마스크 문자열
 * @param gateway 게이트웨이 주소 문자열
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ethernet_hal_enable_static(const char* ip, const char* netmask, const char* gateway)
{
    T_LOGD(TAG, "static_ip:%s", ip);

    if (!s_netif) {
        T_LOGE(TAG, "fail:no_netif");
        return ESP_ERR_INVALID_STATE;
    }

    RETURN_ERR_IF_NULL(ip);
    RETURN_ERR_IF_NULL(netmask);
    RETURN_ERR_IF_NULL(gateway);

    esp_netif_dhcpc_stop(s_netif);

    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = esp_ip4addr_aton(ip);
    ip_info.netmask.addr = esp_ip4addr_aton(netmask);
    ip_info.gw.addr = esp_ip4addr_aton(gateway);

    esp_err_t ret = esp_netif_set_ip_info(s_netif, &ip_info);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:0x%x", ret);
    }
    return ret;
}

// ============================================================================
// 상태 조회
// ============================================================================

/**
 * @brief Ethernet 상태 조회
 *
 * @return 현재 Ethernet 상태
 */
ethernet_hal_state_t ethernet_hal_get_state(void)
{
    return s_state;
}

/**
 * @brief 초기화 여부 확인
 *
 * @return true 초기화됨, false 초기화 안됨
 */
bool ethernet_hal_is_initialized(void)
{
    return s_initialized;
}

/**
 * @brief 링크 상태 조회
 *
 * @return true 링크 업, false 링크 다운
 */
bool ethernet_hal_is_link_up(void)
{
    return s_link_up;
}

/**
 * @brief IP 획득 여부 확인
 *
 * @return true IP 획득됨, false IP 없음
 */
bool ethernet_hal_has_ip(void)
{
    if (!s_netif) {
        return false;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(s_netif, &ip_info);
    return (ret == ESP_OK && ip_info.ip.addr != 0);
}

/**
 * @brief Ethernet 상태 정보 조회
 *
 * 현재 Ethernet 상태를 구조체로 반환합니다.
 *
 * @param status 상태 정보를 저장할 버퍼 (NULL 불가)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t ethernet_hal_get_status(ethernet_hal_status_t* status)
{
    RETURN_ERR_IF_NULL(status);

    memset(status, 0, sizeof(ethernet_hal_status_t));

    status->initialized = s_initialized;
    status->detected = s_detected;
    status->link_up = ethernet_hal_is_link_up();
    status->got_ip = ethernet_hal_has_ip();

    if (s_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(s_netif, &ip_info) == ESP_OK) {
            snprintf(status->ip, sizeof(status->ip), IPSTR, IP2STR(&ip_info.ip));
            snprintf(status->netmask, sizeof(status->netmask), IPSTR, IP2STR(&ip_info.netmask));
            snprintf(status->gateway, sizeof(status->gateway), IPSTR, IP2STR(&ip_info.gw));
        }

        uint8_t mac[6];
        if (esp_netif_get_mac(s_netif, mac) == ESP_OK) {
            snprintf(status->mac, sizeof(status->mac),
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }

    return ESP_OK;
}

/**
 * @brief netif 핸들 조회
 *
 * @return netif 핸들 (없으면 NULL)
 */
esp_netif_t* ethernet_hal_get_netif(void)
{
    return s_netif;
}

// ============================================================================
// 이벤트 핸들러
// ============================================================================

/**
 * @brief 사용자 정의 이벤트 콜백 등록
 *
 * Ethernet/IP 이벤트 발생 시 호출될 콜백 함수를 등록합니다.
 *
 * @param callback 콜백 함수 (NULL 해제)
 * @return ESP_OK 성공
 */
esp_err_t ethernet_hal_register_event_handler(ethernet_hal_event_callback_t callback)
{
    s_event_callback = callback;
    T_LOGD(TAG, "cb:%s", callback ? "set" : "clr");
    return ESP_OK;
}
