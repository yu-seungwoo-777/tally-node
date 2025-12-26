/**
 * @file lora_test_app.cpp
 * @brief LoRa 테스트 앱 구현
 *
 * TX 모드:
 * - 주기 송신: STATUS_REQ (5초), PING (10초)
 * - 단일 클릭: 등록된 RX 디바이스 목록 출력
 * - 롱 프레스: STATUS_REQ 브로드캐스트 송신
 *
 * RX 모드:
 * - 단일 클릭: 현재 상태 출력
 * - 롱 프레스: 통계 출력
 */

#include "lora_test_app.h"
#include "LoRaService.h"
#include "LoRaConfig.h"
#include "event_bus.h"
#include "button_poll.h"
#include "t_log.h"

#include "lora_protocol.h"

// TX 모드 전용 헤더
#ifdef DEVICE_MODE_TX
    #include "tx_command.h"
    #include "rx_manager.h"
#endif

// RX 모드 전용 헤더
#ifndef DEVICE_MODE_TX
    #include "rx_command.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

static const char* TAG __attribute__((used)) = "LoRaTestApp";

// ============================================================================
// 정적 변수
// ============================================================================

static bool s_running = false;

// RX 모드 상태 (테스트용)
#ifdef DEVICE_MODE_RX
static uint8_t s_battery = 100;           // 배터리 %
static uint8_t s_camera_id = 1;           // 카메라 ID
static uint32_t s_uptime = 0;             // 업타임 (초)
static uint8_t s_brightness = 50;         // 밝기
static TaskHandle_t s_uptime_task = nullptr;
#endif

// 주기 송신 태스크 (TX 모드)
#ifdef DEVICE_MODE_TX
static TaskHandle_t s_periodic_tx_task = nullptr;

// 주기 송신 간격 (ms)
#define PERIODIC_STATUS_REQ_INTERVAL_MS  5000   // 5초마다 STATUS_REQ
#define PERIODIC_PING_INTERVAL_MS         10000  // 10초마다 PING
#define PERIODIC_LIST_INTERVAL_MS        10000  // 10초마다 디바이스 목록 출력
#define PERIODIC_CONFIG_INTERVAL_MS      5000   // 5초마다 설정 패킷 전송 (테스트용)

// 설정 패킷 테스트를 위한 순환 카운터
static uint8_t s_config_cycle = 0;
#endif

// ============================================================================
// 내부 함수
// ============================================================================

#ifdef DEVICE_MODE_RX
/**
 * @brief RX 모드 상태 콜백
 * STATUS_REQ 수신 시 호출됨
 */
static void on_get_status(rx_status_t* status) {
    if (status == nullptr) {
        return;
    }

    status->battery = s_battery;
    status->camera_id = s_camera_id;
    status->uptime = s_uptime;
    status->brightness = s_brightness;
    status->is_stopped = false;

    T_LOGI(TAG, "상태 제공: bat=%d%%, cam=%d, up=%us, brt=%d",
            s_battery, s_camera_id, s_uptime, s_brightness);
}

/**
 * @brief 업타임 카운터 태스크 (RX 모드)
 */
static void uptime_task(void* arg) {
    (void)arg;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        s_uptime++;
    }
}
#endif // DEVICE_MODE_RX

#ifdef DEVICE_MODE_TX
/**
 * @brief 등록된 디바이스 목록 출력 (TX 모드)
 */
static void print_registered_devices(void) {
    uint8_t device_ids[RX_MANAGER_MAX_REGISTERED * LORA_DEVICE_ID_LEN];
    uint8_t count = rx_manager_get_registered_devices(device_ids);

    T_LOGI(TAG, "=== 등록된 디바이스 (%d/%d) ===", count, RX_MANAGER_MAX_REGISTERED);

    for (uint8_t i = 0; i < count; i++) {
        char id_str[5];
        lora_device_id_to_str(&device_ids[i * LORA_DEVICE_ID_LEN], id_str);

        // 온라인 상태 확인
        int idx = rx_manager_find_device(&device_ids[i * LORA_DEVICE_ID_LEN]);
        if (idx >= 0) {
            rx_device_t dev;
            if (rx_manager_get_device_at(idx, &dev)) {
                T_LOGI(TAG, "  [%d] %s: bat=%d%%, cam=%d, rssi=%d, snr=%.1f, ping=%ums",
                        i, id_str, dev.battery, dev.camera_id,
                        dev.last_rssi, dev.last_snr, dev.ping_ms);
            }
        } else {
            T_LOGI(TAG, "  [%d] %s: 오프라인", i, id_str);
        }
    }
}

/**
 * @brief 온라인 디바이스 목록 출력 (TX 모드)
 */
static void print_online_devices(void) {
    rx_device_t devices[RX_MANAGER_MAX_DEVICES];
    uint8_t count = rx_manager_get_devices(devices);

    T_LOGI(TAG, "=== 온라인 디바이스 (%d) ===", count);

    for (uint8_t i = 0; i < count; i++) {
        char id_str[5];
        lora_device_id_to_str(devices[i].device_id, id_str);
        T_LOGI(TAG, "  [%d] %s: bat=%d%%, cam=%d, rssi=%d, snr=%.1f, ping=%ums",
                i, id_str, devices[i].battery, devices[i].camera_id,
                devices[i].last_rssi, devices[i].last_snr, devices[i].ping_ms);
    }
}
#endif // DEVICE_MODE_TX

// ============================================================================
// 주기 송신 태스크 (TX 모드)
// ============================================================================

#ifdef DEVICE_MODE_TX
/**
 * @brief 주기 송신 태스크
 * - STATUS_REQ 브로드캐스트 (5초마다)
 * - 등록된 디바이스로 PING (11초마다)
 * - 설정 패킷 전송 (30초마다)
 */
static void periodic_tx_task(void* arg) {
    (void)arg;

    uint32_t config_cnt = 0;

    while (s_running) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1초마다 체크

        config_cnt++;

        // 설정 패킷 전송 (5초마다, 순환)
        if (config_cnt >= (PERIODIC_CONFIG_INTERVAL_MS / 1000)) {
            config_cnt = 0;

            // 등록된 첫 번째 디바이스에만 전송
            uint8_t device_ids[RX_MANAGER_MAX_REGISTERED * LORA_DEVICE_ID_LEN];
            uint8_t count = rx_manager_get_registered_devices(device_ids);

            if (count > 0) {
                const uint8_t* device_id = device_ids;
                char id_str[5];
                lora_device_id_to_str(device_id, id_str);
                esp_err_t ret;

                switch (s_config_cycle % 4) {
                    case 0:  // 밝기 설정
                        T_LOGI(TAG, "[주기 송신] SET_BRIGHTNESS(75) -> %s", id_str);
                        ret = tx_command_set_brightness(device_id, 75);
                        if (ret != ESP_OK) {
                            T_LOGW(TAG, "  SET_BRIGHTNESS 송신 실패: %d", ret);
                        }
                        break;

                    case 1:  // 카메라 ID 설정
                        T_LOGI(TAG, "[주기 송신] SET_CAMERA_ID(2) -> %s", id_str);
                        ret = tx_command_set_camera_id(device_id, 2);
                        if (ret != ESP_OK) {
                            T_LOGW(TAG, "  SET_CAMERA_ID 송신 실패: %d", ret);
                        }
                        break;

                    case 2:  // RF 설정 (테스트용 현재 주파수)
                        T_LOGI(TAG, "[주기 송신] SET_RF(868MHz, 0x12) -> %s", id_str);
                        ret = tx_command_set_rf(device_id, 868.0f, 0x12);
                        if (ret != ESP_OK) {
                            T_LOGW(TAG, "  SET_RF 송신 실패: %d", ret);
                        }
                        break;

                    case 3:  // STOP 명령
                        T_LOGI(TAG, "[주기 송신] STOP -> %s", id_str);
                        ret = tx_command_send_stop(device_id);
                        if (ret != ESP_OK) {
                            T_LOGW(TAG, "  STOP 송신 실패: %d", ret);
                        }
                        break;
                }

                s_config_cycle++;
            } else {
                T_LOGD(TAG, "[주기 송신] 등록된 디바이스 없음, 설정 패킷 스킵");
            }
        }
    }

    // 태스크 종료
    vTaskDelete(nullptr);
}
#endif // DEVICE_MODE_TX

// ============================================================================
// 이벤트 핸들러
// ============================================================================

/**
 * @brief LoRa 상태 변경 이벤트 핸들러
 */
static esp_err_t on_lora_status_changed(const event_data_t* event)
{
    if (event->data != nullptr && event->data_size >= sizeof(bool)) {
        bool running = *(const bool*)event->data;
        T_LOGI(TAG, "이벤트: LoRa 상태 변경 -> %s", running ? "실행 중" : "정지");
    }
    return ESP_OK;
}

/**
 * @brief LoRa 패킷 수신 이벤트 핸들러
 */
static esp_err_t on_lora_packet_received(const event_data_t* event)
{
    if (event->data != nullptr && event->data_size >= sizeof(lora_packet_event_t)) {
        const auto* pkt = reinterpret_cast<const lora_packet_event_t*>(event->data);
        T_LOGD(TAG, "이벤트: LoRa 패킷 수신 (%u bytes, rssi=%d, snr=%.1f)",
                pkt->length, pkt->rssi, pkt->snr);
    }
    return ESP_OK;
}

/**
 * @brief LoRa 패킷 송신 이벤트 핸들러
 */
static esp_err_t on_lora_packet_sent(const event_data_t* event)
{
    if (event->data != nullptr && event->data_size >= sizeof(uint32_t)) {
        uint32_t count = *(const uint32_t*)event->data;
        T_LOGD(TAG, "이벤트: LoRa 패킷 송신 (총 %u)", count);
    }
    return ESP_OK;
}

/**
 * @brief RX 매니저 이벤트 콜백 (TX 모드)
 */
#ifdef DEVICE_MODE_TX
static void on_rx_manager_event(void) {
    T_LOGI(TAG, "이벤트: RX 디바이스 상태 변경");
}
#endif

/**
 * @brief 버튼 이벤트 핸들러
 *
 * TX 모드:
 * - 단일 클릭: 등록된/온라인 디바이스 목록 출력
 * - 롱 프레스: STATUS_REQ 브로드캐스트 송신
 *
 * RX 모드:
 * - 단일 클릭: 현재 상태 출력
 * - 롱 프레스: LoRa 통계 출력
 */
static void on_button_event(button_action_t action)
{
    switch (action) {
        case BUTTON_ACTION_SINGLE: {
#ifdef DEVICE_MODE_TX
            // TX 모드: 등록된/온라인 디바이스 목록 출력
            print_registered_devices();
            print_online_devices();
#else
            // RX 모드: 현재 상태 출력
            T_LOGI(TAG, "[버튼] RX 상태:");
            T_LOGI(TAG, "  배터리: %d%%", s_battery);
            T_LOGI(TAG, "  카메라 ID: %d", s_camera_id);
            T_LOGI(TAG, "  업타임: %u 초", s_uptime);
            T_LOGI(TAG, "  밝기: %d", s_brightness);
#endif
            break;
        }

        case BUTTON_ACTION_LONG: {
#ifdef DEVICE_MODE_TX
            // TX 모드: STATUS_REQ 브로드캐스트 송신
            T_LOGI(TAG, "[버튼] STATUS_REQ 브로드캐스트 송신");
            esp_err_t ret = tx_command_send_status_req();
            if (ret == ESP_OK) {
                T_LOGI(TAG, "  STATUS_REQ 송신 성공");
            } else {
                T_LOGW(TAG, "  STATUS_REQ 송신 실패: %d", ret);
            }
#else
            // RX 모드: LoRa 통계 출력
            lora_service_status_t status = lora_service_get_status();
            T_LOGI(TAG, "[버튼] LoRa 통계:");
            T_LOGI(TAG, "  송신: %u", status.packets_sent);
            T_LOGI(TAG, "  수신: %u", status.packets_received);
            T_LOGI(TAG, "  RSSI: %d dBm", status.rssi);
            T_LOGI(TAG, "  SNR: %d dB", status.snr);
#endif
            break;
        }

        case BUTTON_ACTION_LONG_RELEASE:
            T_LOGI(TAG, "[버튼] 롱 프레스 해제");
            break;
    }
}

// ============================================================================
// 공개 API
// ============================================================================

extern "C" {

esp_err_t lora_test_app_init(void)
{
    T_LOGI(TAG, "========================================");
    T_LOGI(TAG, "LoRa 테스트 앱 초기화");
    T_LOGI(TAG, "모드: %s",
#ifdef DEVICE_MODE_TX
           "TX (송신기)"
#else
           "RX (수신기)"
#endif
           );
    T_LOGI(TAG, "========================================");

    // Event Bus 초기화
    T_LOGI(TAG, "Event Bus 초기화 중...");
    esp_err_t ret = event_bus_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "Event Bus 초기화 실패");
        return ret;
    }

    // LoRa 이벤트 구독
    event_bus_subscribe(EVT_LORA_STATUS_CHANGED, on_lora_status_changed);
    event_bus_subscribe(EVT_LORA_PACKET_RECEIVED, on_lora_packet_received);
    event_bus_subscribe(EVT_LORA_PACKET_SENT, on_lora_packet_sent);
    T_LOGI(TAG, "LoRa 이벤트 구독 완료");

    // LoRa Service 초기화
    T_LOGI(TAG, "LoRa Service 초기화 중...");

    lora_service_config_t config = {
        .frequency = LORA_DEFAULT_FREQ,
        .spreading_factor = LORA_DEFAULT_SF,
        .coding_rate = LORA_DEFAULT_CR,
        .bandwidth = LORA_DEFAULT_BW,
        .tx_power = LORA_DEFAULT_TX_POWER,
        .sync_word = LORA_DEFAULT_SYNC_WORD,
    };

    ret = lora_service_init(&config);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "LoRa Service 초기화 실패");
        return ret;
    }

#ifdef DEVICE_MODE_TX
    // TX 모드: tx_command, rx_manager 초기화
    T_LOGI(TAG, "TX 모드 초기화 중...");

    ret = tx_command_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "tx_command 초기화 실패");
        return ret;
    }

    ret = rx_manager_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "rx_manager 초기화 실패");
        return ret;
    }

    rx_manager_set_event_callback(on_rx_manager_event);

    T_LOGI(TAG, "단일 클릭: 디바이스 목록 | 롱 프레스: STATUS_REQ 송신");
#else
    // RX 모드: rx_command 초기화
    T_LOGI(TAG, "RX 모드 초기화 중...");

    ret = rx_command_init(on_get_status);
    if (ret != ESP_OK) {
        T_LOGE(TAG, "rx_command 초기화 실패");
        return ret;
    }

    // 테스트용 Device ID 설정 (실제로는 MAC 주소 뒤 4자리 사용)
    uint8_t test_device_id[LORA_DEVICE_ID_LEN] = {'R', 'X', '0', '1'};
    rx_command_set_device_id(test_device_id);

    char id_str[5];
    lora_device_id_to_str(test_device_id, id_str);
    T_LOGI(TAG, "Device ID: %s", id_str);

    T_LOGI(TAG, "단일 클릭: 상태 출력 | 롱 프레스: LoRa 통계");
#endif

    // 버튼 폴링 초기화
    T_LOGI(TAG, "버튼 폴링 초기화 중...");
    ret = button_poll_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "버튼 폴링 초기화 실패");
        return ret;
    }
    button_poll_set_callback(on_button_event);
    T_LOGI(TAG, "버튼 폴링 초기화 완료");

    T_LOGI(TAG, "✓ LoRa 테스트 앱 초기화 완료");
    return ESP_OK;
}

esp_err_t lora_test_app_start(void)
{
    if (s_running) {
        T_LOGW(TAG, "이미 실행 중");
        return ESP_OK;
    }

    T_LOGI(TAG, "LoRa 테스트 앱 시작 중...");

    // LoRa Service 시작
    esp_err_t ret = lora_service_start();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "LoRa Service 시작 실패");
        return ret;
    }

    // 먼저 running 상태로 변경 (태스크들이 정상 시작되도록)
    s_running = true;

#ifdef DEVICE_MODE_TX
    // TX 모드
    ret = tx_command_start();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "tx_command 시작 실패");
        s_running = false;
        return ret;
    }

    ret = rx_manager_start();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "rx_manager 시작 실패");
        s_running = false;
        return ret;
    }

    // 주기 송신 태스크 시작
    xTaskCreate(periodic_tx_task, "periodic_tx", 4096, nullptr, 2, &s_periodic_tx_task);
    T_LOGI(TAG, "TX 모드: 주기 송신 태스크 시작 (STATUS_REQ: 5s, PING: 10s)");

    T_LOGI(TAG, "TX 모드: tx_command, rx_manager 시작 완료");
#else
    // RX 모드
    ret = rx_command_start();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "rx_command 시작 실패");
        s_running = false;
        return ret;
    }

    // 업타임 카운터 태스크 시작
    xTaskCreate(uptime_task, "uptime", 2048, nullptr, 1, &s_uptime_task);

    T_LOGI(TAG, "RX 모드: rx_command 시작 완료");
#endif

    // 버튼 폴링 시작
    button_poll_start();

    T_LOGI(TAG, "✓ LoRa 테스트 앱 시작 완료 (%.0f MHz)", LORA_DEFAULT_FREQ);

    // 테스트: 이벤트 발생
    T_LOGI(TAG, "테스트: EVT_SYSTEM_READY 이벤트 발행");
    event_bus_publish(EVT_SYSTEM_READY, nullptr, 0);

    return ESP_OK;
}

void lora_test_app_stop(void)
{
    if (!s_running) {
        return;
    }

    T_LOGI(TAG, "LoRa 테스트 앱 정지 중...");
    s_running = false;  // 태스크 루프 종료 신호

    button_poll_stop();

#ifdef DEVICE_MODE_TX
    tx_command_stop();
    rx_manager_stop();

    // 주기 송신 태스크 종료 대기
    if (s_periodic_tx_task != nullptr) {
        // 태스크가 자연스럽게 종료될 때까지 대기
        vTaskDelay(pdMS_TO_TICKS(100));
        s_periodic_tx_task = nullptr;
    }
#else
    rx_command_stop();
    if (s_uptime_task != nullptr) {
        vTaskDelete(s_uptime_task);
        s_uptime_task = nullptr;
    }
#endif

    lora_service_stop();
    T_LOGI(TAG, "✓ LoRa 테스트 앱 정지 완료");
}

void lora_test_app_deinit(void)
{
    lora_test_app_stop();

    // 모드별 추가 해제 없음

    button_poll_deinit();
    lora_service_deinit();
    T_LOGI(TAG, "✓ LoRa 테스트 앱 해제 완료");
}

bool lora_test_app_is_running(void)
{
    return s_running;
}

} // extern "C"
