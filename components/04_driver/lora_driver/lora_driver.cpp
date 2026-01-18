/**
 * @file lora_driver.cpp
 * @brief LoRa 드라이버 구현
 */

#include "lora_driver.h"
#include "lora_hal.h"
#include "PinConfig.h"
#include "t_log.h"
#include "system_wdt.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <RadioLib.h>
#include <string.h>

static const char* TAG __attribute__((unused)) = "04_LoRa";

// =============================================================================
// LoRa 기본 상수 (하드코딩)
// =============================================================================

// Spreading Factor (SF7=빠름/짧은거리, SF12=느림/긴거리)
#define LORA_DEFAULT_SF         7         // SF7

// Coding Rate (CR: 5=4/5, 6=4/6, 7=4/7, 8=4/8)
#define LORA_DEFAULT_CR         7         // CR 4/7

// Bandwidth (kHz)
#define LORA_DEFAULT_BW         250.0f    // 250kHz

// 송신 전력 (dBm)
#define LORA_DEFAULT_TX_POWER   22

// Sync Word
#define LORA_DEFAULT_SYNC_WORD  0x12

// 칩 타입 이름 (표시용)
#define LORA_CHIP_400_NAME      "SX1268 (433MHz)"
#define LORA_CHIP_900_NAME      "SX1262 (868MHz)"

// =============================================================================
// 정적 변수
// =============================================================================

static Module* s_module = nullptr;
static SX126x* s_radio = nullptr;

static lora_chip_type_t s_chip_type = LORA_CHIP_UNKNOWN;
static float s_frequency = 0.0f;
static uint8_t s_sync_word = 0x12;
static bool s_initialized = false;
static lora_receive_callback_t s_receive_callback = nullptr;

static volatile bool s_is_transmitting = false;
static volatile bool s_transmitted_flag = false;
static volatile bool s_received_flag = false;

// 마지막 패킷 RSSI/SNR (패킷 수신 시에만 업데이트)
static volatile int16_t s_last_packet_rssi = -120;
static volatile int8_t s_last_packet_snr = 0;
static volatile bool s_has_received_packet = false;  // 패킷 수신 여부 플래그

// 통계
static uint32_t s_rx_dropped = 0;  // SPI mutex 타임아웃으로 폐기된 RX 패킷

// RTOS 자원
static SemaphoreHandle_t s_semaphore = nullptr;
static SemaphoreHandle_t s_spi_mutex = nullptr;  // SPI 작업 보호용 뮤텍스
static TaskHandle_t s_task = nullptr;

// =============================================================================
// WDT 및 Health Check 관련 정적 변수
// =============================================================================

// Task WDT 설정 (system_wdt에 전달)
static esp_task_wdt_config_t s_wdt_config = {
    .timeout_ms = 5000,        // 5초 타임아웃
    .idle_core_mask = 0,       // 코어 0, 1 모두 감시
    .trigger_panic = true      // 패닉 모드 (크리티컬 태스크에서 재시작)
};

// Health Check 타이머
static esp_timer_handle_t s_health_check_timer = nullptr;
static int64_t s_last_isr_time_us = 0;          // 마지막 ISR 활동 시간 (마이크로초)
static volatile bool s_recovery_pending = false; // 복구 대기 플래그

// Health Check 설정
#define HEALTH_CHECK_INTERVAL_MS    2000    // 2초 간격 체크
#define HEALTH_CHECK_THRESHOLD_MS    5000    // 5초간 ISR 활동 없음 = 복구

// 복구를 위한 저장된 설정
static lora_config_t s_saved_config = {0};
static bool s_has_saved_config = false;

// =============================================================================
// ISR 핸들러
// =============================================================================

static void IRAM_ATTR tx_isr_handler(void) {
    // 마지막 ISR 활동 시간 업데이트 (행 감지용)
    s_last_isr_time_us = esp_timer_get_time();

    // s_is_transmitting = false;  // 제거: check_transmitted() 완료 후 설정
    s_transmitted_flag = true;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void IRAM_ATTR rx_isr_handler(void) {
    // 마지막 ISR 활동 시간 업데이트 (행 감지용)
    s_last_isr_time_us = esp_timer_get_time();

    s_received_flag = true;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// =============================================================================
// LoRa 전용 태스크
// =============================================================================

/**
 * @brief Health Check 타이머 콜백
 *
 * ISR 활동을 모니터링하고, 5초 이상 활동이 없으면 복구 플래그 설정.
 * 타이머 콜백에서 직접 deinit/init을 호출하지 않고 플래그만 설정.
 */
static void health_check_timer_callback(void* arg) {
    int64_t current_time_us = esp_timer_get_time();
    int64_t elapsed_ms = (current_time_us - s_last_isr_time_us) / 1000;

    // 초기화 후 첫health check는 건너뜀 (s_last_isr_time_us == 0)
    if (s_last_isr_time_us == 0) {
        return;
    }

    // 5초 이상 ISR 활동이 없으면 복구 필요
    if (elapsed_ms > HEALTH_CHECK_THRESHOLD_MS) {
        if (!s_recovery_pending) {
            s_recovery_pending = true;
            T_LOGE(TAG, "hang:detected:%lldms", elapsed_ms);
        }
    }
}

/**
 * @brief LoRa 복구 함수
 *
 * 행 상태에서 복구하기 위해 드라이버를 재초기화합니다.
 */
static void lora_driver_recover(void) {
    T_LOGW(TAG, "recover:start");

    // 현재 상태 저장
    bool was_receiving = !s_is_transmitting;

    // Deinit (태스크 제외 - 태스크는 계속 실행)
    if (s_radio) {
        delete s_radio;
        s_radio = nullptr;
    }

    if (s_module) {
        delete s_module;
        s_module = nullptr;
    }

    // HAL 재초기화
    lora_hal_deinit();
    if (lora_hal_init() != ESP_OK) {
        T_LOGE(TAG, "recover:fail:hal");
        return;
    }

    // 저장된 설정으로 재초기화
    if (!s_has_saved_config) {
        T_LOGE(TAG, "recover:fail:no_config");
        return;
    }

    RadioLibHal* hal = lora_hal_get_instance();
    if (hal == nullptr) {
        T_LOGE(TAG, "recover:fail:hal_null");
        return;
    }

    // Module 재생성
    s_module = new Module(hal, EORA_S3_LORA_CS, EORA_S3_LORA_DIO1,
                         EORA_S3_LORA_RST, EORA_S3_LORA_BUSY);

    const lora_config_t* cfg = &s_saved_config;
    int16_t state = RADIOLIB_ERR_NONE;

    // 칩 타입에 따라 라디오 재생성
    if (s_chip_type == LORA_CHIP_SX1262_433M) {
        SX1262* radio = new SX1262(s_module);
        state = radio->begin(cfg->frequency, cfg->bandwidth, cfg->spreading_factor,
                             cfg->coding_rate, cfg->sync_word, cfg->tx_power, 8, 0.0f);
        s_radio = radio;
    } else if (s_chip_type == LORA_CHIP_SX1268_868M) {
        SX1268* radio = new SX1268(s_module);
        state = radio->begin(cfg->frequency, cfg->bandwidth, cfg->spreading_factor,
                             cfg->coding_rate, cfg->sync_word, cfg->tx_power, 8, 0.0f);
        s_radio = radio;
    } else {
        T_LOGE(TAG, "recover:fail:unknown_chip");
        return;
    }

    if (state != RADIOLIB_ERR_NONE) {
        T_LOGE(TAG, "recover:fail:begin:0x%x", state);
        return;
    }

    // 인터럽트 재등록
    s_radio->setPacketSentAction(tx_isr_handler);
    s_radio->setPacketReceivedAction(rx_isr_handler);

    // 수신 모드 재시작
    state = s_radio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        T_LOGE(TAG, "recover:fail:rx:0x%x", state);
        return;
    }

    // ISR 활동 시간 초기화 (재설정 직후이므로 현재 시간으로)
    s_last_isr_time_us = esp_timer_get_time();
    s_recovery_pending = false;

    T_LOGW(TAG, "recover:ok");
}

static void lora_isr_task(void* param) {
    T_LOGD(TAG, "LoRa ISR task start");

    // WDT에 태스크 등록
    system_wdt_register_task("lora_isr_task");

    while (1) {
        // WDT 리셋 (루프마다)
        system_wdt_reset();

        // 복구 플래그 확인
        if (s_recovery_pending) {
            lora_driver_recover();
        }

        // 시마포로 깨어나면 모든 플래그 처리 (놓치는 이벤트 없음)
        if (xSemaphoreTake(s_semaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
            // 플래그가 모두 cleared 될 때까지 계속 확인
            while (s_transmitted_flag || s_received_flag) {
                if (s_transmitted_flag) {
                    lora_driver_check_transmitted();
                }
                if (s_received_flag) {
                    lora_driver_check_received();
                }
            }
        }
    }
}

// =============================================================================
// 공개 API
// =============================================================================

/**
 * @brief LoRa 드라이버를 초기화합니다
 * @param config LoRa 설정 포인터 (주파수, SF, BW, CR, 송신 전력, Sync Word 등)
 * @return ESP_OK 성공, ESP_ERR_INVALID_ARG config가 nullptr인 경우, ESP_FAIL 초기화 실패
 */
esp_err_t lora_driver_init(const lora_config_t* config) {
    T_LOGD(TAG, "init");

    if (s_initialized) {
        T_LOGD(TAG, "ok:already");
        return ESP_OK;
    }

    if (!config) {
        T_LOGE(TAG, "fail:null");
        return ESP_ERR_INVALID_ARG;
    }

    // NVS에서 로드된 설정 값 사용 (lora_service에서 전달)
    uint8_t sf = config->spreading_factor;
    uint8_t cr = config->coding_rate;
    float bw = config->bandwidth;
    int8_t txp = config->tx_power;
    uint8_t sw = config->sync_word;
    float freq = config->frequency;

    // HAL 초기화
    esp_err_t ret = lora_hal_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "fail:hal:0x%x", ret);
        return ESP_FAIL;
    }

    // HAL 가져오기
    RadioLibHal* hal = lora_hal_get_instance();
    if (hal == nullptr) {
        T_LOGE(TAG, "fail:hal_null");
        return ESP_FAIL;
    }

    // Module 생성
    s_module = new Module(hal, EORA_S3_LORA_CS, EORA_S3_LORA_DIO1,
                         EORA_S3_LORA_RST, EORA_S3_LORA_BUSY);

    // 칩 자동 감지: SX1262 먼저 시도 (900TB 모듈)
    T_LOGD(TAG, "detect:sx1262");
    SX1262* radio_1262 = new SX1262(s_module);
    // begin()은 임시 주파수로 초기화 (나중에 setFrequency()로 NVS 값 적용)
    int16_t state = radio_1262->begin(868.0f, bw, sf, cr, sw, txp, 8, 0.0f);

    if (state == RADIOLIB_ERR_NONE) {
        s_radio = radio_1262;
        s_chip_type = LORA_CHIP_SX1262_433M;
        T_LOGD(TAG, "ok:sx1262");
    } else {
        T_LOGD(TAG, "sx1262:0x%x", state);
        delete radio_1262;

        T_LOGD(TAG, "detect:sx1268");
        SX1268* radio_1268 = new SX1268(s_module);
        state = radio_1268->begin(433.0f, bw, sf, cr, sw, txp, 8, 0.0f);

        if (state == RADIOLIB_ERR_NONE) {
            s_radio = radio_1268;
            s_chip_type = LORA_CHIP_SX1268_868M;
            T_LOGD(TAG, "ok:sx1268");
        } else {
            T_LOGE(TAG, "fail:detect:0x%x", state);
            delete radio_1268;
            delete s_module;
            s_module = nullptr;
            return ESP_FAIL;
        }
    }

    // NVS 주파수로 설정
    state = s_radio->setFrequency(freq);
    if (state != RADIOLIB_ERR_NONE) {
        T_LOGE(TAG, "fail:freq:0x%x", state);
        return ESP_FAIL;
    }
    s_frequency = freq;
    T_LOGD(TAG, "freq:%.1fMHz", s_frequency);

    // 인터럽트 등록
    s_radio->setPacketSentAction(tx_isr_handler);
    s_radio->setPacketReceivedAction(rx_isr_handler);
    T_LOGD(TAG, "interrupt registered");

    // 설정 저장
    s_sync_word = sw;

    // Semaphore 생성
    s_semaphore = xSemaphoreCreateBinary();
    if (s_semaphore == nullptr) {
        T_LOGE(TAG, "fail:sem");
        return ESP_FAIL;
    }

    // SPI Mutex 생성
    s_spi_mutex = xSemaphoreCreateMutex();
    if (s_spi_mutex == nullptr) {
        T_LOGE(TAG, "fail:mutex");
        return ESP_FAIL;
    }

    // 태스크 생성 (우선순위 8로 상향)
    // 실시간 LoRa 수신/송신 처리를 위해 최고 우선순위 부여
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        lora_isr_task,
        "lora_isr_task",
        4096,
        nullptr,
        8,  // 우선순위 (최고 - 실시간 통신)
        &s_task,
        1
    );

    if (task_ret != pdPASS) {
        T_LOGE(TAG, "fail:task");
        // 리소스 정리
        if (s_spi_mutex) {
            vSemaphoreDelete(s_spi_mutex);
            s_spi_mutex = nullptr;
        }
        if (s_semaphore) {
            vSemaphoreDelete(s_semaphore);
            s_semaphore = nullptr;
        }
        return ESP_FAIL;
    }

    // 초기 수신 모드 시작
    state = s_radio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        T_LOGE(TAG, "fail:rx:0x%x", state);
        // 리소스 정리
        if (s_task) {
            vTaskDelete(s_task);
            s_task = nullptr;
        }
        if (s_spi_mutex) {
            vSemaphoreDelete(s_spi_mutex);
            s_spi_mutex = nullptr;
        }
        if (s_semaphore) {
            vSemaphoreDelete(s_semaphore);
            s_semaphore = nullptr;
        }
        return ESP_FAIL;
    }

    // =============================================================================
    // Task WDT 초기화
    // =============================================================================
    // 시스템 WDT 관리자 사용 (이미 초기화되어 있으면 무시)
    if (!system_wdt_is_initialized()) {
        ret = system_wdt_init(&s_wdt_config);
        if (ret != ESP_OK) {
            T_LOGW(TAG, "wdt:init:0x%x", ret);
            // WDT 초기화 실패해도 계속 진행 (치명적 오류 아님)
        } else {
            T_LOGD(TAG, "wdt:ok");
        }
    }

    // =============================================================================
    // Health Check 타이머 생성 및 시작
    // =============================================================================
    const esp_timer_create_args_t timer_args = {
        .callback = &health_check_timer_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lora_health_check"
    };

    ret = esp_timer_create(&timer_args, &s_health_check_timer);
    if (ret != ESP_OK) {
        T_LOGW(TAG, "health:timer:create:0x%x", ret);
        // 타이머 생성 실패해도 계속 진행
    } else {
        // 2초 간격으로 health check 시작
        ret = esp_timer_start_periodic(s_health_check_timer,
                                       HEALTH_CHECK_INTERVAL_MS * 1000);
        if (ret != ESP_OK) {
            T_LOGW(TAG, "health:timer:start:0x%x", ret);
            esp_timer_delete(s_health_check_timer);
            s_health_check_timer = nullptr;
        } else {
            T_LOGD(TAG, "health:timer:ok");
        }
    }

    // =============================================================================
    // 복구를 위한 설정 저장
    // =============================================================================
    s_saved_config = *config;
    s_has_saved_config = true;

    // ISR 활동 시간 초기화
    s_last_isr_time_us = esp_timer_get_time();
    s_recovery_pending = false;

    s_initialized = true;

    T_LOGD(TAG, "ok");
    return ESP_OK;
}

/**
 * @brief LoRa 드라이버를 정리하고 리소스를 해제합니다
 */
void lora_driver_deinit(void) {
    T_LOGD(TAG, "deinit");

    // =============================================================================
    // Health Check 타이머 정리
    // =============================================================================
    if (s_health_check_timer) {
        esp_timer_stop(s_health_check_timer);
        esp_timer_delete(s_health_check_timer);
        s_health_check_timer = nullptr;
        T_LOGD(TAG, "health:timer:stopped");
    }

    // =============================================================================
    // Task WDT 정리
    // =============================================================================
    // 시스템 WDT 관리자 사용 (태스크에서 자동 제거됨)
    // lora_isr_task는 무한 루프이므로 명시적 제거 없음

    // =============================================================================
    // 기존 리소스 정리
    // =============================================================================
    if (s_task) {
        vTaskDelete(s_task);
        s_task = nullptr;
    }

    if (s_semaphore) {
        vSemaphoreDelete(s_semaphore);
        s_semaphore = nullptr;
    }

    if (s_spi_mutex) {
        vSemaphoreDelete(s_spi_mutex);
        s_spi_mutex = nullptr;
    }

    if (s_radio) {
        delete s_radio;
        s_radio = nullptr;
    }

    if (s_module) {
        delete s_module;
        s_module = nullptr;
    }

    lora_hal_deinit();

    s_initialized = false;
    s_has_saved_config = false;
    s_recovery_pending = false;

    T_LOGD(TAG, "ok");
}

/**
 * @brief LoRa 드라이버의 현재 상태를 반환합니다
 * @return LoRa 상태 구조체 (초기화 여부, 칩 타입, 주파수, RSSI, SNR)
 */
lora_status_t lora_driver_get_status(void) {
    lora_status_t status = {
        .is_initialized = s_initialized,
        .chip_type = s_chip_type,
        .frequency = s_frequency,
        .rssi = -120,
        .snr = 0,
        .rx_dropped = s_rx_dropped,
    };

    // 패킷을 수신한 적이 있으면 마지막 패킷의 RSSI/SNR 반환
    if (s_has_received_packet) {
        status.rssi = s_last_packet_rssi;
        status.snr = s_last_packet_snr;
    }

    return status;
}

/**
 * @brief LoRa 칩의 모델명을 반환합니다
 * @return 칩 모델명 문자열 (예: "SX1262 (868MHz)", "SX1268 (433MHz)", "Unknown")
 */
const char* lora_driver_get_chip_name(void) {
    switch (s_chip_type) {
        case LORA_CHIP_SX1262_433M:
            return LORA_CHIP_900_NAME;   // SX1262 (868MHz)
        case LORA_CHIP_SX1268_868M:
            return LORA_CHIP_400_NAME;   // SX1268 (433MHz)
        default:
            return "Unknown";
    }
}

/**
 * @brief LoRa 패킷을 비동기로 송신합니다
 * @param data 송신할 데이터 버퍼
 * @param length 데이터 길이 (bytes)
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음,
 *         ESP_ERR_NOT_SUPPORTED 이미 송신 중, ESP_ERR_TIMEOUT SPI 뮤텍스 획득 실패,
 *         ESP_FAIL 송신 시작 실패
 */
esp_err_t lora_driver_transmit(const uint8_t* data, size_t length) {
    if (!s_initialized || !s_radio) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_is_transmitting) {
        T_LOGW(TAG, "tx:busy");
        return ESP_ERR_NOT_SUPPORTED;
    }

    // SPI 뮤텍스 잠금 (최대 1초 대기)
    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        T_LOGE(TAG, "fail:mutex");
        return ESP_ERR_TIMEOUT;
    }

    T_LOGD(TAG, "tx:%dB", length);

    // 비동기 송신 시작
    s_is_transmitting = false;
    s_transmitted_flag = false;
    s_radio->clearPacketReceivedAction();
    s_radio->setPacketSentAction(tx_isr_handler);

    int16_t state = s_radio->startTransmit((uint8_t*)data, length);
    if (state == RADIOLIB_ERR_NONE) {
        s_is_transmitting = true;
        xSemaphoreGive(s_spi_mutex);  // 송신 시작 후 뮤텍스 해제
        return ESP_OK;
    } else {
        T_LOGE(TAG, "fail:tx:0x%x", state);
        xSemaphoreGive(s_spi_mutex);
        return ESP_FAIL;
    }
}

/**
 * @brief 현재 송신 중인지 확인합니다
 * @return true 송신 중, false 대기 중
 */
bool lora_driver_is_transmitting(void) {
    return s_is_transmitting;
}

/**
 * @brief LoRa 수신 모드를 시작합니다
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음,
 *         ESP_ERR_TIMEOUT SPI 뮤텍스 획득 실패, ESP_FAIL 수신 시작 실패
 */
esp_err_t lora_driver_start_receive(void) {
    if (!s_initialized || !s_radio) {
        return ESP_ERR_INVALID_STATE;
    }

    // SPI 뮤텍스 잠금 (50ms 타임아웃 - 이벤트 지연 최소화)
    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        T_LOGW(TAG, "fail:mutex");
        return ESP_ERR_TIMEOUT;
    }

    s_received_flag = false;
    int16_t state = s_radio->startReceive();
    xSemaphoreGive(s_spi_mutex);
    return (state == RADIOLIB_ERR_NONE) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief 패킷 수신 시 호출될 콜백 함수를 설정합니다
 * @param callback 수신 콜백 함수 포인터 (데이터, 길이, RSSI, SNR)
 */
void lora_driver_set_receive_callback(lora_receive_callback_t callback) {
    s_receive_callback = callback;
}

/**
 * @brief 수신된 패킷을 확인하고 처리합니다 (ISR 태스크에서 호출)
 * 수신된 데이터를 읽고 등록된 콜백 함수를 호출합니다
 */
void lora_driver_check_received(void) {
    if (!s_initialized || !s_radio) {
        return;
    }

    if (s_received_flag) {
        s_received_flag = false;

        // SPI 뮤텍스 잠금 (50ms 타임아웃 - 이벤트 지연 최소화)
        if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
            s_rx_dropped++;
            T_LOGW(TAG, "fail:mutex:drop=%lu", s_rx_dropped);
            return;
        }

        int num_bytes = s_radio->getPacketLength();
        if (num_bytes <= 0 || num_bytes > 256) {
            T_LOGW(TAG, "fail:len:%d", num_bytes);
            xSemaphoreGive(s_spi_mutex);
            return;
        }

        uint8_t buffer[256];
        int state = s_radio->readData(buffer, num_bytes);

        if (state == RADIOLIB_ERR_NONE) {
            float rssi = s_radio->getRSSI();
            float snr = s_radio->getSNR();

            // 패킷 RSSI/SNR 저장
            s_last_packet_rssi = (int16_t)rssi;
            s_last_packet_snr = (int8_t)snr;
            s_has_received_packet = true;

            // 콜백 호출 전 뮤텍스 해제 (데드락 방지)
            xSemaphoreGive(s_spi_mutex);

            T_LOGD(TAG, "rx:%dB,rssi:%.0f,snr:%.0f", num_bytes, rssi, snr);

            if (s_receive_callback) {
                s_receive_callback(buffer, num_bytes, (int16_t)rssi, snr);
            }
        } else {
            xSemaphoreGive(s_spi_mutex);
            if (state == RADIOLIB_ERR_CRC_MISMATCH) {
                T_LOGW(TAG, "fail:crc");
            }
        }
    }
}

/**
 * @brief 송신 완료를 확인하고 후처리를 수행합니다 (ISR 태스크에서 호출)
 * 송신 완료 후 수신 모드로 전환합니다
 */
void lora_driver_check_transmitted(void) {
    if (!s_initialized || !s_radio) {
        return;
    }

    if (s_transmitted_flag) {
        s_transmitted_flag = false;

        // SPI 뮤텍스 잠금 (50ms 타임아웃 - 이벤트 지연 최소화)
        if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
            T_LOGW(TAG, "fail:mutex");
            return;
        }

        s_radio->finishTransmit();

        // 수신 모드로 전환
        s_radio->clearPacketSentAction();
        s_radio->setPacketReceivedAction(rx_isr_handler);
        s_radio->startReceive();

        xSemaphoreGive(s_spi_mutex);

        // 수신 모드 전환 완료 후 송신 상태 해제
        s_is_transmitting = false;

        T_LOGD(TAG, "tx:ok");
    }
}

/**
 * @brief LoRa 칩을 절전 모드(Sleep)로 전환합니다
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음,
 *         ESP_ERR_TIMEOUT SPI 뮤텍스 획득 실패, ESP_FAIL 절전 모드 전환 실패
 */
esp_err_t lora_driver_sleep(void) {
    if (!s_initialized || !s_radio) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int16_t state = s_radio->sleep();
    xSemaphoreGive(s_spi_mutex);
    return (state == RADIOLIB_ERR_NONE) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief LoRa 주파수를 설정합니다
 * @param freq_mhz 주파수 (MHz) - SX1262: 850-930MHz, SX1268: 410-493MHz
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음,
 *         ESP_ERR_INVALID_ARG 주파수 범위 벗어남, ESP_ERR_TIMEOUT SPI 뮤텍스 획득 실패,
 *         ESP_FAIL 주파수 설정 실패
 */
esp_err_t lora_driver_set_frequency(float freq_mhz) {
    if (!s_initialized || !s_radio) {
        return ESP_ERR_INVALID_STATE;
    }

    // 모듈별 주파수 범위 검사
    // SX1262 (900TB): 850-930 MHz
    // SX1268 (400TB): 410-493 MHz
    bool valid = false;
    switch (s_chip_type) {
        case LORA_CHIP_SX1262_433M:  // 900TB
            valid = (freq_mhz >= 850.0f && freq_mhz <= 930.0f);
            break;
        case LORA_CHIP_SX1268_868M:  // 400TB
            valid = (freq_mhz >= 410.0f && freq_mhz <= 493.0f);
            break;
        default:
            break;
    }

    if (!valid) {
        T_LOGW(TAG, "freq:invalid:%.1f", freq_mhz);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int16_t state = s_radio->setFrequency(freq_mhz);

    // 주파수 변경 후 수신 모드 재시작
    if (state == RADIOLIB_ERR_NONE) {
        s_radio->startReceive();
    }

    xSemaphoreGive(s_spi_mutex);

    if (state == RADIOLIB_ERR_NONE) {
        s_frequency = freq_mhz;
        T_LOGD(TAG, "freq:%.1fMHz", freq_mhz);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief LoRa Sync Word를 설정합니다
 * @param sync_word Sync Word 값 (0x00-0xFF, 일반적으로 0x12 또는 0x27)
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음,
 *         ESP_ERR_TIMEOUT SPI 뮤텍스 획득 실패, ESP_FAIL Sync Word 설정 실패
 */
esp_err_t lora_driver_set_sync_word(uint8_t sync_word) {
    if (!s_initialized || !s_radio) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int16_t state = s_radio->setSyncWord(sync_word);

    // Sync Word 변경 후 수신 모드 재시작
    if (state == RADIOLIB_ERR_NONE) {
        s_radio->startReceive();
    }

    xSemaphoreGive(s_spi_mutex);

    if (state == RADIOLIB_ERR_NONE) {
        s_sync_word = sync_word;
        T_LOGD(TAG, "sync:0x%02X", sync_word);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief 지정된 주파수 범위의 채널들을 스캔하여 RSSI를 측정합니다
 * @param start_freq 시작 주파수 (MHz)
 * @param end_freq 종료 주파수 (MHz)
 * @param step 주파수 간격 (MHz)
 * @param results 채널 정보 저장 배열 포인터
 * @param max_results 최대 결과 개수
 * @param result_count 실제 측정된 채널 수 (출력 파라미터)
 * @return ESP_OK 성공, ESP_ERR_INVALID_STATE 초기화되지 않음,
 *         ESP_ERR_INVALID_ARG 파라미터 오류, ESP_ERR_TIMEOUT SPI 뮤텍스 획득 실패
 */
esp_err_t lora_driver_scan_channels(float start_freq, float end_freq, float step,
                                     channel_info_t* results, size_t max_results,
                                     size_t* result_count) {
    if (!s_initialized || !s_radio || !results || !result_count) {
        return ESP_ERR_INVALID_STATE;
    }

    if (start_freq >= end_freq || step <= 0.0f) {
        T_LOGE(TAG, "fail:invalid_range");
        return ESP_ERR_INVALID_ARG;
    }

    T_LOGD(TAG, "scan:%.1f-%.1fMHz", start_freq, end_freq);

    size_t count = 0;
    float original_freq = s_frequency;  // 원래 주파수 저장

    // SPI 뮤텍스 잠금
    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        T_LOGE(TAG, "fail:mutex");
        return ESP_ERR_TIMEOUT;
    }

    for (float freq = start_freq; freq <= end_freq && count < max_results; freq += step) {
        // 주파수 설정
        int16_t state = s_radio->setFrequency(freq);
        if (state != RADIOLIB_ERR_NONE) {
            T_LOGW(TAG, "fail:freq:%.1f", freq);
            continue;
        }

        // 수신 모드로 전환
        s_radio->startReceive();

        // RSSI 안정화를 위한 대기
        vTaskDelay(pdMS_TO_TICKS(20));

        // 3번 측정하여 평균 (안정성 향상)
        float rssi_sum = 0.0f;
        for (int i = 0; i < 3; i++) {
            rssi_sum += s_radio->getRSSI(false);
            if (i < 2) {
                vTaskDelay(pdMS_TO_TICKS(10));  // 측정 간 대기
            }
        }
        float rssi_avg = rssi_sum / 3.0f;

        // 결과 저장
        results[count].frequency = freq;
        results[count].rssi = (int16_t)rssi_avg;
        results[count].noise_floor = -100;  // 기본값 (나중에 CAD로 개선 가능)
        // -80 dBm 미만 = 조용한 채널, 이상 = 노이즈/사용중
        results[count].clear_channel = (rssi_avg < -80.0f);

        T_LOGD(TAG, "%.1fMHz:%.0fdBm", freq, rssi_avg);

        count++;
    }

    // 원래 주파수로 복원
    s_radio->setFrequency(original_freq);
    s_radio->startReceive();

    xSemaphoreGive(s_spi_mutex);

    *result_count = count;
    T_LOGD(TAG, "scan:ok:%d", count);

    return ESP_OK;
}
