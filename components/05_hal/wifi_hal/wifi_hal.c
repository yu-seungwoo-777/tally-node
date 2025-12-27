/**
 * @file WiFiHal.c
 * @brief WiFi HAL 구현
 */

#include "wifi_hal.h"
#include "t_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char* TAG = "WiFiHal";

// ============================================================================
// 정적 변수
// ============================================================================

static bool s_initialized = false;
static wifi_hal_state_t s_state = WIFI_HAL_STATE_IDLE;
static esp_netif_t* s_netif_ap = NULL;
static esp_netif_t* s_netif_sta = NULL;
static wifi_hal_event_callback_t s_event_callback = NULL;
static EventGroupHandle_t s_event_group = NULL;

// 이벤트 비트
#define WIFI_HAL_STARTED_BIT   BIT0
#define WIFI_HAL_STOPPED_BIT   BIT1
#define WIFI_HAL_CONNECTED_BIT BIT2
#define WIFI_HAL_SCAN_DONE_BIT BIT3

// ============================================================================
// 이벤트 핸들러
// ============================================================================

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        T_LOGD(TAG, "WiFi STA 시작됨");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
        T_LOGD(TAG, "WiFi STA 정지됨");
        if (s_event_group) {
            xEventGroupClearBits(s_event_group, WIFI_HAL_CONNECTED_BIT);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        T_LOGW(TAG, "WiFi STA 연결 해제됨");
        if (s_event_group) {
            xEventGroupClearBits(s_event_group, WIFI_HAL_CONNECTED_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        T_LOGI(TAG, "WiFi STA IP 획득: " IPSTR, IP2STR(&event->ip_info.ip));
        if (s_event_group) {
            xEventGroupSetBits(s_event_group, WIFI_HAL_CONNECTED_BIT);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        T_LOGI(TAG, "WiFi AP 시작됨");
        if (s_event_group) {
            xEventGroupSetBits(s_event_group, WIFI_HAL_STARTED_BIT);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        T_LOGI(TAG, "WiFi AP 정지됨");
        if (s_event_group) {
            xEventGroupClearBits(s_event_group, WIFI_HAL_STARTED_BIT);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        T_LOGD(TAG, "WiFi 스캔 완료");
        if (s_event_group) {
            xEventGroupSetBits(s_event_group, WIFI_HAL_SCAN_DONE_BIT);
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

esp_err_t wifi_hal_init(void)
{
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    T_LOGI(TAG, "WiFi HAL 초기화 중...");

    // 이벤트 그룹 생성
    s_event_group = xEventGroupCreate();
    if (!s_event_group) {
        T_LOGE(TAG, "이벤트 그룹 생성 실패");
        return ESP_ERR_NO_MEM;
    }

    // 기본 WiFi 초기화
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // NVS 비활성화 (속도 향상) - RAM에 설정 저장
    cfg.nvs_enable = 0;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 이벤트 루프 생성 (이미 생성된 경우 무시)
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "이벤트 루프 생성 실패: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                     &wifi_event_handler, NULL));

    // WiFi 슬립 모드 비활성화 (최고 성능)
    esp_wifi_set_ps(WIFI_PS_NONE);

    // 802.11bgn 프로토콜 설정 (최고 속도)
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

    s_initialized = true;
    s_state = WIFI_HAL_STATE_IDLE;

    T_LOGI(TAG, "WiFi HAL 초기화 완료");
    return ESP_OK;
}

esp_err_t wifi_hal_deinit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "WiFi HAL 정리 중...");

    esp_wifi_stop();
    esp_wifi_deinit();
    vEventGroupDelete(s_event_group);
    s_event_group = NULL;

    s_initialized = false;
    s_state = WIFI_HAL_STATE_STOPPED;

    T_LOGI(TAG, "WiFi HAL 정리 완료");
    return ESP_OK;
}

// ============================================================================
// netif 생성
// ============================================================================

void* wifi_hal_create_ap_netif(void)
{
    if (s_netif_ap) {
        T_LOGW(TAG, "AP netif 이미 생성됨");
        return s_netif_ap;
    }

    // ESP-IDF 5.5.0: esp_netif_create_default_wifi_ap() 사용
    s_netif_ap = esp_netif_create_default_wifi_ap();

    T_LOGI(TAG, "AP netif 생성 완료");
    return s_netif_ap;
}

void* wifi_hal_create_sta_netif(void)
{
    if (s_netif_sta) {
        T_LOGW(TAG, "STA netif 이미 생성됨");
        return s_netif_sta;
    }

    // ESP-IDF 5.5.0: esp_netif_create_default_wifi_sta() 사용
    s_netif_sta = esp_netif_create_default_wifi_sta();

    T_LOGI(TAG, "STA netif 생성 완료");
    return s_netif_sta;
}

// ============================================================================
// 이벤트 핸들러
// ============================================================================

esp_err_t wifi_hal_register_event_handler(wifi_hal_event_callback_t callback)
{
    s_event_callback = callback;
    return ESP_OK;
}

// ============================================================================
// WiFi 제어
// ============================================================================

esp_err_t wifi_hal_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "WiFi 시작 중...");
    esp_err_t ret = esp_wifi_start();
    if (ret == ESP_OK) {
        s_state = WIFI_HAL_STATE_STARTED;
    }
    return ret;
}

esp_err_t wifi_hal_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "WiFi 정지 중...");
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_OK) {
        s_state = WIFI_HAL_STATE_STOPPED;
    }
    return ret;
}

esp_err_t wifi_hal_connect(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "WiFi STA 연결 시도...");
    return esp_wifi_connect();
}

esp_err_t wifi_hal_disconnect(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "WiFi STA 연결 해제");
    return esp_wifi_disconnect();
}

// ============================================================================
// 설정
// ============================================================================

esp_err_t wifi_hal_set_config(wifi_interface_t iface, const void* config)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // ESP-IDF 5.5.0: esp_wifi_set_config는 const가 아닌 포인터를 받음
    return esp_wifi_set_config(iface, (wifi_config_t*)config);
}

esp_err_t wifi_hal_get_config(wifi_interface_t iface, void* config)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_wifi_get_config(iface, (wifi_config_t*)config);
}

// ============================================================================
// 스캔
// ============================================================================

esp_err_t wifi_hal_scan_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // 이벤트 그룹 비트 클리어
    if (s_event_group) {
        xEventGroupClearBits(s_event_group, WIFI_HAL_SCAN_DONE_BIT);
    }

    T_LOGI(TAG, "WiFi 스캔 시작...");
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    return esp_wifi_scan_start(&scan_config, false);
}

esp_err_t wifi_hal_scan_get_results(void* ap_records, uint16_t max_count, uint16_t* out_count)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // ESP-IDF 5.5.0: number 파라미터가 입력/값으로 사용됨
    uint16_t number = max_count;
    esp_err_t ret = esp_wifi_scan_get_ap_records(&number, (wifi_ap_record_t*)ap_records);
    if (ret == ESP_OK && out_count) {
        *out_count = number;
    }
    return ret;
}

// ============================================================================
// 상태 조회
// ============================================================================

wifi_hal_state_t wifi_hal_get_state(void)
{
    return s_state;
}

bool wifi_hal_is_initialized(void)
{
    return s_initialized;
}
