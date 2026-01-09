/**
 * @file DisplayManager.cpp
 * @brief 디스플레이 관리자 구현
 */

#include "DisplayManager.h"
#include "display_driver.h"
#include "BootPage.h"
#include "TxPage.h"
#include "RxPage.h"
#include "t_log.h"
#include "event_bus.h"
#include "TallyTypes.h"
#include <string.h>

// ============================================================================
// 상수
// ============================================================================

static const char* TAG = "DisplayMgr";
#define DEFAULT_REFRESH_INTERVAL_MS  500  // 2 FPS
#define MAX_PAGES                     5
#define STATUS_LOG_INTERVAL_MS       5000  // 상태 로그 출력 주기 (5초)

// ============================================================================
// 내부 상태
// ============================================================================

// 통합 데이터 저장 구조체 (event_bus 구조체 그대로 사용)
typedef struct {
    system_info_event_t system;    // EVT_INFO_UPDATED
    bool system_valid;

    // LoRa 정보 (EVT_LORA_RSSI_CHANGED)
    struct {
        int16_t rssi;
        float snr;
        bool valid;
    } lora;

    // Tally 정보 (EVT_TALLY_STATE_CHANGED, RX 전용)
    struct {
        uint8_t pgm_channels[20];
        uint8_t pvw_channels[20];
        uint8_t pgm_count;
        uint8_t pvw_count;
        bool valid;
    } tally;

    // 디바이스 설정 (RX용)
    struct {
        uint8_t brightness;
        uint8_t camera_id;
        bool valid;
    } device;

    // 기능 정지 상태 (RX용)
    bool stopped;

    switcher_status_event_t switcher;  // EVT_SWITCHER_STATUS_CHANGED
    bool switcher_valid;

    network_status_event_t network;    // EVT_NETWORK_STATUS_CHANGED
    bool network_valid;

    lora_rf_event_t rf;               // EVT_RF_CHANGED
    bool rf_valid;
} display_data_t;

static struct {
    bool initialized;
    bool running;
    bool power_on;
    bool events_subscribed;    ///< 이벤트 구독 여부
    uint32_t refresh_interval_ms;
    uint32_t last_refresh_ms;
    uint32_t last_status_log_ms;  ///< 마지막 상태 로그 출력 시간

    display_page_t current_page;
    display_page_t previous_page;

    // 등록된 페이지 목록
    const display_page_interface_t* pages[MAX_PAGES];
    int page_count;

    // 통합 데이터 저장
    display_data_t data;
} s_mgr = {
    .initialized = false,
    .running = false,
    .power_on = true,
    .events_subscribed = false,
    .refresh_interval_ms = DEFAULT_REFRESH_INTERVAL_MS,
    .last_refresh_ms = 0,
    .last_status_log_ms = 0,
    .current_page = PAGE_NONE,
    .previous_page = PAGE_NONE,
    .pages = {nullptr},
    .page_count = 0,
    .data = {}
};

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief 현재 페이지 렌더링
 */
static void render_current_page(void)
{
    if (s_mgr.current_page == PAGE_NONE || s_mgr.current_page >= PAGE_COUNT) {
        return;
    }

    // 뮤텍스 획득 (전체 렌더링 사이클 보호)
    if (DisplayDriver_takeMutex(100) != ESP_OK) {
        T_LOGW(TAG, "뮤텍스 획득 실패 - 렌더링 스킵");
        return;
    }

    // 등록된 페이지에서 찾기
    for (int i = 0; i < s_mgr.page_count; i++) {
        if (s_mgr.pages[i]->id == s_mgr.current_page) {
            u8g2_t* u8g2 = display_manager_get_u8g2();
            if (u8g2 != nullptr) {
                DisplayDriver_clearBuffer();
                s_mgr.pages[i]->render(u8g2);
                DisplayDriver_sendBufferSync();  // 이미 뮤텍스 보유 중
            }
            break;
        }
    }

    DisplayDriver_giveMutex();
}

/**
 * @brief 페이지 전환 처리
 */
static void handle_page_transition(void)
{
    // 이전 페이지의 on_exit 호출
    if (s_mgr.previous_page != PAGE_NONE && s_mgr.previous_page != s_mgr.current_page) {
        for (int i = 0; i < s_mgr.page_count; i++) {
            if (s_mgr.pages[i]->id == s_mgr.previous_page) {
                if (s_mgr.pages[i]->on_exit != nullptr) {
                    s_mgr.pages[i]->on_exit();
                }
                break;
            }
        }
    }

    // 새 페이지의 on_enter 호출
    if (s_mgr.current_page != PAGE_NONE) {
        for (int i = 0; i < s_mgr.page_count; i++) {
            if (s_mgr.pages[i]->id == s_mgr.current_page) {
                if (s_mgr.pages[i]->on_enter != nullptr) {
                    s_mgr.pages[i]->on_enter();
                }
                break;
            }
        }
    }
}

/**
 * @brief 통합 상태 로그 출력
 *
 * 저장된 모든 데이터를 한 번에 출력
 */
static void print_status_log(void)
{
    T_LOGI(TAG, "──────────────────────────────────");

    // 시스템 정보
    if (s_mgr.data.system_valid) {
        T_LOGI(TAG, "ID:%s Bat:%d%% %.1fV %.0f°C Up:%us",
               s_mgr.data.system.device_id,
               s_mgr.data.system.battery,
               s_mgr.data.system.voltage,
               s_mgr.data.system.temperature,
               s_mgr.data.system.uptime);
    }

    // LoRa 정보
    if (s_mgr.data.lora.valid) {
        T_LOGI(TAG, "LoRa RSSI:%ddB SNR:%.0fdB",
               s_mgr.data.lora.rssi,
               s_mgr.data.lora.snr);
    }

#ifdef DEVICE_MODE_RX
    // Tally 정보 (RX)
    if (s_mgr.data.tally.valid) {
        // 채널 목록 문자열 생성
        char pgm_str[32] = {0};
        char pvw_str[32] = {0};
        int offset = 0;

        for (uint8_t i = 0; i < s_mgr.data.tally.pgm_count && i < 20; i++) {
            offset += snprintf(pgm_str + offset, sizeof(pgm_str) - offset,
                             "%s%d", (i > 0) ? "," : "", s_mgr.data.tally.pgm_channels[i]);
        }

        offset = 0;
        for (uint8_t i = 0; i < s_mgr.data.tally.pvw_count && i < 20; i++) {
            offset += snprintf(pvw_str + offset, sizeof(pvw_str) - offset,
                             "%s%d", (i > 0) ? "," : "", s_mgr.data.tally.pvw_channels[i]);
        }

        T_LOGI(TAG, "Tally PGM:[%s] PVW:[%s]",
               (s_mgr.data.tally.pgm_count > 0) ? pgm_str : "-",
               (s_mgr.data.tally.pvw_count > 0) ? pvw_str : "-");
    }

    // 디바이스 정보 (RX)
    if (s_mgr.data.device.valid) {
        T_LOGI(TAG, "Bri:%d Cam:%d",
               s_mgr.data.device.brightness,
               s_mgr.data.device.camera_id);
    }
#elif defined(DEVICE_MODE_TX)
    // Switcher 정보 (TX)
    if (s_mgr.data.switcher_valid) {
        if (s_mgr.data.switcher.dual_mode) {
            T_LOGI(TAG, "S1:%s@%s:%d %c | S2:%s@%s:%d %c",
                   s_mgr.data.switcher.s1_type,
                   s_mgr.data.switcher.s1_ip[0] ? s_mgr.data.switcher.s1_ip : "-",
                   s_mgr.data.switcher.s1_port,
                   s_mgr.data.switcher.s1_connected ? 'Y' : 'N',
                   s_mgr.data.switcher.s2_type,
                   s_mgr.data.switcher.s2_ip[0] ? s_mgr.data.switcher.s2_ip : "-",
                   s_mgr.data.switcher.s2_port,
                   s_mgr.data.switcher.s2_connected ? 'Y' : 'N');
        } else {
            T_LOGI(TAG, "S1:%s@%s:%d %c",
                   s_mgr.data.switcher.s1_type,
                   s_mgr.data.switcher.s1_ip[0] ? s_mgr.data.switcher.s1_ip : "-",
                   s_mgr.data.switcher.s1_port,
                   s_mgr.data.switcher.s1_connected ? 'Y' : 'N');
        }
    }

    // Network 정보 (TX)
    if (s_mgr.data.network_valid) {
        if (s_mgr.data.network.sta_connected) {
            T_LOGI(TAG, "WiFi:%s@%s | ETH:%s",
                   s_mgr.data.network.sta_ssid, s_mgr.data.network.sta_ip,
                   s_mgr.data.network.eth_connected ? s_mgr.data.network.eth_ip : "N/A");
        } else {
            T_LOGI(TAG, "WiFi:- | ETH:%s",
                   s_mgr.data.network.eth_connected ? s_mgr.data.network.eth_ip : "N/A");
        }
    }

    // RF 정보 (TX)
    if (s_mgr.data.rf_valid) {
        T_LOGI(TAG, "RF %.1fMHz Sync:0x%02X",
               s_mgr.data.rf.frequency,
               s_mgr.data.rf.sync_word);
    }
#endif
    T_LOGI(TAG, "──────────────────────────────────");
}

// ============================================================================
// 이벤트 핸들러
// ============================================================================

/**
 * @brief EVT_INFO_UPDATED 핸들러
 *
 * hardware_service에서 1초마다 발행하는 시스템 정보를 저장
 */
static esp_err_t on_info_updated(const event_data_t* event)
{
    if (!event) {
        return ESP_OK;
    }

    const system_info_event_t* info = (const system_info_event_t*)event->data;

    // 데이터 저장 (구조체 그대로 복사)
    s_mgr.data.system = *info;
    s_mgr.data.system_valid = true;

    // 페이지별 데이터 설정
#ifdef DEVICE_MODE_TX
    tx_page_set_device_id(info->device_id);
    tx_page_set_battery(info->battery);
    tx_page_set_voltage(info->voltage);
    tx_page_set_temperature(info->temperature);
#elif defined(DEVICE_MODE_RX)
    rx_page_set_device_id(info->device_id);
    rx_page_set_battery(info->battery);
    rx_page_set_voltage(info->voltage);
    rx_page_set_temperature(info->temperature);
#endif

    return ESP_OK;
}

/**
 * @brief EVT_LORA_RSSI_CHANGED 핸들러
 *
 * LoRa RSSI/SNR 변경 시 저장
 */
static esp_err_t on_lora_rssi_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_OK;
    }

    const lora_rssi_event_t* rssi_event = (const lora_rssi_event_t*)event->data;

    // 데이터 저장
    s_mgr.data.lora.rssi = rssi_event->rssi;
    s_mgr.data.lora.snr = (float)rssi_event->snr;
    s_mgr.data.lora.valid = true;

    // 페이지별 데이터 설정
#ifdef DEVICE_MODE_TX
    tx_page_set_rssi(rssi_event->rssi);
    tx_page_set_snr((float)rssi_event->snr);
#elif defined(DEVICE_MODE_RX)
    rx_page_set_rssi(rssi_event->rssi);
    rx_page_set_snr((float)rssi_event->snr);
#endif

    return ESP_OK;
}

#ifdef DEVICE_MODE_RX
/**
 * @brief EVT_LORA_RX_STATUS_CHANGED 핸들러 (RX 전용)
 *
 * LoRa RX 수신 통계 변경 시 RxPage에 업데이트
 */
static esp_err_t on_lora_rx_status_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_OK;
    }

    const lora_rx_status_event_t* rx_status = (const lora_rx_status_event_t*)event->data;

    // RxPage에 RX 통계 데이터 설정
    rx_page_set_rx_stats(rx_status->lastRssi, rx_status->lastSnr,
                         rx_status->interval, rx_status->totalCount);

    T_LOGD(TAG, "RX stats updated: RSSI=%d, SNR=%d, INTVL=%u, TOTAL=%u",
             rx_status->lastRssi, rx_status->lastSnr, rx_status->interval, rx_status->totalCount);

    return ESP_OK;
}
#endif // DEVICE_MODE_RX

#ifdef DEVICE_MODE_RX
/**
 * @brief EVT_TALLY_STATE_CHANGED 핸들러 (RX 전용)
 *
 * Tally 상태 변경 시 PGM/PVW 채널 추출하여 저장
 */
static esp_err_t on_tally_state_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_OK;
    }

    const tally_event_data_t* tally_evt = (const tally_event_data_t*)event->data;

    // packed_data_t 구조체 생성
    packed_data_t tally = {
        .data = (uint8_t*)tally_evt->tally_data,
        .data_size = static_cast<uint8_t>((tally_evt->channel_count + 3) / 4),
        .channel_count = tally_evt->channel_count
    };

    // PGM/PVW 채널 추출
    uint8_t pgm_channels[20];
    uint8_t pvw_channels[20];
    uint8_t pgm_count = 0;
    uint8_t pvw_count = 0;

    for (uint8_t i = 0; i < tally.channel_count && i < 20; i++) {
        uint8_t status = packed_data_get_channel(&tally, i + 1);
        if (status == TALLY_STATUS_PROGRAM || status == TALLY_STATUS_BOTH) {
            pgm_channels[pgm_count++] = i + 1;
        }
        if (status == TALLY_STATUS_PREVIEW || status == TALLY_STATUS_BOTH) {
            pvw_channels[pvw_count++] = i + 1;
        }
    }

    // 데이터 저장
    s_mgr.data.tally.pgm_count = pgm_count;
    s_mgr.data.tally.pvw_count = pvw_count;
    memcpy(s_mgr.data.tally.pgm_channels, pgm_channels, sizeof(uint8_t) * pgm_count);
    memcpy(s_mgr.data.tally.pvw_channels, pvw_channels, sizeof(uint8_t) * pvw_count);
    s_mgr.data.tally.valid = true;

    // RxPage에 PGM/PVW 채널 설정 및 즉시 갱신
    rx_page_set_pgm_channels(pgm_channels, pgm_count);
    rx_page_set_pvw_channels(pvw_channels, pvw_count);
    render_current_page();

    return ESP_OK;
}

/**
 * @brief EVT_CAMERA_ID_CHANGED 핸들러 (RX 전용)
 *
 * 카메라 ID 변경 시 RxPage에 반영
 */
static esp_err_t on_camera_id_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_OK;
    }

    const uint8_t* camera_id = (const uint8_t*)event->data;
    s_mgr.data.device.camera_id = *camera_id;
    s_mgr.data.device.valid = true;
    rx_page_set_cam_id(*camera_id);
    render_current_page();
    T_LOGI(TAG, "카메라 ID 변경 (디스플레이): %d", *camera_id);

    return ESP_OK;
}

/**
 * @brief EVT_BRIGHTNESS_CHANGED 핸들러 (RX 전용)
 *
 * 밝기 변경 시 저장 (상태 로그용)
 */
static esp_err_t on_brightness_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_OK;
    }

    const uint8_t* brightness = (const uint8_t*)event->data;
    s_mgr.data.device.brightness = *brightness;
    s_mgr.data.device.valid = true;
    T_LOGI(TAG, "밝기 변경 (디스플레이): %d", *brightness);

    return ESP_OK;
}

/**
 * @brief EVT_STOP_CHANGED 핸들러 (RX 전용)
 *
 * 기능 정지 상태 변경 시 처리
 */
static esp_err_t on_stop_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_OK;
    }

    const bool* stopped = (const bool*)event->data;
    s_mgr.data.stopped = *stopped;

    // RxPage에 정지 상태 전달
    rx_page_set_stopped(*stopped);

    if (*stopped) {
        T_LOGW(TAG, "기능 정지 상태 (디스플레이)");
    } else {
        T_LOGI(TAG, "기능 정지 해제 (디스플레이)");
    }

    // 즉시 갱신
    render_current_page();

    return ESP_OK;
}
#endif // DEVICE_MODE_RX

#ifdef DEVICE_MODE_TX
/**
 * @brief EVT_SWITCHER_STATUS_CHANGED 핸들러 (TX 전용)
 *
 * 스위처 상태 변경 시 저장
 */
static esp_err_t on_switcher_status_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_OK;
    }

    const switcher_status_event_t* sw = (const switcher_status_event_t*)event->data;

    // 데이터 저장 (구조체 그대로 복사)
    s_mgr.data.switcher = *sw;
    s_mgr.data.switcher_valid = true;

    // TxPage에 상태 설정
    tx_page_set_dual_mode(sw->dual_mode);
    tx_page_set_s1(sw->s1_type, sw->s1_ip, sw->s1_port, sw->s1_connected);
    if (sw->dual_mode) {
        tx_page_set_s2(sw->s2_type, sw->s2_ip, sw->s2_port, sw->s2_connected);
    }

    return ESP_OK;
}

/**
 * @brief EVT_NETWORK_STATUS_CHANGED 핸들러 (TX 전용)
 *
 * 네트워크 상태 변경 시 저장
 */
static esp_err_t on_network_status_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_OK;
    }

    const network_status_event_t* net = (const network_status_event_t*)event->data;

    // 데이터 저장 (구조체 그대로 복사)
    s_mgr.data.network = *net;
    s_mgr.data.network_valid = true;

    // TxPage에 상태 설정
    tx_page_set_ap_name(net->ap_ssid);
    tx_page_set_ap_ip(net->ap_ip);
    tx_page_set_wifi_ssid(net->sta_ssid);
    tx_page_set_wifi_ip(net->sta_ip);
    tx_page_set_wifi_connected(net->sta_connected);
    tx_page_set_eth_ip(net->eth_ip);
    display_manager_update_ethernet_dhcp_mode(net->eth_dhcp);
    tx_page_set_eth_connected(net->eth_connected);

    return ESP_OK;
}
#endif // DEVICE_MODE_TX

/**
 * @brief EVT_RF_CHANGED 핸들러 (TX/RX 공용)
 *
 * RF 설정 변경 시 저장
 */
static esp_err_t on_rf_changed(const event_data_t* event)
{
    if (!event) {
        return ESP_OK;
    }

    const lora_rf_event_t* rf = (const lora_rf_event_t*)event->data;

    // 데이터 저장
    s_mgr.data.rf.frequency = rf->frequency;
    s_mgr.data.rf.sync_word = rf->sync_word;
    s_mgr.data.rf_valid = true;

#ifdef DEVICE_MODE_TX
    tx_page_set_frequency(rf->frequency);
    tx_page_set_sync_word(rf->sync_word);
#elif defined(DEVICE_MODE_RX)
    rx_page_set_frequency(rf->frequency);
    rx_page_set_sync_word(rf->sync_word);
#endif

    return ESP_OK;
}

// ============================================================================
// 공개 API 구현
// ============================================================================

extern "C" bool display_manager_init(void)
{
    if (s_mgr.initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return true;
    }

    // DisplayDriver 초기화
    esp_err_t ret = DisplayDriver_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "DisplayDriver 초기화 실패: %s", esp_err_to_name(ret));
        return false;
    }

    memset(s_mgr.pages, 0, sizeof(s_mgr.pages));
    s_mgr.page_count = 0;
    s_mgr.current_page = PAGE_NONE;
    s_mgr.previous_page = PAGE_NONE;

    // BootPage 자동 등록
    boot_page_init();

    // 빌드 환경에 따라 TxPage 또는 RxPage 등록
#ifdef DEVICE_MODE_TX
    tx_page_init();
#elif defined(DEVICE_MODE_RX)
    rx_page_init();
#endif

    s_mgr.initialized = true;
    T_LOGI(TAG, "DisplayManager 초기화 완료");
    return true;
}

extern "C" void display_manager_start(void)
{
    if (!s_mgr.initialized) {
        T_LOGE(TAG, "초기화되지 않음");
        return;
    }

    // 이벤트 구독 (최초 1회)
    if (!s_mgr.events_subscribed) {
        // 공통 이벤트
        event_bus_subscribe(EVT_INFO_UPDATED, on_info_updated);
        event_bus_subscribe(EVT_LORA_RSSI_CHANGED, on_lora_rssi_changed);

#ifdef DEVICE_MODE_RX
        // RX 전용 이벤트
        event_bus_subscribe(EVT_TALLY_STATE_CHANGED, on_tally_state_changed);
        event_bus_subscribe(EVT_CAMERA_ID_CHANGED, on_camera_id_changed);
        event_bus_subscribe(EVT_BRIGHTNESS_CHANGED, on_brightness_changed);
        event_bus_subscribe(EVT_RF_CHANGED, on_rf_changed);
        event_bus_subscribe(EVT_STOP_CHANGED, on_stop_changed);
        event_bus_subscribe(EVT_LORA_RX_STATUS_CHANGED, on_lora_rx_status_changed);
#elif defined(DEVICE_MODE_TX)
        // TX 전용 이벤트
        event_bus_subscribe(EVT_SWITCHER_STATUS_CHANGED, on_switcher_status_changed);
        event_bus_subscribe(EVT_NETWORK_STATUS_CHANGED, on_network_status_changed);
        event_bus_subscribe(EVT_RF_CHANGED, on_rf_changed);
#endif

        s_mgr.events_subscribed = true;
        T_LOGI(TAG, "이벤트 구독 완료: EVT_INFO_UPDATED, EVT_LORA_RSSI_CHANGED"
#ifdef DEVICE_MODE_RX
               ", EVT_TALLY_STATE_CHANGED, EVT_CAMERA_ID_CHANGED, EVT_BRIGHTNESS_CHANGED, EVT_RF_CHANGED, EVT_STOP_CHANGED, EVT_LORA_RX_STATUS_CHANGED"
#elif defined(DEVICE_MODE_TX)
               ", EVT_SWITCHER_STATUS_CHANGED, EVT_NETWORK_STATUS_CHANGED, EVT_RF_CHANGED"
#endif
        );
    }

    s_mgr.running = true;
    T_LOGI(TAG, "DisplayManager 시작");
}

extern "C" void display_manager_set_refresh_interval(uint32_t interval_ms)
{
    s_mgr.refresh_interval_ms = interval_ms;
}

extern "C" bool display_manager_register_page(const display_page_interface_t* page_interface)
{
    if (page_interface == nullptr) {
        T_LOGE(TAG, "페이지 인터페이스가 nullptr임");
        return false;
    }

    if (s_mgr.page_count >= MAX_PAGES) {
        T_LOGE(TAG, "페이지 등록 한도 도달 (%d)", MAX_PAGES);
        return false;
    }

    // 중복 확인
    for (int i = 0; i < s_mgr.page_count; i++) {
        if (s_mgr.pages[i]->id == page_interface->id) {
            T_LOGW(TAG, "페이지 ID %d 이미 등록됨", page_interface->id);
            return false;
        }
    }

    s_mgr.pages[s_mgr.page_count++] = page_interface;
    T_LOGI(TAG, "페이지 등록: %s (ID=%d)", page_interface->name, page_interface->id);

    // 페이지 초기화
    if (page_interface->init != nullptr) {
        page_interface->init();
    }

    return true;
}

extern "C" void display_manager_set_page(display_page_t page_id)
{
    if (!s_mgr.initialized) {
        return;
    }

    if (s_mgr.current_page == page_id) {
        return;  // 이미 같은 페이지
    }

    s_mgr.previous_page = s_mgr.current_page;
    s_mgr.current_page = page_id;

    T_LOGD(TAG, "페이지 전환: %d -> %d", s_mgr.previous_page, s_mgr.current_page);

    handle_page_transition();
    // 페이지 전환 시 즉시 렌더링 (플래그 방식 사용 안 함)
    render_current_page();
}

extern "C" display_page_t display_manager_get_current_page(void)
{
    return s_mgr.current_page;
}

extern "C" void display_manager_force_refresh(void)
{
    if (!s_mgr.initialized || !s_mgr.power_on) {
        return;
    }

    // 즉시 렌더링
    render_current_page();
}

extern "C" void display_manager_set_power(bool on)
{
    s_mgr.power_on = on;
    DisplayDriver_setPower(on);

    if (on && s_mgr.initialized) {
        display_manager_force_refresh();
    }

    T_LOGI(TAG, "디스플레이 전원: %s", on ? "ON" : "OFF");
}

extern "C" u8g2_t* display_manager_get_u8g2(void)
{
    return DisplayDriver_getU8g2();
}

// ============================================================================
// BootPage 편의 API 구현
// ============================================================================

extern "C" void display_manager_boot_set_message(const char* message)
{
    boot_page_set_message(message);
}

extern "C" void display_manager_boot_set_progress(uint8_t progress)
{
    boot_page_set_progress(progress);
}

/**
 * @brief 디스플레이 갱신 루프 (주기적으로 호출해야 함)
 *
 * @note 이 함수는 메인 루프나 타이머에서 주기적으로 호출되어야 함
 */
extern "C" void display_manager_update(void)
{
    if (!s_mgr.initialized || !s_mgr.running || !s_mgr.power_on) {
        return;
    }

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 갱신 주기 체크
    if (now - s_mgr.last_refresh_ms >= s_mgr.refresh_interval_ms) {
        s_mgr.last_refresh_ms = now;
        render_current_page();
    }

    // 상태 로그 주기 체크 (5초마다)
    if (now - s_mgr.last_status_log_ms >= STATUS_LOG_INTERVAL_MS) {
        s_mgr.last_status_log_ms = now;
        print_status_log();
    }
}

// ============================================================================
// 부팅 완료 후 페이지 전환
// ============================================================================

/**
 * @brief 빌드 환경에 따른 기본 페이지 가져오기
 *
 * @return PAGE_TX (DEVICE_MODE_TX) 또는 PAGE_RX (DEVICE_MODE_RX)
 */
static display_page_t get_default_page(void)
{
#ifdef DEVICE_MODE_TX
    return PAGE_TX;
#elif defined(DEVICE_MODE_RX)
    return PAGE_RX;
#else
    return PAGE_BOOT;  // 기본값
#endif
}

/**
 * @brief 부팅 완료 후 기본 페이지로 전환
 *
 * BootPage에서 TX/RX 페이지로 자동 전환
 */
extern "C" void display_manager_boot_complete(void)
{
    display_page_t default_page = get_default_page();

    T_LOGD(TAG, "부팅 완료 -> 페이지 전환: %d", default_page);
    display_manager_set_page(default_page);
}

// ============================================================================
// 페이지 편의 API 구현 (TX/RX 공통)
// ============================================================================

// 페이지별 함수 매크로 (빌드 모드에 따라 TxPage 또는 RxPage 함수 호출)
#ifdef DEVICE_MODE_TX
    #define PAGE_GET_CURRENT()   tx_page_get_current_page()
    #define PAGE_SWITCH(idx)      tx_page_switch_page(idx)
#elif defined(DEVICE_MODE_RX)
    #define PAGE_GET_CURRENT()   rx_page_get_current_page()
    #define PAGE_SWITCH(idx)      rx_page_switch_page(idx)
#endif

extern "C" uint8_t display_manager_get_page_index(void)
{
    return PAGE_GET_CURRENT();
}

extern "C" void display_manager_switch_page(uint8_t index)
{
    PAGE_SWITCH(index);
}

// 매크로 정의 해제
#undef PAGE_GET_CURRENT
#undef PAGE_SWITCH

// ============================================================================
// RxPage 전용 API 구현 (DEVICE_MODE_RX일 때만)
// ============================================================================

#ifdef DEVICE_MODE_RX

extern "C" void display_manager_set_cam_id(uint8_t cam_id)
{
    rx_page_set_cam_id(cam_id);
}

extern "C" int display_manager_get_state(void)
{
    return (int)rx_page_get_state();
}

extern "C" void display_manager_show_camera_id_popup(uint8_t max_camera_num)
{
    rx_page_show_camera_id_popup_with_max(max_camera_num);
}

extern "C" void display_manager_hide_camera_id_popup(void)
{
    rx_page_hide_camera_id_popup();
}

extern "C" void display_manager_set_camera_id_changing(bool changing)
{
    rx_page_set_camera_id_changing(changing);
}

extern "C" bool display_manager_is_camera_id_changing(void)
{
    return rx_page_is_camera_id_changing();
}

extern "C" uint8_t display_manager_get_display_camera_id(void)
{
    return rx_page_get_display_camera_id();
}

extern "C" uint8_t display_manager_cycle_camera_id(uint8_t max_camera_num)
{
    return rx_page_cycle_camera_id(max_camera_num);
}

#endif // DEVICE_MODE_RX

// ============================================================================
// System 데이터 업데이트 API
// ============================================================================

// 페이지별 함수 매크로 (빌드 모드에 따라 TxPage 또는 RxPage 함수 호출)
#ifdef DEVICE_MODE_TX
    #define PAGE_SET_DEVICE_ID(id)    tx_page_set_device_id(id)
    #define PAGE_SET_BATTERY(pct)     tx_page_set_battery(pct)
    #define PAGE_SET_VOLTAGE(vol)     tx_page_set_voltage(vol)
    #define PAGE_SET_TEMPERATURE(t)   tx_page_set_temperature(t)
#elif defined(DEVICE_MODE_RX)
    #define PAGE_SET_DEVICE_ID(id)    rx_page_set_device_id(id)
    #define PAGE_SET_BATTERY(pct)     rx_page_set_battery(pct)
    #define PAGE_SET_VOLTAGE(vol)     rx_page_set_voltage(vol)
    #define PAGE_SET_TEMPERATURE(t)   rx_page_set_temperature(t)
#endif

extern "C" void display_manager_update_system(const char* device_id, uint8_t battery,
                                               float voltage, float temperature)
{
    PAGE_SET_DEVICE_ID(device_id);
    PAGE_SET_BATTERY(battery);
    PAGE_SET_VOLTAGE(voltage);
    PAGE_SET_TEMPERATURE(temperature);
}

// 매크로 정의 해제
#undef PAGE_SET_DEVICE_ID
#undef PAGE_SET_BATTERY
#undef PAGE_SET_VOLTAGE
#undef PAGE_SET_TEMPERATURE

// ============================================================================
// RSSI/SNR 업데이트 API
// ============================================================================

// 페이지별 RSSI/SNR 매크로
#ifdef DEVICE_MODE_TX
    #define PAGE_SET_RSSI(rssi)       tx_page_set_rssi(rssi)
    #define PAGE_SET_SNR(snr)         tx_page_set_snr(snr)
#elif defined(DEVICE_MODE_RX)
    #define PAGE_SET_RSSI(rssi)       rx_page_set_rssi(rssi)
    #define PAGE_SET_SNR(snr)         rx_page_set_snr(snr)
#endif

extern "C" void display_manager_update_rssi(int16_t rssi, float snr)
{
    PAGE_SET_RSSI(rssi);
    PAGE_SET_SNR(snr);
}

// 매크로 정의 해제
#undef PAGE_SET_RSSI
#undef PAGE_SET_SNR

// ============================================================================
// PGM/PVW 채널 업데이트 (RX 모드만)
// ============================================================================

#ifdef DEVICE_MODE_RX

extern "C" void display_manager_update_tally(const uint8_t* pgm_channels, uint8_t pgm_count,
                                              const uint8_t* pvw_channels, uint8_t pvw_count)
{
    if (pgm_channels && pgm_count > 0) {
        rx_page_set_pgm_channels(pgm_channels, pgm_count);
    } else {
        // PGM 채널 없으면 빈 배열로 설정
        rx_page_set_pgm_channels(nullptr, 0);
    }

    if (pvw_channels && pvw_count > 0) {
        rx_page_set_pvw_channels(pvw_channels, pvw_count);
    } else {
        // PVW 채널 없으면 빈 배열로 설정
        rx_page_set_pvw_channels(nullptr, 0);
    }

    // PGM/PVW 리스트는 즉시 갱신
    render_current_page();
}

#endif // DEVICE_MODE_RX

// ============================================================================
// Ethernet DHCP 모드 업데이트 (TX 전용)
// ============================================================================

#ifdef DEVICE_MODE_TX

extern "C" void display_manager_update_ethernet_dhcp_mode(bool dhcp_mode)
{
    tx_page_set_eth_dhcp_mode(dhcp_mode);
}

#endif // DEVICE_MODE_TX
