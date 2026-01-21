/**
 * @file license_service.cpp
 * @brief 라이센스 상태 관리 서비스 구현 (이벤트 기반 NVS)
 */

#include "license_service.h"
#include "license_client.h"
#include "event_bus.h"
#include "t_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cstring>

static const char* TAG = "03_License";

// ============================================================================
// 이벤트 핸들러 C 링케지 forward declarations
// ============================================================================

extern "C" {
    esp_err_t license_on_validate_request(const event_data_t* event);
    esp_err_t license_on_network_status_changed(const event_data_t* event);
    esp_err_t license_on_license_data_save(const event_data_t* event);
    esp_err_t license_on_connection_test_request(const event_data_t* event);
}

// ============================================================================
// 상수 정의
// ============================================================================

// ============================================================================
// LicenseService 클래스 (싱글톤)
// ============================================================================

class LicenseService {
public:
    static esp_err_t init(void);
    static esp_err_t start(void);
    static void stop(void);
    static esp_err_t validate(const char* key);

    static uint8_t getDeviceLimit(void);
    static bool isValid(void);
    static license_state_t getState(void);
    static bool canSendTally(void);
    static esp_err_t getKey(char* out_key);
    static bool connectionTest(void);

    // 이벤트 핸들러 (C 래퍼에서 호출)
    static esp_err_t onValidateRequest(const event_data_t* event);
    static esp_err_t onNetworkStatusChanged(const event_data_t* event);
    static esp_err_t onLicenseDataSave(const event_data_t* event);
    static esp_err_t onConnectionTestRequest(const event_data_t* event);

private:
    LicenseService() = delete;
    ~LicenseService() = delete;

    static void publishStateEvent(void);
    static void validateInTask(const char* key);

    static void updateState(void);

    static bool s_initialized;
    static bool s_started;
    static license_state_t s_state;
    static uint8_t s_device_limit;
    static char s_license_key[17];
    static bool s_sta_connected;
    static bool s_eth_connected;
    static bool s_data_loaded;  // NVS 데이터 로드 완료 여부
    static char s_last_error[128];  // 마지막 에러 메시지
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
bool LicenseService::s_data_loaded = false;
char LicenseService::s_last_error[128] = {0};

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
    // 키 복사
    if (s_license_key[0] != '\0') {
        strncpy(event.key, s_license_key, 16);
        event.key[16] = '\0';
    }
    // 에러 메시지 복사
    if (s_last_error[0] != '\0') {
        strncpy(event.error, s_last_error, sizeof(event.error) - 1);
        event.error[sizeof(event.error) - 1] = '\0';
    }

    T_LOGD(TAG, "license state event published: limit=%d, state=%d, key=%.16s, error=%s",
           event.device_limit, event.state, event.key, event.error);

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

    T_LOGI(TAG, "license validate request received: %.16s", req->key);
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
        T_LOGI(TAG, "network connected (STA:%d, ETH:%d)", s_sta_connected, s_eth_connected);
    } else if (was_connected && !now_connected) {
        T_LOGW(TAG, "network disconnected");
    }

    return ESP_OK;
}

/**
 * @brief 라이센스 데이터 저장 이벤트 핸들러
 * @note config_service에서 NVS 데이터를 발행
 */
esp_err_t LicenseService::onLicenseDataSave(const event_data_t* event)
{
    if (!event || event->type != EVT_LICENSE_DATA_SAVE) {
        return ESP_OK;
    }

    if (event->data_size < sizeof(license_data_event_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    const license_data_event_t* data = (const license_data_event_t*)event->data;

    // 로컬 캐시 업데이트
    s_device_limit = data->device_limit;
    strncpy(s_license_key, data->key, 16);
    s_license_key[16] = '\0';
    s_data_loaded = true;

    // 상태 업데이트 및 이벤트 발행
    updateState();
    publishStateEvent();

    return ESP_OK;
}

/**
 * @brief 라이센스 서버 연결 테스트 이벤트 핸들러
 */
esp_err_t LicenseService::onConnectionTestRequest(const event_data_t* event)
{
    if (!event || event->type != EVT_LICENSE_CONNECTION_TEST) {
        return ESP_OK;
    }

    T_LOGI(TAG, "connection test request");

    // 연결 테스트 수행
    bool success = license_client_connection_test();

    // 결과 이벤트 발행 (에러 메시지 포함)
    license_connection_test_result_t result = {};
    result.success = success;
    if (!success) {
        strncpy(result.error, "Connection timeout or TLS error", sizeof(result.error) - 1);
    }

    event_bus_publish(EVT_LICENSE_CONNECTION_TEST_RESULT, &result, sizeof(result));

    T_LOGI(TAG, "connection test result: %s", success ? "ok" : result.error);

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

    T_LOGI(TAG, "license validation start: %.16s", key);

    // 에러 메시지 초기화
    s_last_error[0] = '\0';

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
            T_LOGW(TAG, "offline: existing license maintained (limit=%d, key=%.16s)",
                    s_device_limit, s_license_key);
            s_state = LICENSE_STATE_VALID;
        } else {
            T_LOGE(TAG, "offline: new license validation failed (network required)");
            s_state = LICENSE_STATE_INVALID;
            strncpy(s_last_error, "네트워크 연결 없음", sizeof(s_last_error) - 1);
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

        // 성공 시 에러 메시지 클리어
        s_last_error[0] = '\0';

        // NVS 저장은 이벤트로 요청 (config_service가 처리)
        license_data_event_t save_data;
        save_data.device_limit = s_device_limit;
        strncpy(save_data.key, s_license_key, 16);
        save_data.key[16] = '\0';
        event_bus_publish(EVT_LICENSE_DATA_SAVE, &save_data, sizeof(save_data));

        T_LOGI(TAG, "license validation success: device_limit = %d", s_device_limit);
    } else {
        T_LOGE(TAG, "license validation failed: %s", response.error);

        // 에러 메시지 저장 (기존 라이센스는 유지됨)
        if (response.error[0] != '\0') {
            strncpy(s_last_error, response.error, sizeof(s_last_error) - 1);
            s_last_error[sizeof(s_last_error) - 1] = '\0';
        } else {
            strncpy(s_last_error, "인증 실패", sizeof(s_last_error) - 1);
        }

        // 기존 라이센스는 유지하고 에러 메시지만 표시
        // NVS 업데이트 없음 (기존 라이센스 유지)
    }

    updateState();
    publishStateEvent();
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

    T_LOGI(TAG, "initializing...");

    // license_client 초기화 (드라이버 계층)
    license_client_init();

    // NVS에서 직접 읽지 않고 start()에서 이벤트로 요청
    s_device_limit = 0;
    s_license_key[0] = '\0';

    s_initialized = true;
    T_LOGI(TAG, "init complete");

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
        event_bus_subscribe(EVT_LICENSE_VALIDATE, license_on_validate_request);
        event_bus_subscribe(EVT_LICENSE_CONNECTION_TEST, license_on_connection_test_request);
        event_bus_subscribe(EVT_NETWORK_STATUS_CHANGED, license_on_network_status_changed);
        event_bus_subscribe(EVT_LICENSE_DATA_SAVE, license_on_license_data_save);
        s_started = true;
        T_LOGI(TAG, "service started (event subscribed)");
    } else {
        T_LOGI(TAG, "already started");
    }

    // 라이센스 데이터 로드 요청 (config_service가 NVS에서 읽어서 응답)
    if (!s_data_loaded) {
        T_LOGI(TAG, "requesting license data from NVS...");
        event_bus_publish(EVT_LICENSE_DATA_REQUEST, NULL, 0);

        // 응답 대기 (최대 100ms)
        int retry = 10;
        while (!s_data_loaded && retry-- > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (!s_data_loaded) {
            T_LOGW(TAG, "license data load timeout, using defaults");
        }
    }

    updateState();
    publishStateEvent();

    return ESP_OK;
}

void LicenseService::stop(void)
{
    if (!s_started) {
        return;
    }

    event_bus_unsubscribe(EVT_LICENSE_VALIDATE, license_on_validate_request);
    event_bus_unsubscribe(EVT_LICENSE_CONNECTION_TEST, license_on_connection_test_request);
    event_bus_unsubscribe(EVT_NETWORK_STATUS_CHANGED, license_on_network_status_changed);
    event_bus_unsubscribe(EVT_LICENSE_DATA_SAVE, license_on_license_data_save);

    s_started = false;

    T_LOGI(TAG, "service stopped");
}

esp_err_t LicenseService::validate(const char* key)
{
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }

    validateInTask(key);
    return ESP_OK;
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

bool LicenseService::connectionTest(void)
{
    return license_client_connection_test();
}

// ============================================================================
// C 인터페이스
// ============================================================================

// 이벤트 핸들러 래퍼 (event_bus_subscribe를 위한 C 링케지)
extern "C" {

esp_err_t license_on_validate_request(const event_data_t* event)
{
    return LicenseService::onValidateRequest(event);
}

esp_err_t license_on_network_status_changed(const event_data_t* event)
{
    return LicenseService::onNetworkStatusChanged(event);
}

esp_err_t license_on_license_data_save(const event_data_t* event)
{
    return LicenseService::onLicenseDataSave(event);
}

esp_err_t license_on_connection_test_request(const event_data_t* event)
{
    return LicenseService::onConnectionTestRequest(event);
}

} // extern "C" for event handlers

// ============================================================================
// 공개 C 인터페이스
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

bool license_service_can_send_tally(void)
{
    return LicenseService::canSendTally();
}

esp_err_t license_service_get_key(char* out_key)
{
    return LicenseService::getKey(out_key);
}

bool license_service_connection_test(void)
{
    return LicenseService::connectionTest();
}

}  // extern "C"
