/**
 * @file lora_driver.cpp
 * @brief LoRa 드라이버 구현
 */

#include "lora_driver.h"
#include "lora_hal.h"
#include "PinConfig.h"
#include "t_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <RadioLib.h>
#include <string.h>

static const char* TAG __attribute__((unused)) = "LoRaDriver";

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

// RTOS 자원
static SemaphoreHandle_t s_semaphore = nullptr;
static SemaphoreHandle_t s_spi_mutex = nullptr;  // SPI 작업 보호용 뮤텍스
static TaskHandle_t s_task = nullptr;

// =============================================================================
// ISR 핸들러
// =============================================================================

static void IRAM_ATTR tx_isr_handler(void) {
    // s_is_transmitting = false;  // 제거: check_transmitted() 완료 후 설정
    s_transmitted_flag = true;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void IRAM_ATTR rx_isr_handler(void) {
    s_received_flag = true;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// =============================================================================
// LoRa 전용 태스크
// =============================================================================

static void lora_task(void* param) {
    T_LOGI(TAG, "LoRa 전용 태스크 시작");

    while (1) {
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

esp_err_t lora_driver_init(const lora_config_t* config) {
    if (s_initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    // 기본 설정 (LoRaConfig.h 매크로 사용)
    uint8_t sf = config ? config->spreading_factor : LORA_DEFAULT_SF;
    uint8_t cr = config ? config->coding_rate : LORA_DEFAULT_CR;
    float bw = config ? config->bandwidth : LORA_DEFAULT_BW;
    int8_t txp = config ? config->tx_power : LORA_DEFAULT_TX_POWER;
    uint8_t sw = config ? config->sync_word : LORA_DEFAULT_SYNC_WORD;

    T_LOGI(TAG, "LoRa 드라이버 초기화 중...");
    T_LOGI(TAG, "  SF=%d, BW=%.0fkHz, CR=4/%d, TXP=%ddBm, SW=0x%02X",
             sf, bw, cr, txp, sw);

    // HAL 초기화
    esp_err_t ret = lora_hal_init();
    if (ret != ESP_OK) {
        T_LOGE(TAG, "HAL 초기화 실패: %d", ret);
        return ESP_FAIL;
    }

    // HAL 가져오기
    RadioLibHal* hal = lora_hal_get_instance();
    if (hal == nullptr) {
        T_LOGE(TAG, "HAL 가져오기 실패 (초기화되지 않음)");
        return ESP_FAIL;
    }

    // Module 생성
    s_module = new Module(hal, EORA_S3_LORA_CS, EORA_S3_LORA_DIO1,
                         EORA_S3_LORA_RST, EORA_S3_LORA_BUSY);

    // 칩 자동 감지: SX1262 (868MHz) 먼저 시도 (900TB 모듈)
    T_LOGI(TAG, "SX1262 (868MHz) 감지 시도...");
    SX1262* radio_1262 = new SX1262(s_module);
    int16_t state = radio_1262->begin(868.0f, bw, sf, cr, sw, txp, 8, 0.0f);  // Preamble=8

    if (state == RADIOLIB_ERR_NONE) {
        s_radio = radio_1262;
        s_chip_type = LORA_CHIP_SX1262_433M;  // 타입명은 유지
        s_frequency = 868.0f;
        T_LOGI(TAG, "✓ SX1262 (868MHz) 감지됨");
    } else {
        T_LOGW(TAG, "SX1262 실패: %d, SX1268 시도...", state);
        delete radio_1262;

        // SX1268 (433MHz) 시도 (400TB 모듈)
        T_LOGI(TAG, "SX1268 (433MHz) 감지 시도...");
        SX1268* radio_1268 = new SX1268(s_module);
        state = radio_1268->begin(433.0f, bw, sf, cr, sw, txp, 8, 0.0f);  // Preamble=8

        if (state == RADIOLIB_ERR_NONE) {
            s_radio = radio_1268;
            s_chip_type = LORA_CHIP_SX1268_868M;  // 타입명은 유지
            s_frequency = 433.0f;
            T_LOGI(TAG, "✓ SX1268 (433MHz) 감지됨");
        } else {
            T_LOGE(TAG, "LoRa 칩 감지 실패 (모든 칩): %d", state);
            delete radio_1268;
            delete s_module;
            s_module = nullptr;
            return ESP_FAIL;
        }
    }

    // 인터럽트 등록
    s_radio->setPacketSentAction(tx_isr_handler);
    s_radio->setPacketReceivedAction(rx_isr_handler);
    T_LOGI(TAG, "✓ 인터럽트 등록 완료");

    // 설정 저장
    s_sync_word = sw;

    // Semaphore 생성
    s_semaphore = xSemaphoreCreateBinary();
    if (s_semaphore == nullptr) {
        T_LOGE(TAG, "Semaphore 생성 실패");
        return ESP_FAIL;
    }

    // SPI Mutex 생성
    s_spi_mutex = xSemaphoreCreateMutex();
    if (s_spi_mutex == nullptr) {
        T_LOGE(TAG, "SPI Mutex 생성 실패");
        return ESP_FAIL;
    }

    // 태스크 생성
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        lora_task,
        "lora_task",
        4096,
        nullptr,
        6,  // 우선순위 (중간)
        &s_task,
        1
    );

    if (task_ret != pdPASS) {
        T_LOGE(TAG, "태스크 생성 실패");
        return ESP_FAIL;
    }

    // 초기 수신 모드 시작
    state = s_radio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        T_LOGE(TAG, "수신 모드 시작 실패: %d", state);
        return ESP_FAIL;
    }

    s_initialized = true;
    T_LOGI(TAG, "✓ LoRa 드라이버 초기화 완료");
    T_LOGI(TAG, "  칩: %s", lora_driver_get_chip_name());
    T_LOGI(TAG, "  주파수: %.1f MHz", s_frequency);

    return ESP_OK;
}

void lora_driver_deinit(void) {
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
}

lora_status_t lora_driver_get_status(void) {
    lora_status_t status = {
        .is_initialized = s_initialized,
        .chip_type = s_chip_type,
        .frequency = s_frequency,
        .rssi = -120,
        .snr = 0,
    };

    // 패킷을 수신한 적이 있으면 마지막 패킷의 RSSI/SNR 반환
    if (s_has_received_packet) {
        status.rssi = s_last_packet_rssi;
        status.snr = s_last_packet_snr;
    }

    return status;
}

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

esp_err_t lora_driver_transmit(const uint8_t* data, size_t length) {
    if (!s_initialized || !s_radio) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_is_transmitting) {
        T_LOGW(TAG, "송신 중 - 패킷 무시");
        return ESP_ERR_NOT_SUPPORTED;
    }

    // SPI 뮤텍스 잠금 (최대 1초 대기)
    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        T_LOGE(TAG, "SPI 뮤텍스 획득 타임아웃");
        return ESP_ERR_TIMEOUT;
    }

    T_LOGD(TAG, "→ 송신: %d bytes", length);

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
        T_LOGE(TAG, "송신 시작 실패: %d", state);
        xSemaphoreGive(s_spi_mutex);
        return ESP_FAIL;
    }
}

bool lora_driver_is_transmitting(void) {
    return s_is_transmitting;
}

esp_err_t lora_driver_start_receive(void) {
    if (!s_initialized || !s_radio) {
        return ESP_ERR_INVALID_STATE;
    }

    // SPI 뮤텍스 잠금
    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        T_LOGW(TAG, "수신 모드 시작 중 뮤텍스 획득 실패");
        return ESP_ERR_TIMEOUT;
    }

    s_received_flag = false;
    int16_t state = s_radio->startReceive();
    xSemaphoreGive(s_spi_mutex);
    return (state == RADIOLIB_ERR_NONE) ? ESP_OK : ESP_FAIL;
}

void lora_driver_set_receive_callback(lora_receive_callback_t callback) {
    s_receive_callback = callback;
}

void lora_driver_check_received(void) {
    if (!s_initialized || !s_radio) {
        return;
    }

    if (s_received_flag) {
        s_received_flag = false;

        // SPI 뮤텍스 잠금
        if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            T_LOGW(TAG, "수신 처리 중 뮤텍스 획득 실패");
            return;
        }

        int num_bytes = s_radio->getPacketLength();
        if (num_bytes <= 0 || num_bytes > 256) {
            T_LOGW(TAG, "잘못된 패킷 길이: %d", num_bytes);
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

            T_LOGD(TAG, "← 수신: %d bytes (RSSI: %.1f dBm, SNR: %.1f dB)",
                     num_bytes, rssi, snr);

            if (s_receive_callback) {
                s_receive_callback(buffer, num_bytes, (int16_t)rssi, snr);
            }
        } else {
            xSemaphoreGive(s_spi_mutex);
            if (state == RADIOLIB_ERR_CRC_MISMATCH) {
                T_LOGW(TAG, "CRC 오류");
            }
        }
    }
}

void lora_driver_check_transmitted(void) {
    if (!s_initialized || !s_radio) {
        return;
    }

    if (s_transmitted_flag) {
        s_transmitted_flag = false;

        // SPI 뮤텍스 잠금
        if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            T_LOGW(TAG, "송신 완료 처리 중 뮤텍스 획득 실패");
            return;
        }

        T_LOGD(TAG, "✓ 송신 완료");

        s_radio->finishTransmit();

        // 수신 모드로 전환
        s_radio->clearPacketSentAction();
        s_radio->setPacketReceivedAction(rx_isr_handler);
        s_radio->startReceive();

        xSemaphoreGive(s_spi_mutex);

        // 수신 모드 전환 완료 후 송신 상태 해제
        s_is_transmitting = false;
    }
}

esp_err_t lora_driver_sleep(void) {
    if (!s_initialized || !s_radio) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int16_t state = s_radio->sleep();
    xSemaphoreGive(s_spi_mutex);
    return (state == RADIOLIB_ERR_NONE) ? ESP_OK : ESP_FAIL;
}

esp_err_t lora_driver_set_frequency(float freq_mhz) {
    if (!s_initialized || !s_radio) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int16_t state = s_radio->setFrequency(freq_mhz);
    xSemaphoreGive(s_spi_mutex);

    if (state == RADIOLIB_ERR_NONE) {
        s_frequency = freq_mhz;
        T_LOGI(TAG, "주파수 변경: %.1f MHz", freq_mhz);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t lora_driver_set_sync_word(uint8_t sync_word) {
    if (!s_initialized || !s_radio) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    int16_t state = s_radio->setSyncWord(sync_word);
    xSemaphoreGive(s_spi_mutex);

    if (state == RADIOLIB_ERR_NONE) {
        s_sync_word = sync_word;
        T_LOGI(TAG, "Sync Word 변경: 0x%02X", sync_word);
        return ESP_OK;
    }
    return ESP_FAIL;
}
