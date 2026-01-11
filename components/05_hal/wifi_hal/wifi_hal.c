/**
 * @file wifi_hal.c
 * @brief WiFi HAL 구현
 *
 * ESP32-S3 WiFi 스택을 래핑하는 HAL 계층입니다.
 * - AP 모드: 액세스 포인트 기능
 * - STA 모드: 스테이션 기능 (연결, IP 획득)
 * - 스캔 기능: 주변 AP 스캔
 * - 이벤트 처리: WiFi/IP 이벤트 핸들링
 *
 * @note NVS 비활성화로 설정을 RAM에 저장하여 속도 향상
 * @note netif는 deinit 시 해제하지 않아 LwIP 충돌 방지
 */

#include "wifi_hal.h"
#include "t_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/dns.h"  // LwIP DNS 설정

static const char* TAG = "05_WiFi";

// ============================================================================
// 상수 정의
// ============================================================================

/**
 * @brief WiFi 이벤트 그룹 비트
 *
 * FreeRTOS 이벤트 그룹을 사용하여 비동기 이벤트를 처리합니다.
 */
#define WIFI_HAL_STARTED_BIT   BIT0  ///< WiFi 시작 완료
#define WIFI_HAL_STOPPED_BIT   BIT1  ///< WiFi 정지 완료
#define WIFI_HAL_CONNECTED_BIT BIT2  ///< STA 연결 완료 (IP 획득)
#define WIFI_HAL_SCAN_DONE_BIT BIT3  ///< 스캔 완료

/**
 * @brief DNS 서버 주소
 *
 * Google DNS (8.8.8.8)와 Cloudflare DNS (1.1.1.1) 사용
 */
#define DNS_PRIMARY_ADDR   "8.8.8.8"
#define DNS_BACKUP_ADDR    "1.1.1.1"

// ============================================================================
// 내부 상태 변수
// ============================================================================

/** 초기화 완료 여부 */
static bool s_initialized = false;

/** 현재 WiFi 상태 */
static wifi_hal_state_t s_state = WIFI_HAL_STATE_IDLE;

/** AP 모드 netif 핸들 */
static esp_netif_t* s_netif_ap = NULL;

/** STA 모드 netif 핸들 */
static esp_netif_t* s_netif_sta = NULL;

/** 사용자 정의 이벤트 콜백 */
static wifi_hal_event_callback_t s_event_callback = NULL;

/** 비동기 이벤트 처리를 위한 이벤트 그룹 */
static EventGroupHandle_t s_event_group = NULL;

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief WiFi/IP 이벤트 핸들러
 *
 * ESP-IDF WiFi 및 IP 이벤트를 처리하고 상태를 업데이트합니다.
 *
 * @param arg 사용자 데이터 (콜백 전달용)
 * @param event_base 이벤트 베이스 (WIFI_EVENT, IP_EVENT)
 * @param event_id 이벤트 ID
 * @param event_data 이벤트 데이터
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                T_LOGD(TAG, "WIFI_EVENT: STA started");
                break;

            case WIFI_EVENT_STA_STOP:
                T_LOGD(TAG, "WIFI_EVENT: STA stopped");
                if (s_event_group) {
                    xEventGroupClearBits(s_event_group, WIFI_HAL_CONNECTED_BIT);
                }
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                T_LOGW(TAG, "WIFI_EVENT: STA disconnected");
                if (s_event_group) {
                    xEventGroupClearBits(s_event_group, WIFI_HAL_CONNECTED_BIT);
                }
                break;

            case WIFI_EVENT_AP_START:
                T_LOGD(TAG, "WIFI_EVENT: AP started");
                if (s_event_group) {
                    xEventGroupSetBits(s_event_group, WIFI_HAL_STARTED_BIT);
                }
                break;

            case WIFI_EVENT_AP_STOP:
                T_LOGD(TAG, "WIFI_EVENT: AP stopped");
                if (s_event_group) {
                    xEventGroupClearBits(s_event_group, WIFI_HAL_STARTED_BIT);
                }
                break;

            case WIFI_EVENT_SCAN_DONE:
                T_LOGD(TAG, "WIFI_EVENT: Scan done");
                if (s_event_group) {
                    xEventGroupSetBits(s_event_group, WIFI_HAL_SCAN_DONE_BIT);
                }
                break;

            default:
                // 기타 WiFi 이벤트는 처리하지 않음
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            T_LOGI(TAG, "IP_EVENT: STA got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, WIFI_HAL_CONNECTED_BIT);
            }
        }
    }

    // 사용자 콜백 호출 (상위 계층에서 추가 처리 가능)
    if (s_event_callback) {
        s_event_callback(arg, event_base, event_id, event_data);
    }
}

// ============================================================================
// 공개 API 구현
// ============================================================================

/**
 * @brief WiFi HAL 초기화
 *
 * WiFi 스택을 초기화하고 이벤트 핸들러를 등록합니다.
 * NVS 비활성화로 설정을 RAM에 저장하여 부팅 속도를 향상시킵니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_hal_init(void)
{
    if (s_initialized) {
        T_LOGD(TAG, "Already initialized");
        return ESP_OK;
    }

    T_LOGI(TAG, "Initializing WiFi HAL");

    // 이벤트 그룹 생성 (비동기 이벤트 처리용)
    if (s_event_group == NULL) {
        s_event_group = xEventGroupCreate();
        if (!s_event_group) {
            T_LOGE(TAG, "Failed to create event group");
            return ESP_ERR_NO_MEM;
        }
        T_LOGD(TAG, "Event group created");
    }

    // WiFi 기본 설정 (NVS 비활성화)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 0;  // NVS 비활성화 (설정을 RAM에 저장)

    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Failed to init WiFi: %s (0x%x)", esp_err_to_name(ret), ret);
        return ret;
    }
    T_LOGD(TAG, "WiFi initialized (NVS disabled)");

    // 기본 이벤트 루프 생성 (이미 생성된 경우 무시)
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "Failed to create event loop: %s (0x%x)", esp_err_to_name(ret), ret);
        return ret;
    }

    // WiFi 이벤트 핸들러 등록
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Failed to register WiFi event handler: %s (0x%x)", esp_err_to_name(ret), ret);
        return ret;
    }

    // IP 이벤트 핸들러 등록
    ret = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                     &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Failed to register IP event handler: %s (0x%x)", esp_err_to_name(ret), ret);
        return ret;
    }
    T_LOGD(TAG, "Event handlers registered");

    s_initialized = true;
    s_state = WIFI_HAL_STATE_IDLE;

    T_LOGI(TAG, "WiFi HAL initialized successfully");
    return ESP_OK;
}

/**
 * @brief WiFi HAL 해제
 *
 * WiFi 스택을 정리합니다.
 * netif는 해제하지 않아 LwIP 스택과의 충돌을 방지합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_hal_deinit(void)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Deinitializing WiFi HAL");

    // WiFi 정지
    esp_wifi_stop();

    // WiFi 해제
    esp_wifi_deinit();

    // netif는 해제하지 않음 (LwIP 스택이 계속 참조할 수 있음)
    // 재초기화 시 기존 netif를 재사용하여 문제 방지
    T_LOGD(TAG, "Netifs preserved (AP=%p, STA=%p)", (void*)s_netif_ap, (void*)s_netif_sta);

    // 이벤트 그룹 해제
    if (s_event_group) {
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
    }

    s_initialized = false;
    s_state = WIFI_HAL_STATE_STOPPED;

    T_LOGI(TAG, "WiFi HAL deinitialized");
    return ESP_OK;
}

// ============================================================================
// netif 생성
// ============================================================================

/**
 * @brief AP 모드 netif 생성
 *
 * WiFi AP 모드를 위한 네트워크 인터페이스를 생성합니다.
 * 이미 생성된 경우 기존 netif를 반환합니다.
 *
 * @return netif 핸들 (실패 시 NULL)
 */
void* wifi_hal_create_ap_netif(void)
{
    if (s_netif_ap) {
        T_LOGW(TAG, "AP netif already created (%p)", (void*)s_netif_ap);
        return s_netif_ap;
    }

    T_LOGI(TAG, "Creating AP netif");

    // ESP-IDF 5.5.0: 기본 AP netif 생성
    s_netif_ap = esp_netif_create_default_wifi_ap();

    if (s_netif_ap) {
        T_LOGI(TAG, "AP netif created (%p)", (void*)s_netif_ap);
    } else {
        T_LOGE(TAG, "Failed to create AP netif");
    }

    return s_netif_ap;
}

/**
 * @brief STA 모드 netif 생성
 *
 * WiFi STA 모드를 위한 네트워크 인터페이스를 생성합니다.
 * 이미 생성된 경우 기존 netif를 반환합니다.
 * DNS 서버를 미리 설정하여 DHCP 시작 전에 적용되도록 합니다.
 *
 * @return netif 핸들 (실패 시 NULL)
 */
void* wifi_hal_create_sta_netif(void)
{
    if (s_netif_sta) {
        T_LOGW(TAG, "STA netif already created (%p)", (void*)s_netif_sta);
        return s_netif_sta;
    }

    T_LOGI(TAG, "Creating STA netif");

    // ESP-IDF 5.5.0: 기본 STA netif 생성
    s_netif_sta = esp_netif_create_default_wifi_sta();

    if (!s_netif_sta) {
        T_LOGE(TAG, "Failed to create STA netif");
        return NULL;
    }

    // DNS 서버 미리 설정 (DHCP 시작 전에 설정해야 클리어되지 않음)
    ip_addr_t dns_primary, dns_backup;
    dns_primary.u_addr.ip4.addr = esp_ip4addr_aton(DNS_PRIMARY_ADDR);
    dns_primary.type = IPADDR_TYPE_V4;
    dns_backup.u_addr.ip4.addr = esp_ip4addr_aton(DNS_BACKUP_ADDR);
    dns_backup.type = IPADDR_TYPE_V4;

    dns_setserver(0, &dns_primary);
    dns_setserver(1, &dns_backup);

    T_LOGI(TAG, "STA netif created (%p, DNS: %s, %s)",
            (void*)s_netif_sta, DNS_PRIMARY_ADDR, DNS_BACKUP_ADDR);

    return s_netif_sta;
}

// ============================================================================
// 이벤트 핸들러 등록
// ============================================================================

/**
 * @brief 사용자 정의 이벤트 콜백 등록
 *
 * WiFi/IP 이벤트 발생 시 호출될 콜백 함수를 등록합니다.
 *
 * @param callback 콜백 함수 (NULL 해제)
 * @return ESP_OK 성공
 */
esp_err_t wifi_hal_register_event_handler(wifi_hal_event_callback_t callback)
{
    s_event_callback = callback;
    T_LOGD(TAG, "Event callback registered: %s", callback ? "set" : "cleared");
    return ESP_OK;
}

// ============================================================================
// WiFi 제어
// ============================================================================

/**
 * @brief WiFi 시작
 *
 * 설정된 모드(AP/STA/APSTA)로 WiFi를 시작합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_hal_start(void)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Starting WiFi");
    esp_err_t ret = esp_wifi_start();
    if (ret == ESP_OK) {
        s_state = WIFI_HAL_STATE_STARTED;
        T_LOGI(TAG, "WiFi started (state=%d)", s_state);
    } else {
        T_LOGE(TAG, "Failed to start WiFi: %s (0x%x)", esp_err_to_name(ret), ret);
    }
    return ret;
}

/**
 * @brief WiFi 정지
 *
 * WiFi를 정지합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_hal_stop(void)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Stopping WiFi");
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_OK) {
        s_state = WIFI_HAL_STATE_STOPPED;
        T_LOGI(TAG, "WiFi stopped (state=%d)", s_state);
    } else {
        T_LOGE(TAG, "Failed to stop WiFi: %s (0x%x)", esp_err_to_name(ret), ret);
    }
    return ret;
}

/**
 * @brief STA 연결 시도
 *
 * 설정된 AP에 연결을 시도합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_hal_connect(void)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Requesting STA connection");
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Failed to request connection: %s (0x%x)", esp_err_to_name(ret), ret);
    }
    return ret;
}

/**
 * @brief STA 연결 해제
 *
 * AP 연결을 해제합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_hal_disconnect(void)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Disconnecting STA");
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        T_LOGW(TAG, "Disconnect failed: %s (0x%x)", esp_err_to_name(ret), ret);
    }
    return ret;
}

// ============================================================================
// 설정
// ============================================================================

/**
 * @brief WiFi 설정 적용
 *
 * AP 또는 STA 설정을 적용합니다.
 *
 * @param iface WiFi 인터페이스 (WIFI_IF_AP, WIFI_IF_STA)
 * @param config WiFi 설정 구조체
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_hal_set_config(wifi_interface_t iface, const void* config)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (config == NULL) {
        T_LOGE(TAG, "Invalid parameter: config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // ESP-IDF 5.5.0: esp_wifi_set_config는 const가 아닌 포인터를 받음
    T_LOGD(TAG, "Setting WiFi config (iface=%d)", iface);
    esp_err_t ret = esp_wifi_set_config(iface, (wifi_config_t*)config);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Failed to set config: %s (0x%x)", esp_err_to_name(ret), ret);
    }
    return ret;
}

/**
 * @brief WiFi 설정 조회
 *
 * 현재 AP 또는 STA 설정을 조회합니다.
 *
 * @param iface WiFi 인터페이스 (WIFI_IF_AP, WIFI_IF_STA)
 * @param config 설정을 저장할 버퍼
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_hal_get_config(wifi_interface_t iface, void* config)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (config == NULL) {
        T_LOGE(TAG, "Invalid parameter: config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    T_LOGD(TAG, "Getting WiFi config (iface=%d)", iface);
    return esp_wifi_get_config(iface, (wifi_config_t*)config);
}

// ============================================================================
// 스캔
// ============================================================================

/**
 * @brief WiFi 스캔 시작
 *
 * 주변 AP를 스캔합니다.
 *
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_hal_scan_start(void)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    T_LOGI(TAG, "Starting WiFi scan");

    // 이벤트 그룹 비트 클리어
    if (s_event_group) {
        xEventGroupClearBits(s_event_group, WIFI_HAL_SCAN_DONE_BIT);
    }

    // 스캔 설정 (활성화 모드, 숨겨진 AP도 표시)
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,           // 모든 채널 스캔
        .show_hidden = true,     // 숨겨진 AP도 표시
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Failed to start scan: %s (0x%x)", esp_err_to_name(ret), ret);
    }
    return ret;
}

/**
 * @brief 스캔 결과 조회
 *
 * 스캔된 AP 레코드를 가져옵니다.
 *
 * @param ap_records AP 레코드를 저장할 버퍼
 * @param max_count 최대 레코드 수
 * @param out_count 실제 레코드 수 (NULL 가능)
 * @return ESP_OK 성공, 에러 코드 실패
 */
esp_err_t wifi_hal_scan_get_results(void* ap_records, uint16_t max_count, uint16_t* out_count)
{
    if (!s_initialized) {
        T_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (ap_records == NULL) {
        T_LOGE(TAG, "Invalid parameter: ap_records is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    T_LOGD(TAG, "Getting scan results (max_count=%u)", max_count);

    // ESP-IDF 5.5.0: number 파라미터가 입력/값으로 사용됨
    uint16_t number = max_count;
    esp_err_t ret = esp_wifi_scan_get_ap_records(&number, (wifi_ap_record_t*)ap_records);
    if (ret == ESP_OK) {
        T_LOGI(TAG, "Scan results: %u AP(s) found", number);
        if (out_count) {
            *out_count = number;
        }
    } else {
        T_LOGE(TAG, "Failed to get scan results: %s (0x%x)", esp_err_to_name(ret), ret);
    }
    return ret;
}

// ============================================================================
// 상태 조회
// ============================================================================

/**
 * @brief WiFi 상태 조회
 *
 * 현재 WiFi 상태를 반환합니다.
 *
 * @return WiFi 상태
 */
wifi_hal_state_t wifi_hal_get_state(void)
{
    return s_state;
}

/**
 * @brief 초기화 여부 확인
 *
 * @return true 초기화됨, false 초기화 안됨
 */
bool wifi_hal_is_initialized(void)
{
    return s_initialized;
}
