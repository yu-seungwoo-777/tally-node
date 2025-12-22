/**
 * @file EthernetCore.cpp
 * @brief W5500 SPI Ethernet Core 구현
 */

#include "EthernetCore.h"
#include "log.h"
#include "log_tags.h"
#include "esp_eth_driver.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = TAG_ETHERNET;

// 정적 멤버 초기화
esp_eth_handle_t EthernetCore::s_eth_handle = nullptr;
esp_netif_t* EthernetCore::s_eth_netif = nullptr;
bool EthernetCore::s_initialized = false;
bool EthernetCore::s_link_up = false;
bool EthernetCore::s_got_ip = false;
bool EthernetCore::s_dhcp_mode = true;
char EthernetCore::s_ip[16] = {0};
char EthernetCore::s_netmask[16] = {0};
char EthernetCore::s_gateway[16] = {0};
char EthernetCore::s_mac[18] = {0};
TaskHandle_t EthernetCore::s_dhcp_timeout_task = nullptr;
char EthernetCore::s_static_ip[16] = {0};
char EthernetCore::s_static_netmask[16] = {0};
char EthernetCore::s_static_gateway[16] = {0};

void EthernetCore::ethEventHandler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t*)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        LOG_0(TAG, "Ethernet Link Up");
        LOG_0(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        snprintf(s_mac, sizeof(s_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        s_link_up = true;

        // 링크 재연결 시 DHCP 재시도
        if (s_eth_netif) {
            LOG_0(TAG, "링크 재연결, DHCP 시도 중...");
            s_got_ip = false;
            esp_netif_dhcpc_stop(s_eth_netif);
            esp_netif_dhcpc_start(s_eth_netif);
            s_dhcp_mode = true;

            // DHCP 타임아웃 태스크 시작 (기존 태스크가 있으면 무시)
            if (!s_dhcp_timeout_task) {
                xTaskCreate(dhcpTimeoutTask, "dhcp_timeout", 2048, nullptr, 5, &s_dhcp_timeout_task);
            }
        }
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        LOG_0(TAG, "Ethernet Link Down");
        s_link_up = false;
        s_got_ip = false;
        break;
    case ETHERNET_EVENT_START:
        LOG_0(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        LOG_0(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

void EthernetCore::ipEventHandler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    const esp_netif_ip_info_t* ip_info = &event->ip_info;

    LOG_0(TAG, "Ethernet Got IP Address");
    LOG_0(TAG, "~~~~~~~~~~~");
    LOG_0(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    LOG_0(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    LOG_0(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    LOG_0(TAG, "~~~~~~~~~~~");

    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&ip_info->ip));
    snprintf(s_netmask, sizeof(s_netmask), IPSTR, IP2STR(&ip_info->netmask));
    snprintf(s_gateway, sizeof(s_gateway), IPSTR, IP2STR(&ip_info->gw));
    s_got_ip = true;

    // DHCP 타임아웃 태스크 취소
    if (s_dhcp_timeout_task) {
        vTaskDelete(s_dhcp_timeout_task);
        s_dhcp_timeout_task = nullptr;
    }
}

void EthernetCore::dhcpTimeoutTask(void* arg)
{
    vTaskDelay(pdMS_TO_TICKS(10000));  // 10초 대기

    if (!s_got_ip && s_dhcp_mode) {
        LOG_0(TAG, "");
        LOG_0(TAG, "---------------------------------");
        LOG_0(TAG, "DHCP 타임아웃! Static IP로 전환...");
        enableStatic(s_static_ip, s_static_netmask, s_static_gateway);
        LOG_0(TAG, "---------------------------------");
        LOG_0(TAG, "");
    }

    s_dhcp_timeout_task = nullptr;
    vTaskDelete(nullptr);
}

esp_err_t EthernetCore::init(bool dhcp_enabled, const char* static_ip,
                             const char* static_netmask, const char* static_gateway)
{
    if (s_initialized) {
        LOG_0(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    // GPIO ISR 서비스 설치 (W5500 인터럽트 핀 사용을 위해)
    static bool gpio_isr_installed = false;
    if (!gpio_isr_installed) {
        esp_err_t ret = gpio_install_isr_service(0);
        if (ret == ESP_OK) {
            gpio_isr_installed = true;
            LOG_1(TAG, "GPIO ISR 서비스 설치 완료");
        } else if (ret != ESP_ERR_INVALID_STATE) {
            // 이미 설치되어 있으면 무시
            LOG_0(TAG, "GPIO ISR 서비스 설치 실패: %s", esp_err_to_name(ret));
        }
    }

    // Static IP 설정 저장
    strncpy(s_static_ip, static_ip, sizeof(s_static_ip) - 1);
    strncpy(s_static_netmask, static_netmask, sizeof(s_static_netmask) - 1);
    strncpy(s_static_gateway, static_gateway, sizeof(s_static_gateway) - 1);

    // W5500 RST 핀 초기화
    if (EORA_S3_W5500_RST >= 0) {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << EORA_S3_W5500_RST);
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io_conf);

        // 하드웨어 리셋 (TALLY_NODE 방식)
        LOG_1(TAG, "W5500 하드웨어 리셋 (RST:%d)", EORA_S3_W5500_RST);
        gpio_set_level(EORA_S3_W5500_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(10));   // LOW 유지: 10ms
        gpio_set_level(EORA_S3_W5500_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(50));   // HIGH 안정화: 50ms
        LOG_1(TAG, "W5500 리셋 완료, 안정화 대기 완료");
    }

    // SPI 버스 설정
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = EORA_S3_W5500_MOSI;
    buscfg.miso_io_num = EORA_S3_W5500_MISO;
    buscfg.sclk_io_num = EORA_S3_W5500_SCK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 0;

    // SPI 버스 초기화
    LOG_1(TAG, "SPI 버스 초기화 (MOSI:%d, MISO:%d, SCLK:%d)",
          EORA_S3_W5500_MOSI, EORA_S3_W5500_MISO, EORA_S3_W5500_SCK);
    esp_err_t ret = spi_bus_initialize(EORA_S3_W5500_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE = 이미 초기화됨 (정상)
        LOG_0(TAG, "SPI 버스 초기화 실패: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        LOG_1(TAG, "SPI 버스 이미 초기화됨 (재사용)");
    }

    // W5500 SPI 디바이스 설정 (ESP-IDF 5.5.0 방식)
    spi_device_interface_config_t devcfg = {};
    devcfg.command_bits = 16;
    devcfg.address_bits = 8;
    devcfg.mode = 0;
    devcfg.clock_speed_hz = 20 * 1000 * 1000;  // 20MHz (W5500 최대 속도)
    devcfg.queue_size = 20;
    devcfg.spics_io_num = EORA_S3_W5500_CS;

    // W5500 설정 (ESP-IDF 5.5.0 방식)
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(EORA_S3_W5500_SPI_HOST, &devcfg);
    w5500_config.int_gpio_num = EORA_S3_W5500_INT;

    // INT 핀 미사용 시 폴링 모드 활성화 (필수)
    if (EORA_S3_W5500_INT < 0) {
        w5500_config.poll_period_ms = 100;  // 100ms 폴링
        LOG_1(TAG, "INT 핀 미사용, 폴링 모드 활성화 (100ms)");
    }

    // MAC 설정
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.sw_reset_timeout_ms = 500;

    // PHY 설정
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = -1;  // 수동 리셋 완료

    LOG_1(TAG, "W5500 드라이버 생성...");
    LOG_1(TAG, "  CS:%d, INT:%d, RST:%d", EORA_S3_W5500_CS, EORA_S3_W5500_INT, EORA_S3_W5500_RST);
    LOG_1(TAG, "  SPI 클럭: 20MHz");

    // MAC 및 PHY 드라이버 생성 (ESP-IDF 5.5.0 API)
    esp_eth_mac_t* mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t* phy = esp_eth_phy_new_w5500(&phy_config);

    if (!mac || !phy) {
        LOG_0(TAG, "W5500 MAC/PHY 드라이버 생성 실패");
        if (mac) free(mac);
        if (phy) free(phy);
        return ESP_FAIL;
    }

    // 이더넷 드라이버 설치
    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    ret = esp_eth_driver_install(&eth_cfg, &s_eth_handle);
    if (ret != ESP_OK) {
        LOG_0(TAG, "이더넷 드라이버 설치 실패: %s", esp_err_to_name(ret));

        // W5500 감지 실패 디버깅 정보
        if (ret == ESP_ERR_INVALID_VERSION) {
            LOG_1(TAG, "===== W5500 칩 버전 불일치 =====");
            LOG_1(TAG, "원인: SPI 통신 실패 (칩 ID 읽기 실패)");
            LOG_1(TAG, "");
            LOG_1(TAG, "하드웨어 체크리스트:");
            LOG_1(TAG, "  1. W5500 모듈 장착 확인 (900TB/400TB 공통)");
            LOG_1(TAG, "  2. 전원 공급 확인 (3.3V)");
            LOG_1(TAG, "  3. SPI 핀 연결:");
            LOG_1(TAG, "     MOSI:%d, MISO:%d, SCK:%d, CS:%d",
                  EORA_S3_W5500_MOSI, EORA_S3_W5500_MISO, EORA_S3_W5500_SCK, EORA_S3_W5500_CS);
            LOG_1(TAG, "  4. 제어 핀: RST:%d, INT:%d", EORA_S3_W5500_RST, EORA_S3_W5500_INT);
            LOG_1(TAG, "");
            LOG_1(TAG, "SPI 설정:");
            LOG_1(TAG, "  버스: SPI3_HOST");
            LOG_1(TAG, "  클럭: 10MHz");
            LOG_1(TAG, "  모드: 0");
            LOG_1(TAG, "");
            LOG_1(TAG, "※ 900TB/400TB 차이: LoRa 주파수만 다름 (W5500 동일)");
            LOG_1(TAG, "================================");
        }

        return ESP_FAIL;
    }

    // W5500에 MAC 주소 설정 (ESP-IDF 공식 예제 방식)
    // W5500은 공장 MAC이 없으므로 ESP32 MAC 주소 기반으로 생성
    uint8_t base_mac[6];
    esp_efuse_mac_get_default(base_mac);
    uint8_t local_mac[6];
    esp_derive_local_mac(local_mac, base_mac);

    ret = esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, local_mac);
    if (ret != ESP_OK) {
        LOG_0(TAG, "MAC 주소 설정 실패: %s (계속 진행)", esp_err_to_name(ret));
    } else {
        LOG_1(TAG, "  MAC: %02X:%02X:%02X:%02X:%02X:%02X",
              local_mac[0], local_mac[1], local_mac[2], local_mac[3], local_mac[4], local_mac[5]);
    }

    // netif 설정
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);

    // netif와 드라이버 연결
    void* glue = esp_eth_new_netif_glue(s_eth_handle);
    ret = esp_netif_attach(s_eth_netif, glue);
    if (ret != ESP_OK) {
        LOG_0(TAG, "netif 연결 실패: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    // 이벤트 핸들러 등록
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                                &ethEventHandler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                                &ipEventHandler, nullptr));

    // 이더넷 시작
    LOG_1(TAG, "이더넷 시작...");
    ret = esp_eth_start(s_eth_handle);
    if (ret != ESP_OK) {
        LOG_0(TAG, "이더넷 시작 실패: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    s_dhcp_mode = dhcp_enabled;
    s_initialized = true;

    // DHCP 활성화 여부에 따라 분기
    if (s_dhcp_mode) {
        LOG_1(TAG, "DHCP 모드 활성화 (10초 타임아웃 후 Static IP로 자동 전환)");
        xTaskCreate(dhcpTimeoutTask, "dhcp_timeout", 2048, nullptr, 5, &s_dhcp_timeout_task);
    } else {
        LOG_1(TAG, "Static IP 모드 (DHCP 비활성화, 직접 연결용)");
        vTaskDelay(pdMS_TO_TICKS(500));  // 이더넷 시작 대기
        enableStatic(static_ip, static_netmask, static_gateway);
    }

    return ESP_OK;
}

EthernetStatus EthernetCore::getStatus()
{
    EthernetStatus status = {};

    status.initialized = s_initialized;
    status.link_up = s_link_up;
    status.got_ip = s_got_ip;
    status.dhcp_mode = s_dhcp_mode;
    strncpy(status.ip, s_ip, sizeof(status.ip) - 1);
    strncpy(status.netmask, s_netmask, sizeof(status.netmask) - 1);
    strncpy(status.gateway, s_gateway, sizeof(status.gateway) - 1);
    strncpy(status.mac, s_mac, sizeof(status.mac) - 1);

    return status;
}

esp_err_t EthernetCore::enableDHCP()
{
    if (!s_initialized || !s_eth_netif) {
        return ESP_FAIL;
    }

    LOG_0(TAG, "DHCP 활성화");
    esp_netif_dhcpc_start(s_eth_netif);
    s_dhcp_mode = true;

    // DHCP 타임아웃 태스크 시작
    if (!s_dhcp_timeout_task) {
        xTaskCreate(dhcpTimeoutTask, "dhcp_timeout", 2048, nullptr, 5, &s_dhcp_timeout_task);
    }

    return ESP_OK;
}

esp_err_t EthernetCore::enableStatic(const char* ip, const char* netmask, const char* gateway)
{
    if (!s_initialized || !s_eth_netif) {
        return ESP_FAIL;
    }

    LOG_0(TAG, "Static IP 설정: %s", ip);

    // DHCP 중지
    esp_netif_dhcpc_stop(s_eth_netif);

    // Static IP 설정
    esp_netif_ip_info_t ip_info = {};
    ip_info.ip.addr = esp_ip4addr_aton(ip);
    ip_info.netmask.addr = esp_ip4addr_aton(netmask);
    ip_info.gw.addr = esp_ip4addr_aton(gateway);

    esp_err_t ret = esp_netif_set_ip_info(s_eth_netif, &ip_info);
    if (ret == ESP_OK) {
        strncpy(s_ip, ip, sizeof(s_ip) - 1);
        strncpy(s_netmask, netmask, sizeof(s_netmask) - 1);
        strncpy(s_gateway, gateway, sizeof(s_gateway) - 1);
        s_got_ip = true;
        s_dhcp_mode = false;

        LOG_0(TAG, "Static IP 설정 완료");
        LOG_0(TAG, "IP: %s", s_ip);
        LOG_0(TAG, "Netmask: %s", s_netmask);
        LOG_0(TAG, "Gateway: %s", s_gateway);
    } else {
        LOG_0(TAG, "Static IP 설정 실패: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t EthernetCore::restart()
{
    if (!s_initialized || !s_eth_handle) {
        return ESP_FAIL;
    }

    LOG_0(TAG, "이더넷 재시작");

    esp_eth_stop(s_eth_handle);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_eth_start(s_eth_handle);

    return ESP_OK;
}

bool EthernetCore::isLinkUp()
{
    return s_link_up;
}

bool EthernetCore::hasIP()
{
    return s_got_ip;
}
