/**
 * @file license_service.cpp
 * @brief 라이센스 상태 관리 서비스 구현 (간소화 버전)
 */

#include "license_service.h"
#include "license_client.h"
#include "event_bus.h"
#include "t_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cstring>

static const char* TAG = "LicenseService";

// ============================================================================
// 상수 정의
// ============================================================================

#define NVS_NAMESPACE "license"

// ============================================================================
// LicenseService 클래스 (싱글톤)
// ============================================================================

class LicenseService {
public:
    static esp_err_t init(void);
    static esp_err_t start(void);
    static void stop(void);
    static esp_err_t validate(const char* key);
    static esp_err_t searchLicense(const char* name, const char* phone,
                                  const char* email, char* out_response,
                                  size_t response_size);

    static uint8_t getDeviceLimit(void);
    static bool isValid(void);
    static license_state_t getState(void);
    static bool canSendTally(void);
    static esp_err_t getKey(char* out_key);

private:
    LicenseService() = delete;
    ~LicenseService() = delete;

    static void publishStateEvent(void);
    static esp_err_t onValidateRequest(const event_data_t* event);
    static esp_err_t onNetworkStatusChanged(const event_data_t* event);
    static void validateInTask(const char* key);

    static esp_err_t nvsSetDeviceLimit(uint8_t limit);
    static uint8_t nvsGetDeviceLimit(void);
    static esp_err_t nvsSetLicenseKey(const char* key);
    static esp_err_t nvsGetLicenseKey(char* key, size_t len);

    static void updateState(void);

    static bool s_initialized;
    static bool s_started;
    static license_state_t s_state;
    static uint8_t s_device_limit;
    static char s_license_key[17];
    static bool s_sta_connected;
    static bool s_eth_connected;
};

// ============================================================================
// 정적 변수
// ============================================================================

bool LicenseService::s_initialized = false;
bool LicenseService::s_started = false;
license_state_t LicenseService::s_state = LICENSE_STATE_INVALID;
uint8_t LicenseService::s_device_limit = 0;
char LicenseService::s_license_key[17] = {0};
bool LicenseService::s_sta_connected = false;
bool LicenseService::s_eth_connected = false;

// ============================================================================
// 이벤트 발행 헬퍼
// ============================================================================

void LicenseService::publishStateEvent(void)
{
    // 스택 할당 구조체 사용 (static 변수 문제 회피)
    license_state_event_t event;
    memset(&event, 0, sizeof(event));
    event.device_limit = s_device_limit;
    event.state = static_cast<uint8_t>(s_state);
    event.grace_remaining = 0;

    T_LOGD(TAG, "라이센스 상태 이벤트 발행: limit=%d, state=%d, addr=%p",
           event.device_limit, event.state, &event);

    event_bus_publish(EVT_LICENSE_STATE_CHANGED, &event, sizeof(event));
}

// ============================================================================
// 이벤트 핸들러
// ============================================================================

esp_err_t LicenseService::onValidateRequest(const event_data_t* event)
{
    if (!event || event->type != EVT_LICENSE_VALIDATE) {
        return ESP_OK;
    }

    const license_validate_event_t* req = (const license_validate_event_t*)event->data;
    if (!req || req->key[0] == '\0') {
        T_LOGE(TAG, "Invalid validate request");
        return ESP_ERR_INVALID_ARG;
    }

    T_LOGI(TAG, "라이센스 검증 요청 수신: %.16s", req->key);
    validateInTask(req->key);

    return ESP_OK;
}

esp_err_t LicenseService::onNetworkStatusChanged(const event_data_t* event)
{
    if (!event || event->type != EVT_NETWORK_STATUS_CHANGED) {
        return ESP_OK;
    }

    const network_status_event_t* status = (const network_status_event_t*)event->data;
    if (!status) {
        return ESP_OK;
    }

    bool was_connected = s_sta_connected || s_eth_connected;
    s_sta_connected = status->sta_connected;
    s_eth_connected = status->eth_connected;
    bool now_connected = s_sta_connected || s_eth_connected;

    if (!was_connected && now_connected) {
        T_LOGI(TAG, "네트워크 연결됨 (STA:%d, ETH:%d)", s_sta_connected, s_eth_connected);
    } else if (was_connected && !now_connected) {
        T_LOGW(TAG, "네트워크 연결 해제됨");
    }

    return ESP_OK;
}

// ============================================================================
// 검증 함수
// ============================================================================

void LicenseService::validateInTask(const char* key)
{
    if (!key) {
        return;
    }

    T_LOGI(TAG, "라이센스 검증 시작: %.16s", key);

    s_state = LICENSE_STATE_CHECKING;
    publishStateEvent();

    // MAC 주소 획득
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // 네트워크 연결 확인
    bool connected = s_sta_connected || s_eth_connected;
    if (!connected) {
        // 오프라인 상태: 기존 유효 라이센스가 있으면 유지
        if (s_device_limit > 0 && s_license_key[0] != '\0') {
            T_LOGW(TAG, "오프라인 상태: 기존 라이센스 유지 (limit=%d, key=%.4s****%.4s)",
                    s_device_limit, s_license_key, s_license_key + 12);
            s_state = LICENSE_STATE_VALID;
        } else {
            T_LOGE(TAG, "오프라인 상태: 신규 라이센스 검증 불가 (네트워크 연결 필요)");
            s_state = LICENSE_STATE_INVALID;
        }
        publishStateEvent();
        return;
    }

    // license_client를 통한 검증 (드라이버 계층 사용)
    license_validate_response_t response;
    esp_err_t err = license_client_validate(key, mac_str, connected, &response);

    if (err == ESP_OK && response.success) {
        s_device_limit = response.device_limit;
        strncpy(s_license_key, key, 16);
        s_license_key[16] = '\0';

        nvsSetLicenseKey(s_license_key);
        nvsSetDeviceLimit(s_device_limit);

        T_LOGI(TAG, "라이센스 검증 성공: device_limit = %d", s_device_limit);
    } else {
        T_LOGE(TAG, "라이센스 검증 실패: %s", response.error);
    }

    updateState();
    publishStateEvent();
}

// ============================================================================
// NVS 헬퍼
// ============================================================================

esp_err_t LicenseService::nvsSetDeviceLimit(uint8_t limit)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = nvs_set_u8(handle, "device_limit", limit);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);
    return ret;
}

uint8_t LicenseService::nvsGetDeviceLimit(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return 0;
    }
    uint8_t limit = 0;
    nvs_get_u8(handle, "device_limit", &limit);
    nvs_close(handle);
    return limit;
}

esp_err_t LicenseService::nvsSetLicenseKey(const char* key)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = nvs_set_str(handle, "license_key", key);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);
    return ret;
}

esp_err_t LicenseService::nvsGetLicenseKey(char* key, size_t len)
{
    if (!key || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    key[0] = '\0';

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret;
    }
    size_t actual_len = len;
    ret = nvs_get_str(handle, "license_key", key, &actual_len);
    nvs_close(handle);
    return ret;
}

// ============================================================================
// 상태 관리
// ============================================================================

void LicenseService::updateState(void)
{
    if (s_device_limit > 0) {
        s_state = LICENSE_STATE_VALID;
    } else {
        s_state = LICENSE_STATE_INVALID;
    }
}

// ============================================================================
// 공개 API 구현
// ============================================================================

esp_err_t LicenseService::init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    T_LOGI(TAG, "LicenseService 초기화 중...");

    // license_client 초기화 (드라이버 계층)
    license_client_init();

    nvsGetLicenseKey(s_license_key, sizeof(s_license_key));
    s_device_limit = nvsGetDeviceLimit();

    T_LOGI(TAG, "로드된 라이센스: key=%.16s, limit=%d", s_license_key, s_device_limit);

    updateState();

    s_initialized = true;
    T_LOGI(TAG, "LicenseService 초기화 완료 (상태: %d, limit: %d)",
           s_state, s_device_limit);

    // init()에서는 이벤트 발행 안 함 (start()에서 함)

    return ESP_OK;
}

esp_err_t LicenseService::start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // 이벤트 구독 (최초 1회)
    if (!s_started) {
        event_bus_subscribe(EVT_LICENSE_VALIDATE, onValidateRequest);
        event_bus_subscribe(EVT_NETWORK_STATUS_CHANGED, onNetworkStatusChanged);
        s_started = true;
        T_LOGI(TAG, "LicenseService 시작 (이벤트 구독 완료)");
    } else {
        T_LOGI(TAG, "LicenseService 이미 시작됨");
    }

    // 상태 이벤트 발행 (재호출 시에도 발행하여 구독자에게 전달)
    publishStateEvent();

    return ESP_OK;
}

void LicenseService::stop(void)
{
    if (!s_started) {
        return;
    }

    event_bus_unsubscribe(EVT_LICENSE_VALIDATE, onValidateRequest);
    event_bus_unsubscribe(EVT_NETWORK_STATUS_CHANGED, onNetworkStatusChanged);

    s_started = false;

    T_LOGI(TAG, "LicenseService 정지");
}

esp_err_t LicenseService::validate(const char* key)
{
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }

    validateInTask(key);
    return ESP_OK;
}

esp_err_t LicenseService::searchLicense(const char* name, const char* phone,
                                         const char* email, char* out_response,
                                         size_t response_size)
{
    // license_client (04_driver)로 위임
    return license_client_search_license(name, phone, email, out_response, response_size);
}

uint8_t LicenseService::getDeviceLimit(void)
{
    return s_device_limit;
}

bool LicenseService::isValid(void)
{
    updateState();
    return (s_state == LICENSE_STATE_VALID);
}

license_state_t LicenseService::getState(void)
{
    updateState();
    return s_state;
}

bool LicenseService::canSendTally(void)
{
    return isValid();
}

esp_err_t LicenseService::getKey(char* out_key)
{
    if (!out_key) {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(out_key, s_license_key, 16);
    out_key[16] = '\0';
    return ESP_OK;
}

// ============================================================================
// C 인터페이스
// ============================================================================

extern "C" {

esp_err_t license_service_init(void)
{
    return LicenseService::init();
}

esp_err_t license_service_start(void)
{
    return LicenseService::start();
}

void license_service_stop(void)
{
    LicenseService::stop();
}

esp_err_t license_service_validate(const char* key)
{
    return LicenseService::validate(key);
}

uint8_t license_service_get_device_limit(void)
{
    return LicenseService::getDeviceLimit();
}

bool license_service_is_valid(void)
{
    return LicenseService::isValid();
}

license_state_t license_service_get_state(void)
{
    return LicenseService::getState();
}

bool license_service_is_grace_active(void)
{
    return false;  // 유예 기간 미사용
}

uint32_t license_service_get_grace_remaining(void)
{
    return 0;  // 유예 기간 미사용
}

bool license_service_can_send_tally(void)
{
    return LicenseService::canSendTally();
}

esp_err_t license_service_get_key(char* out_key)
{
    return LicenseService::getKey(out_key);
}

esp_err_t license_service_search_license(const char* name, const char* phone,
                                          const char* email, char* out_response,
                                          size_t response_size)
{
    return LicenseService::searchLicense(name, phone, email, out_response, response_size);
}

}  // extern "C"
