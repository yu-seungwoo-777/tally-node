/**
 * @file LoRaCore.cpp
 * @brief SX1262/SX1268 LoRa Core 구현
 */

#include "LoRaCore.h"
#include "log.h"
#include "log_tags.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <RadioLib.h>

static const char* TAG = TAG_LORA;

// ESP-IDF HAL for RadioLib
class EspHal : public RadioLibHal {
public:
    EspHal() : RadioLibHal(0, 1, 0, 1, GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE),
               spi_host(EORA_S3_LORA_SPI_HOST), initialized(false) {}

    virtual ~EspHal() {
        term();
    }

    void init() override {
        if (initialized) {
            return;
        }

        // GPIO 초기화
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << EORA_S3_LORA_CS) | (1ULL << EORA_S3_LORA_RST);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);

        // BUSY, DIO1 input
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << EORA_S3_LORA_BUSY) | (1ULL << EORA_S3_LORA_DIO1);
        gpio_config(&io_conf);

        // SPI 초기화
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = EORA_S3_LORA_MOSI;
        buscfg.miso_io_num = EORA_S3_LORA_MISO;
        buscfg.sclk_io_num = EORA_S3_LORA_SCK;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = 256;

        esp_err_t ret = spi_bus_initialize(spi_host, &buscfg, SPI_DMA_DISABLED);
        if (ret == ESP_ERR_INVALID_STATE) {
            LOG_0(TAG, "SPI 버스 이미 초기화됨");
        } else if (ret != ESP_OK) {
            LOG_0(TAG, "SPI bus 초기화 실패: %d", ret);
            return;
        }

        // SPI 디바이스 설정
        spi_device_interface_config_t devcfg = {};
        devcfg.clock_speed_hz = 2 * 1000 * 1000;  // 2MHz
        devcfg.mode = 0;
        devcfg.spics_io_num = -1;  // CS는 RadioLib에서 수동 제어
        devcfg.queue_size = 1;
        devcfg.flags = 0;

        ret = spi_bus_add_device(spi_host, &devcfg, &spi_device);
        if (ret != ESP_OK) {
            LOG_0(TAG, "SPI 디바이스 추가 실패: %d", ret);
        }

        initialized = true;
    }

    void term() override {
        if (spi_device) {
            spi_bus_remove_device(spi_device);
            spi_device = nullptr;
        }
        spi_bus_free(spi_host);
    }

    void pinMode(uint32_t pin, uint32_t mode) override {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.pin_bit_mask = (1ULL << pin);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

        if (mode == 0) {  // INPUT
            io_conf.mode = GPIO_MODE_INPUT;
        } else {  // OUTPUT
            io_conf.mode = GPIO_MODE_OUTPUT;
        }
        gpio_config(&io_conf);
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        gpio_set_level((gpio_num_t)pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {
        return gpio_get_level((gpio_num_t)pin);
    }

    void delay(unsigned long ms) override {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    void delayMicroseconds(unsigned long us) override {
        esp_rom_delay_us(us);
    }

    unsigned long millis() override {
        return (unsigned long)(esp_timer_get_time() / 1000ULL);
    }

    unsigned long micros() override {
        return (unsigned long)esp_timer_get_time();
    }

    void spiBegin() override {
        // SPI는 init()에서 초기화됨
    }

    void spiBeginTransaction() override {
        // SX126x BUSY 대기
        uint32_t timeout = 10000;  // 10ms
        uint32_t start = micros();
        while (digitalRead(EORA_S3_LORA_BUSY) == 1) {
            if (micros() - start > timeout) {
                break;
            }
            delayMicroseconds(10);
        }
    }

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override {
        if (len == 0) return;

        spi_transaction_t trans = {};
        trans.length = len * 8;
        trans.rxlength = len * 8;
        trans.tx_buffer = out;
        trans.rx_buffer = in;
        trans.flags = 0;

        spi_device_polling_transmit(spi_device, &trans);
    }

    void spiEndTransaction() override {}
    void spiEnd() override {}

    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override {
        gpio_set_intr_type((gpio_num_t)interruptNum, (gpio_int_type_t)mode);

        // GPIO ISR 서비스 설치 (높은 우선순위 레벨 3)
        // EthernetCore에서 먼저 설치할 수 있으므로 ERR_INVALID_STATE는 무시
        esp_err_t ret = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            LOG_0("EspHal", "GPIO ISR 서비스 설치 실패: %s", esp_err_to_name(ret));
        } else if (ret == ESP_OK) {
            LOG_1("EspHal", "✓ GPIO ISR 서비스 설치 (Level 3 우선순위)");
        }

        gpio_isr_handler_add((gpio_num_t)interruptNum, (gpio_isr_t)interruptCb, nullptr);
    }

    void detachInterrupt(uint32_t interruptNum) override {
        gpio_isr_handler_remove((gpio_num_t)interruptNum);
    }

    long pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) override {
        return 0;
    }

private:
    spi_host_device_t spi_host;
    spi_device_handle_t spi_device = nullptr;
    bool initialized;
};

// 전송 큐 구조체
struct LoRaTxPacket {
    uint8_t data[256];
    size_t length;
};

// 정적 변수
static EspHal* s_hal = nullptr;
static SX126x* s_radio = nullptr;

// 최신 패킷 정보 저장
static float s_last_packet_rssi = -120.0f;
static float s_last_packet_snr = 0.0f;
static int64_t s_last_packet_time = 0;
static Module* s_module = nullptr;
static LoRaChipType s_chip_type = LoRaChipType::UNKNOWN;
static float s_frequency = 0.0f;
static uint8_t s_sync_word = 0x12;  // 기본값
static bool s_initialized = false;
static LoRaReceiveCallback s_receive_callback = nullptr;
static volatile bool s_is_transmitting = false;   // 송신 중 여부 (상태)
static volatile bool s_transmitted_flag = false;  // 송신 완료 이벤트
static volatile bool s_received_flag = false;

// 전송 큐 (링 버퍼)
#define TX_QUEUE_SIZE 5
static LoRaTxPacket s_tx_queue[TX_QUEUE_SIZE];
static uint8_t s_tx_queue_head = 0;  // 쓰기 위치
static uint8_t s_tx_queue_tail = 0;  // 읽기 위치
static uint8_t s_tx_queue_count = 0; // 큐에 있는 패킷 수

// RTOS 태스크 및 Semaphore
static SemaphoreHandle_t s_lora_sem = nullptr;
static TaskHandle_t s_lora_task = nullptr;

// LoRa 전용 태스크
static void loraTask(void* param) {
    LOG_0(TAG, "[태스크] LoRa 전용 태스크 시작 (우선순위: %d)", uxTaskPriorityGet(nullptr));

    while (1) {
        // Semaphore 대기 (인터럽트에서 신호)
        if (xSemaphoreTake(s_lora_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
            // TX 완료 체크
            if (s_transmitted_flag) {
                LoRaCore::checkTransmitted();
            }
            // RX 체크
            if (s_received_flag) {
                LoRaCore::checkReceived();
            }
        }
    }
}

// RadioLib 송신 완료 인터럽트 핸들러
static void IRAM_ATTR loraTxIsr() {
    s_is_transmitting = false;  // 송신 완료 - idle 상태
    s_transmitted_flag = true;   // 송신 완료 이벤트 발생

    // 태스크 깨우기
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_lora_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// RadioLib 수신 완료 인터럽트 핸들러
static void IRAM_ATTR loraRxIsr() {
    s_received_flag = true;

    // 태스크 깨우기
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_lora_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 큐에 패킷 추가
static bool enqueueTxPacket(const uint8_t* data, size_t length) {
    if (s_tx_queue_count >= TX_QUEUE_SIZE) {
        // 큐가 가득 참 - 가장 오래된 패킷 덮어쓰기
        LOG_0(TAG, "[큐] 가득 참 - 가장 오래된 패킷 덮어쓰기");
        s_tx_queue_tail = (s_tx_queue_tail + 1) % TX_QUEUE_SIZE;
        s_tx_queue_count--;
    }

    // 패킷 복사
    memcpy(s_tx_queue[s_tx_queue_head].data, data, length);
    s_tx_queue[s_tx_queue_head].length = length;

    // 헤드 이동
    s_tx_queue_head = (s_tx_queue_head + 1) % TX_QUEUE_SIZE;
    s_tx_queue_count++;

    LOG_1(TAG, "[큐] 패킷 추가: %d/%d", s_tx_queue_count, TX_QUEUE_SIZE);
    return true;
}

// 큐에서 패킷 꺼내기 (병합 최적화)
static bool dequeueTxPacket(uint8_t* data, size_t* length) {
    if (s_tx_queue_count == 0) {
        return false;  // 큐가 비어있음
    }

    // 큐 병합: 여러 패킷이 쌓였으면 최신 것만 전송 (Tally는 최신 상태가 중요)
    if (s_tx_queue_count > 1) {
        LOG_1(TAG, "[큐 병합] %d개 → 최신 것만 전송", s_tx_queue_count);
        // 가장 최신 패킷 인덱스 (head - 1)
        uint8_t latest_idx = (s_tx_queue_head - 1 + TX_QUEUE_SIZE) % TX_QUEUE_SIZE;
        memcpy(data, s_tx_queue[latest_idx].data, s_tx_queue[latest_idx].length);
        *length = s_tx_queue[latest_idx].length;

        // 큐 비우기
        s_tx_queue_tail = s_tx_queue_head;
        s_tx_queue_count = 0;

        LOG_1(TAG, "[큐] 병합 완료 - 큐 비움");
        return true;
    }

    // 패킷이 1개만 있으면 일반 처리
    memcpy(data, s_tx_queue[s_tx_queue_tail].data, s_tx_queue[s_tx_queue_tail].length);
    *length = s_tx_queue[s_tx_queue_tail].length;

    // 테일 이동
    s_tx_queue_tail = (s_tx_queue_tail + 1) % TX_QUEUE_SIZE;
    s_tx_queue_count--;

    LOG_1(TAG, "[큐] 패킷 꺼냄: %d/%d 남음", s_tx_queue_count, TX_QUEUE_SIZE);
    return true;
}

esp_err_t LoRaCore::init(const LoRaConfig* config)
{
    if (s_initialized) {
        LOG_0(TAG, "이미 초기화됨");
        return ESP_OK;
    }

    // HAL 초기화
    s_hal = new EspHal();
    s_hal->init();

    // 기본 설정
    uint8_t sf = config ? config->spreading_factor : 7;
    uint8_t cr = config ? config->coding_rate : 7;
    float bw = config ? config->bandwidth : 125.0f;
    int8_t tx_power = config ? config->tx_power : 22;
    uint8_t sync_word = config ? config->sync_word : 0x12;

    LOG_0(TAG, "칩 자동 감지 중...");

    // Module 생성
    s_module = new Module(s_hal, EORA_S3_LORA_CS, EORA_S3_LORA_DIO1,
                         EORA_S3_LORA_RST, EORA_S3_LORA_BUSY);

    // SX1262 868MHz 시도 (900TB)
    LOG_0(TAG, "SX1262 (868MHz) 감지 시도...");
    SX1262* radio_1262 = new SX1262(s_module);
    int16_t state = radio_1262->begin(868.0f, bw, sf, cr, sync_word, tx_power, 8, 0.0f);

    if (state == RADIOLIB_ERR_NONE) {
        s_radio = radio_1262;
        s_chip_type = LoRaChipType::SX1262_868M;
        s_frequency = 868.0f;
        LOG_0(TAG, "✓ SX1262 (868MHz) 감지됨");
    } else {
        LOG_0(TAG, "SX1262 감지 실패: %d, SX1268 시도...", state);
        delete radio_1262;

        // SX1268 433MHz 시도 (400TB)
        LOG_0(TAG, "SX1268 (433MHz) 감지 시도...");
        SX1268* radio_1268 = new SX1268(s_module);
        state = radio_1268->begin(433.0f, bw, sf, cr, sync_word, tx_power, 8, 0.0f);

        if (state == RADIOLIB_ERR_NONE) {
            s_radio = radio_1268;
            s_chip_type = LoRaChipType::SX1268_433M;
            s_frequency = 433.0f;
            LOG_0(TAG, "✓ SX1268 (433MHz) 감지됨");
        } else {
            LOG_0(TAG, "LoRa 칩 감지 실패 (모든 칩): %d", state);
            delete radio_1268;
            if (s_hal) {
                s_hal->term();
                delete s_hal;
                s_hal = nullptr;
            }
            s_module = nullptr;
            return ESP_FAIL;
        }
    }

    // RadioLib 인터럽트 활성화
    // RadioLib가 자동으로 GPIO 인터럽트를 설정함
    s_radio->setPacketSentAction(loraTxIsr);        // 송신 완료
    s_radio->setPacketReceivedAction(loraRxIsr);    // 수신 완료

    LOG_0(TAG, "✓ 인터럽트 등록 완료 (TX + RX)");

    s_initialized = true;

    // 설정 저장
    s_sync_word = sync_word;

    LOG_0(TAG, "칩: %s", getChipName());
    LOG_0(TAG, "주파수: %.1f MHz", s_frequency);
    LOG_0(TAG, "SF: %d, BW: %.0f kHz, CR: 4/%d", sf, bw, cr);
    LOG_0(TAG, "TX Power: %d dBm", tx_power);
    LOG_0(TAG, "Sync Word: 0x%02X", sync_word);

    // RTOS 태스크 및 Semaphore 생성
    if (s_lora_sem == nullptr) {
        s_lora_sem = xSemaphoreCreateBinary();
        if (s_lora_sem == nullptr) {
            LOG_0(TAG, "✗ Semaphore 생성 실패");
            return ESP_FAIL;
        }
    }

    if (s_lora_task == nullptr) {
        // 높은 우선순위로 태스크 생성 (configMAX_PRIORITIES-2)
        BaseType_t ret = xTaskCreatePinnedToCore(
            loraTask,           // 태스크 함수
            "lora_task",        // 태스크 이름
            4096,               // 스택 크기
            nullptr,            // 파라미터
            configMAX_PRIORITIES - 2,  // 높은 우선순위
            &s_lora_task,       // 태스크 핸들
            1                   // CPU 코어 1 (PRO_CPU)
        );

        if (ret != pdPASS) {
            LOG_0(TAG, "✗ LoRa 태스크 생성 실패");
            return ESP_FAIL;
        }

        LOG_0(TAG, "✓ LoRa 전용 태스크 생성 (우선순위: %d)", configMAX_PRIORITIES - 2);
    }

    // 초기 수신 모드 시작 (idle 상태 방지 및 인터럽트 활성화)
    state = s_radio->startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        LOG_0(TAG, "✓ 초기 수신 모드 시작");
    } else {
        LOG_0(TAG, "✗ 수신 모드 시작 실패: %d", state);
        return ESP_FAIL;
    }

    return ESP_OK;
}

lora_status_t LoRaCore::getStatus()
{
    lora_status_t status = {};
    status.is_initialized = s_initialized;
    status.chip_type = (lora_chip_type_t)s_chip_type;
    status.frequency = s_frequency;
    status.sync_word = s_sync_word;

    // 칩 타입에 따른 주파수 범위 설정
    switch (s_chip_type) {
        case LoRaChipType::SX1268_433M:
            status.freq_min = 410.0f;
            status.freq_max = 493.0f;
            break;
        case LoRaChipType::SX1262_868M:
            status.freq_min = 850.0f;
            status.freq_max = 930.0f;
            break;
        default:
            status.freq_min = 0.0f;
            status.freq_max = 0.0f;
            break;
    }

    if (s_initialized && s_radio) {
        // 최신 패킷의 RSSI/SNR 사용 (없으면 현재 값)
        if (s_last_packet_time > 0) {
            status.rssi = (int16_t)s_last_packet_rssi;
            status.snr = (int16_t)s_last_packet_snr;
        } else {
            status.rssi = (int16_t)s_radio->getRSSI();
            status.snr = (int16_t)s_radio->getSNR();
        }
    } else {
        status.rssi = -120;
        status.snr = 0;
    }

    return status;
}

const char* LoRaCore::getChipName()
{
    switch (s_chip_type) {
        case LoRaChipType::SX1268_433M:
            return "SX1268 (433MHz)";
        case LoRaChipType::SX1262_868M:
            return "SX1262 (868MHz)";
        default:
            return "Unknown";
    }
}

esp_err_t LoRaCore::transmit(const uint8_t* data, size_t length)
{
    if (!s_initialized || !s_radio) {
        LOG_0(TAG, "LoRa not initialized");
        return ESP_FAIL;
    }

    // 송신 중이면 큐에 저장
    if (isTransmitting()) {
        LOG_1(TAG, "송신 중 - 큐에 저장");
        enqueueTxPacket(data, length);
        return ESP_OK;  // 큐에 저장 성공
    }

    // 송신 데이터 출력 (디버깅용)
    LOG_1(TAG, "→ 송신 시작: %d bytes", length);
    char hex_str[80];
    int pos = 0;
    for (size_t i = 0; i < length && i < 10 && pos < sizeof(hex_str) - 4; i++) {
        pos += snprintf(hex_str + pos, sizeof(hex_str) - pos, "%02X ", data[i]);
    }
    LOG_1(TAG, "  데이터: %s", hex_str);

    // 수신 모드에서 송신으로 전환 시 명시적 처리
    // RadioLib는 수신 모드에서 startTransmit() 호출 시 자동 전환하지만,
    // 인터럽트 설정이 유지되지 않을 수 있으므로 재등록
    s_radio->clearPacketReceivedAction();
    s_radio->setPacketSentAction(loraTxIsr);

    // 비동기 송신 시작 (non-blocking)
    s_is_transmitting = true;    // 송신 중 상태로 변경
    s_transmitted_flag = false;  // 이벤트 플래그 초기화
    int16_t state = s_radio->startTransmit((uint8_t*)data, length);
    if (state == RADIOLIB_ERR_NONE) {
        LOG_1(TAG, "✓ 비동기 송신 시작");
        return ESP_OK;
    } else {
        LOG_0(TAG, "✗ 송신 시작 실패: %d", state);
        s_is_transmitting = false;  // 실패 시 상태 복원
        // 수신 모드 복원
        s_radio->setPacketReceivedAction(loraRxIsr);
        s_radio->startReceive();
        return ESP_FAIL;
    }
}

esp_err_t LoRaCore::transmitAsync(const uint8_t* data, size_t length)
{
    if (!s_initialized || !s_radio) {
        LOG_0(TAG, "LoRa not initialized");
        return ESP_FAIL;
    }

    s_is_transmitting = true;    // 송신 중 상태로 변경
    s_transmitted_flag = false;  // 이벤트 플래그 초기화
    int16_t state = s_radio->startTransmit((uint8_t*)data, length);
    if (state == RADIOLIB_ERR_NONE) {
        LOG_1(TAG, "비동기 송신 시작: %d bytes", length);
        return ESP_OK;
    } else {
        LOG_0(TAG, "비동기 송신 시작 실패: %d", state);
        s_is_transmitting = false;  // 실패 시 상태 복원
        return ESP_FAIL;
    }
}

bool LoRaCore::isTransmitting()
{
    if (!s_initialized || !s_radio) {
        return false;
    }
    return s_is_transmitting;
}

esp_err_t LoRaCore::startReceive()
{
    if (!s_initialized || !s_radio) {
        LOG_0(TAG, "LoRa not initialized");
        return ESP_FAIL;
    }

    s_received_flag = false;
    int16_t state = s_radio->startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        LOG_1(TAG, "✓ 수신 모드 시작됨");
        return ESP_OK;
    } else {
        LOG_0(TAG, "✗ 수신 모드 시작 실패: %d", state);
        return ESP_FAIL;
    }
}

void LoRaCore::setReceiveCallback(LoRaReceiveCallback callback)
{
    s_receive_callback = callback;
}

void LoRaCore::checkReceived()
{
    if (!s_initialized || !s_radio) {
        return;
    }

    // 수신 플래그 확인 (RadioLib 예제와 완전히 동일)
    if (s_received_flag) {
        // 플래그 리셋
        s_received_flag = false;

        // 예제 방식: 먼저 패킷 길이 확인
        int numBytes = s_radio->getPacketLength();
        if (numBytes <= 0 || numBytes > 256) {
            LOG_0(TAG, "✗ 잘못된 패킷 길이: %d", numBytes);
            return;
        }

        // 데이터 읽기
        uint8_t buffer[256];
        int state = s_radio->readData(buffer, numBytes);

        if (state == RADIOLIB_ERR_NONE) {
            // 패킷 수신 성공
            float packet_rssi = s_radio->getRSSI();
            float packet_snr = s_radio->getSNR();

            LOG_0(TAG, "✓ 패킷 수신: %d bytes (RSSI: %.1f dBm, SNR: %.1f dB)",
                  numBytes, packet_rssi, packet_snr);

            // 최신 패킷의 RSSI/SNR 저장 (getStatus()에서 사용)
            s_last_packet_rssi = packet_rssi;
            s_last_packet_snr = packet_snr;
            s_last_packet_time = esp_timer_get_time() / 1000;  // 밀리초

            // 수신 콜백 호출
            if (s_receive_callback) {
                s_receive_callback(buffer, (size_t)numBytes);
            }
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            // CRC 오류
            LOG_0(TAG, "✗ CRC 오류");
        } else {
            // 기타 오류
            LOG_0(TAG, "✗ 수신 오류: %d", state);
        }

        // RadioLib은 자동으로 다시 listen 모드로 전환됨
        // startReceive() 재호출 불필요
    }
}

void LoRaCore::checkTransmitted()
{
    if (!s_initialized || !s_radio) {
        return;
    }

    // 송신 완료 플래그 확인
    if (s_transmitted_flag) {
        // 플래그 리셋
        s_transmitted_flag = false;

        LOG_1(TAG, "✓ 송신 완료");

        // RadioLib 예제: 송신 정리 (필수!)
        s_radio->finishTransmit();

        // 큐에 대기 중인 패킷이 있으면 바로 송신
        uint8_t buffer[256];
        size_t length;
        if (dequeueTxPacket(buffer, &length)) {
            LOG_1(TAG, "→ 큐에서 다음 패킷 송신: %d bytes", length);

            // 송신 인터럽트 재등록 (수신→송신 전환)
            // finishTransmit() 후에도 인터럽트 설정이 필요할 수 있음
            s_radio->setPacketSentAction(loraTxIsr);

            // 비동기 송신 시작
            s_is_transmitting = true;    // 송신 중 상태로 변경
            s_transmitted_flag = false;  // 이벤트 플래그 초기화
            int16_t state = s_radio->startTransmit(buffer, length);
            if (state == RADIOLIB_ERR_NONE) {
                LOG_1(TAG, "✓ 큐 패킷 송신 시작");
            } else {
                LOG_0(TAG, "✗ 큐 패킷 송신 실패: %d", state);
                s_is_transmitting = false;  // 실패 시 상태 복원
                // 실패 시 수신 모드로 전환
                s_radio->clearPacketSentAction();
                s_radio->setPacketReceivedAction(loraRxIsr);
                s_radio->startReceive();
            }
        } else {
            // 큐가 비어있으면 수신 모드로 전환 (양방향 통신)
            s_radio->clearPacketSentAction();
            s_radio->setPacketReceivedAction(loraRxIsr);
            int16_t state = s_radio->startReceive();
            if (state == RADIOLIB_ERR_NONE) {
                LOG_1(TAG, "→ 수신 모드로 전환");
            } else {
                LOG_0(TAG, "✗ 수신 모드 전환 실패: %d", state);
            }
        }
    }
}

esp_err_t LoRaCore::sleep()
{
    if (!s_initialized || !s_radio) {
        return ESP_FAIL;
    }

    int16_t state = s_radio->sleep();
    if (state == RADIOLIB_ERR_NONE) {
        LOG_1(TAG, "절전 모드 진입");
        return ESP_OK;
    } else {
        LOG_0(TAG, "절전 모드 실패: %d", state);
        return ESP_FAIL;
    }
}

esp_err_t LoRaCore::scanChannels(float start_freq, float end_freq, float step,
                                   channel_info_t* results, size_t max_results,
                                   size_t* result_count)
{
    if (!s_initialized || !s_radio || !results || !result_count) {
        return ESP_FAIL;
    }

    if (start_freq >= end_freq || step <= 0.0f) {
        LOG_0(TAG, "잘못된 스캔 범위");
        return ESP_FAIL;
    }

    LOG_0(TAG, "채널 스캔 시작: %.1f ~ %.1f MHz (간격 %.1f MHz)", start_freq, end_freq, step);

    size_t count = 0;
    float original_freq = s_frequency;  // 원래 주파수 저장

    for (float freq = start_freq; freq <= end_freq && count < max_results; freq += step) {
        // 주파수 설정
        int16_t state = s_radio->setFrequency(freq);
        if (state != RADIOLIB_ERR_NONE) {
            LOG_0(TAG, "주파수 설정 실패: %.1f MHz", freq);
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
            if (i < 2) vTaskDelay(pdMS_TO_TICKS(10));  // 측정 간 대기
        }
        float rssi = rssi_sum / 3.0f;

        // 결과 저장
        results[count].frequency = freq;
        results[count].rssi = (int16_t)rssi;
        results[count].noise_floor = -100;  // 기본값
        results[count].clear_channel = (rssi < -100.0f);  // RSSI 기준 판단

        LOG_1(TAG, "%.1f MHz: %.1f dBm", freq, rssi);

        count++;
    }

    *result_count = count;

    // 원래 주파수로 복원
    s_radio->setFrequency(original_freq);
    s_radio->startReceive();

    LOG_0(TAG, "채널 스캔 완료: %d개 채널", count);
    return ESP_OK;
}

esp_err_t LoRaCore::setFrequency(float freq)
{
    if (!s_initialized || !s_radio) {
        return ESP_FAIL;
    }

    int16_t state = s_radio->setFrequency(freq);
    if (state == RADIOLIB_ERR_NONE) {
        s_frequency = freq;
        LOG_0(TAG, "주파수 변경: %.1f MHz", freq);
        return ESP_OK;
    } else {
        LOG_0(TAG, "주파수 변경 실패: %d", state);
        return ESP_FAIL;
    }
}

esp_err_t LoRaCore::setSyncWord(uint8_t sync_word)
{
    if (!s_initialized || !s_radio) {
        return ESP_FAIL;
    }

    int16_t state = s_radio->setSyncWord(sync_word);
    if (state == RADIOLIB_ERR_NONE) {
        s_sync_word = sync_word;  // 전역 변수 업데이트
        LOG_0(TAG, "Sync Word 변경: 0x%02X", sync_word);
        return ESP_OK;
    } else {
        LOG_0(TAG, "Sync Word 변경 실패: %d", state);
        return ESP_FAIL;
    }
}
