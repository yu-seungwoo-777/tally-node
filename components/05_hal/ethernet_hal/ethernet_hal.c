/**
 * @file EthernetHal.c
 * @brief W5500 Ethernet HAL 구현 (ESP-IDF 5.5.0)
 */

#include "ethernet_hal.h"
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
#include "lwip/dns.h"  // LwIP DNS
#include "driver/gpio.h"
#include "driver/spi_master.h"

static const char* TAG = "05_Ethernet";

// ============================================================================
// 정적 변수
// ============================================================================

static bool s_initialized = false;
static bool s_started = false;
static bool s_detected = false;      // W5500 칩 감지 여부
static bool s_link_up = false;      // 링크 상태 추적
static ethernet_hal_state_t s_state = ETHERNET_HAL_STATE_IDLE;
static esp_eth_handle_t s_eth_handle = NULL;
static esp_netif_t* s_netif = NULL;
static ethernet_hal_event_callback_t s_event_callback = NULL;
static EventGroupHandle_t s_event_group = NULL;

// 이벤트 비트
#define ETH_HAL_STARTED_BIT    BIT0
#define ETH_HAL_STOPPED_BIT    BIT1
#define ETH_HAL_GOT_IP_BIT     BIT2

// ============================================================================
// 정적 함수 선언
// ============================================================================

static void eth_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);
static void ip_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);

// ============================================================================
// 이벤트 핸들러
// ============================================================================

static void eth_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == ETH_EVENT) {
        if (event_id == ETHERNET_EVENT_START) {
            T_LOGI(TAG, "Ethernet 시작됨");
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, ETH_HAL_STARTED_BIT);
            }
        } else if (event_id == ETHERNET_EVENT_STOP) {
            T_LOGI(TAG, "Ethernet 정지됨");
            s_started = false;
            s_link_up = false;
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, ETH_HAL_STOPPED_BIT);
            }
        } else if (event_id == ETHERNET_EVENT_CONNECTED) {
            T_LOGI(TAG, "Ethernet 링크 업");
            s_link_up = true;
        } else if (event_id == ETHERNET_EVENT_DISCONNECTED) {
            T_LOGW(TAG, "Ethernet 링크 다운");
            s_link_up = false;
        }
    }

    // 사용자 콜백 호출
    if (s_event_callback) {
        s_event_callback(arg, event_base, event_id, event_data);
    }
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_ETH_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            T_LOGI(TAG, "Ethernet IP 획득: " IPSTR, IP2STR(&event->ip_info.ip));
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, ETH_HAL_GOT_IP_BIT);
            }
        }
    }

    // 사용자 콜백 호출
    if (s_event_callback) {
        s_event_callback(arg, event_base, event_id, event_data);
    }
}

// ============================================================================
// 초기화/정리
// ============================================================================

esp_err_t ethernet_hal_init(void)
{
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "Ethernet HAL 초기화 중...");

    // 이벤트 그룹 생성
    s_event_group = xEventGroupCreate();
    if (!s_event_group) {
        T_LOGE(TAG, "이벤트 그룹 생성 실패");
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
    vTaskDelay(pdMS_TO_TICKS(10));  // LOW 유지: 10ms
    gpio_set_level(EORA_S3_W5500_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));  // HIGH 안정화: 50ms

    T_LOGI(TAG, "Ethernet HAL 초기화 완료");
    s_initialized = true;
    s_state = ETHERNET_HAL_STATE_IDLE;
    return ESP_OK;
}

esp_err_t ethernet_hal_deinit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Ethernet HAL 정리 중...");

    ethernet_hal_stop();
    vEventGroupDelete(s_event_group);
    s_event_group = NULL;

    s_initialized = false;
    s_state = ETHERNET_HAL_STATE_STOPPED;

    T_LOGI(TAG, "Ethernet HAL 정리 완료");
    return ESP_OK;
}

// ============================================================================
// 제어
// ============================================================================

esp_err_t ethernet_hal_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_started) {
        T_LOGW(TAG, "이미 시작됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "Ethernet 시작 중...");

    // 감지 상태 초기화
    s_detected = false;

    // netif 초기화 (ESP-IDF 5.5.0 필수)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "esp_netif_init 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 이벤트 루프 생성 (이미 존재하면 무시)
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "이벤트 루프 생성 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // SPI 버스 설정 (ESP-IDF 5.5.0 방식)
    spi_bus_config_t buscfg = {
        .mosi_io_num = EORA_S3_W5500_MOSI,
        .miso_io_num = EORA_S3_W5500_MISO,
        .sclk_io_num = EORA_S3_W5500_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };

    // SPI 버스 초기화
    T_LOGI(TAG, "SPI 버스: MOSI=%d, MISO=%d, SCK=%d, CS=%d",
             EORA_S3_W5500_MOSI, EORA_S3_W5500_MISO, EORA_S3_W5500_SCK, EORA_S3_W5500_CS);
    ret = spi_bus_initialize(EORA_S3_W5500_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "SPI 버스 초기화 실패: %s", esp_err_to_name(ret));
        return ret;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        T_LOGI(TAG, "SPI 버스 이미 초기화됨 (재사용)");
    }

    // SPI 디바이스 설정 (examples/1, 2 동일)
    spi_device_interface_config_t spi_devcfg = {
        .command_bits = 16,
        .address_bits = 8,
        .mode = 0,
        .clock_speed_hz = 20 * 1000 * 1000,  // 20MHz
        .queue_size = 20,
        .spics_io_num = EORA_S3_W5500_CS,
    };

    // W5500 MAC 설정 (ESP-IDF 5.5.0 방식)
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(EORA_S3_W5500_SPI_HOST, &spi_devcfg);
    w5500_config.int_gpio_num = EORA_S3_W5500_INT;  // -1 (폴링 모드)

    // INT 핀이 없으면 폴링 모드 설정 필수
    if (w5500_config.int_gpio_num < 0) {
        w5500_config.poll_period_ms = 100;  // 100ms 폴링
        T_LOGI(TAG, "INT 핀 미사용, 폴링 모드 활성화 (100ms)");
    }

    // MAC 설정 (examples/1 참고)
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.sw_reset_timeout_ms = 500;

    // PHY 설정
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = -1;  // 수동 리셋 완료

    T_LOGI(TAG, "W5500 드라이버 생성 (SPI 클럭: 20MHz)...");

    // MAC과 PHY 생성
    esp_eth_mac_t* mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t* phy = esp_eth_phy_new_w5500(&phy_config);

    if (!mac || !phy) {
        T_LOGE(TAG, "W5500 MAC/PHY 드라이버 생성 실패");
        if (mac) free(mac);
        if (phy) free(phy);
        return ESP_FAIL;
    }

    // Ethernet 드라이버 설치
    esp_eth_config_t eth_config = {
        .mac = mac,
        .phy = phy,
        .check_link_period_ms = 2000,
    };

    ret = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Ethernet 드라이버 설치 실패: %s", esp_err_to_name(ret));
        s_detected = false;  // 감지 실패

        // W5500 감지 실패 디버깅 정보
        if (ret == ESP_ERR_INVALID_VERSION) {
            T_LOGE(TAG, "===== W5500 칩 버전 불일치 =====");
            T_LOGE(TAG, "원인: SPI 통신 실패 (칩 ID 읽기 실패)");
            T_LOGE(TAG, "");
            T_LOGE(TAG, "하드웨어 체크리스트:");
            T_LOGE(TAG, "  1. W5500 모듈 장착 확인");
            T_LOGE(TAG, "  2. 전원 공급 확인 (3.3V)");
            T_LOGE(TAG, "  3. SPI 핀 연결: MOSI=%d, MISO=%d, SCK=%d, CS=%d",
                     EORA_S3_W5500_MOSI, EORA_S3_W5500_MISO, EORA_S3_W5500_SCK, EORA_S3_W5500_CS);
            T_LOGE(TAG, "  4. 제어 핀: RST=%d, INT=%d", EORA_S3_W5500_RST, EORA_S3_W5500_INT);
            T_LOGE(TAG, "");
            T_LOGE(TAG, "SPI 설정:");
            T_LOGE(TAG, "  버스: SPI3_HOST");
            T_LOGE(TAG, "  클럭: 20MHz");
            T_LOGE(TAG, "  모드: 0");
            T_LOGE(TAG, "================================");
        }

        return ret;
    }

    s_detected = true;  // 감지 성공

    // MAC 주소 설정
    uint8_t base_mac[6];
    esp_efuse_mac_get_default(base_mac);
    uint8_t local_mac[6];
    esp_derive_local_mac(local_mac, base_mac);
    esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, local_mac);

    // 이벤트 핸들러 등록
    ret = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "ETH 이벤트 핸들러 등록 실패");
        return ret;
    }

    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "IP 이벤트 핸들러 등록 실패");
        return ret;
    }

    // netif 생성 (ESP-IDF 5.5.0 방식)
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif = esp_netif_new(&netif_cfg);

    // DNS 서버 미리 설정 (DHCP 시작 전에 설정해야 클리어되지 않음)
    ip_addr_t dns_primary, dns_backup;
    dns_primary.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");
    dns_primary.type = IPADDR_TYPE_V4;
    dns_backup.u_addr.ip4.addr = esp_ip4addr_aton("1.1.1.1");
    dns_backup.type = IPADDR_TYPE_V4;

    dns_setserver(0, &dns_primary);
    dns_setserver(1, &dns_backup);

    T_LOGI(TAG, "Ethernet netif 생성 완료 (DNS 미리 설정: 8.8.8.8, 1.1.1.1)");

    // netif와 Ethernet 드라이버 연결
    void* glue = esp_eth_new_netif_glue(s_eth_handle);
    ret = esp_netif_attach(s_netif, glue);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "netif 연결 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    // 시작
    ret = esp_eth_start(s_eth_handle);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Ethernet 시작 실패: %s", esp_err_to_name(ret));
        return ret;
    }

    s_started = true;
    s_state = ETHERNET_HAL_STATE_STARTED;

    T_LOGI(TAG, "Ethernet 시작 완료");
    return ESP_OK;
}

esp_err_t ethernet_hal_stop(void)
{
    if (!s_initialized || !s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Ethernet 정지 중...");

    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
    }

    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }

    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler);

    s_started = false;
    s_state = ETHERNET_HAL_STATE_STOPPED;

    T_LOGI(TAG, "Ethernet 정지 완료");
    return ESP_OK;
}

esp_err_t ethernet_hal_restart(void)
{
    esp_err_t ret = ethernet_hal_stop();
    if (ret != ESP_OK) {
        return ret;
    }
    // 드라이버 재초기화를 위한 충분한 대기 시간
    vTaskDelay(pdMS_TO_TICKS(500));
    return ethernet_hal_start();
}

// ============================================================================
// IP 설정
// ============================================================================

esp_err_t ethernet_hal_enable_dhcp(void)
{
    if (!s_netif) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "DHCP 모드 활성화");

    esp_netif_dhcp_status_t dhcp_status;
    esp_netif_dhcpc_get_status(s_netif, &dhcp_status);
    if (dhcp_status == ESP_NETIF_DHCP_STARTED) {
        return ESP_OK;  // 이미 DHCP 모드
    }

    esp_netif_dhcpc_stop(s_netif);
    return esp_netif_dhcpc_start(s_netif);
}

esp_err_t ethernet_hal_enable_static(const char* ip, const char* netmask, const char* gateway)
{
    if (!s_netif) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Static IP 모드 활성화");
    T_LOGI(TAG, "  IP: %s", ip);
    T_LOGI(TAG, "  Netmask: %s", netmask);
    T_LOGI(TAG, "  Gateway: %s", gateway);

    // DHCP 정지
    esp_netif_dhcpc_stop(s_netif);

    // Static IP 설정
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = esp_ip4addr_aton(ip);
    ip_info.netmask.addr = esp_ip4addr_aton(netmask);
    ip_info.gw.addr = esp_ip4addr_aton(gateway);

    return esp_netif_set_ip_info(s_netif, &ip_info);
}

// ============================================================================
// 상태 조회
// ============================================================================

ethernet_hal_state_t ethernet_hal_get_state(void)
{
    return s_state;
}

bool ethernet_hal_is_initialized(void)
{
    return s_initialized;
}

bool ethernet_hal_is_link_up(void)
{
    return s_link_up;
}

bool ethernet_hal_has_ip(void)
{
    if (!s_netif) {
        return false;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(s_netif, &ip_info);
    return (ret == ESP_OK && ip_info.ip.addr != 0);
}

esp_err_t ethernet_hal_get_status(ethernet_hal_status_t* status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

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

        // MAC 주소
        uint8_t mac[6];
        if (esp_netif_get_mac(s_netif, mac) == ESP_OK) {
            snprintf(status->mac, sizeof(status->mac),
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }

    return ESP_OK;
}

esp_netif_t* ethernet_hal_get_netif(void)
{
    return s_netif;
}

// ============================================================================
// 이벤트 핸들러
// ============================================================================

esp_err_t ethernet_hal_register_event_handler(ethernet_hal_event_callback_t callback)
{
    s_event_callback = callback;
    return ESP_OK;
}
