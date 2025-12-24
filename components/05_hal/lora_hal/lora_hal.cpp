/**
 * @file lora_hal.cpp
 * @brief LoRa HAL - RadioLib를 위한 ESP-IDF SPI/GPIO 추상화
 *
 * ESP32-S3에서 RadioLib SX1262/SX1268을 사용하기 위한 HAL 구현
 */

#include "lora_hal.h"
#include "PinConfig.h"
#include "t_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <RadioLib.h>

static const char* TAG = "LoRaHal";

// ============================================================================
// EspHal 클래스 - RadioLib HAL 구현
// ============================================================================

// ISR 서비스 설치 상태 추적 (전역 변수)
static bool s_isr_service_installed = false;

class EspHal : public RadioLibHal {
public:
    EspHal() : RadioLibHal(0, 1, 0, 1, GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE),
               spi_host(EORA_S3_LORA_SPI_HOST), spi_device(nullptr), initialized(false) {}

    virtual ~EspHal() {
        term();
    }

    // HAL 초기화 (외부에서 호출 필요)
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
            T_LOGI(TAG, "SPI 버스 이미 초기화됨");
        } else if (ret != ESP_OK) {
            T_LOGE(TAG, "SPI 버스 초기화 실패: %d", ret);
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
            T_LOGE(TAG, "SPI 디바이스 추가 실패: %d (%s)", ret, esp_err_to_name(ret));
            return;
        }

        initialized = true;
        T_LOGI(TAG, "LoRa HAL 초기화 완료 (spi_device=%p)", (void*)spi_device);
    }

    // 초기화 상태 확인
    bool isInitialized() const {
        return initialized;
    }

    void term() {
        if (spi_device) {
            spi_bus_remove_device(spi_device);
            spi_device = nullptr;
        }
        spi_bus_free(spi_host);
    }

    // GPIO 모드 설정
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

    // GPIO 쓰기
    void digitalWrite(uint32_t pin, uint32_t value) override {
        gpio_set_level((gpio_num_t)pin, value);
    }

    // GPIO 읽기
    uint32_t digitalRead(uint32_t pin) override {
        return gpio_get_level((gpio_num_t)pin);
    }

    // 밀리초 지연
    void delay(unsigned long ms) override {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    // 마이크로초 지연
    void delayMicroseconds(unsigned long us) override {
        esp_rom_delay_us(us);
    }

    // 밀리초 가져오기
    unsigned long millis() override {
        return (unsigned long)(esp_timer_get_time() / 1000ULL);
    }

    // 마이크로초 가져오기
    unsigned long micros() override {
        return (unsigned long)esp_timer_get_time();
    }

    // SPI 시작
    void spiBegin() override {
        // SPI는 init()에서 초기화됨
    }

    // SPI 트랜잭션 시작 (BUSY 대기)
    void spiBeginTransaction() override {
        uint32_t timeout = 10000;  // 10ms
        uint32_t start = micros();
        while (digitalRead(EORA_S3_LORA_BUSY) == 1) {
            if (micros() - start > timeout) {
                break;
            }
            delayMicroseconds(10);
        }
    }

    // SPI 전송
    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override {
        if (len == 0) return;
        if (spi_device == nullptr) {
            T_LOGE(TAG, "SPI 디바이스가 초기화되지 않음");
            return;
        }

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

    // 인터럽트 핸들러 등록
    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override {
        gpio_set_intr_type((gpio_num_t)interruptNum, (gpio_int_type_t)mode);

        // GPIO ISR 서비스 설치 (한 번만)
        if (!s_isr_service_installed) {
            esp_err_t ret = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
            if (ret == ESP_OK) {
                s_isr_service_installed = true;
            } else if (ret != ESP_ERR_INVALID_STATE) {
                T_LOGE(TAG, "GPIO ISR 서비스 설치 실패: %s", esp_err_to_name(ret));
            }
        }

        gpio_isr_handler_add((gpio_num_t)interruptNum, (gpio_isr_t)interruptCb, nullptr);
    }

    // 인터럽트 핸들러 제거
    void detachInterrupt(uint32_t interruptNum) override {
        gpio_isr_handler_remove((gpio_num_t)interruptNum);
    }

    long pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) override {
        return 0;
    }

private:
    spi_host_device_t spi_host;
    spi_device_handle_t spi_device;
    bool initialized;
};

// ============================================================================
// 정적 변수 및 C API
// ============================================================================

static EspHal* s_hal = nullptr;

// C API 구현
extern "C" {

esp_err_t lora_hal_init(void)
{
    if (s_hal == nullptr) {
        s_hal = new EspHal();
    }

    s_hal->init();

    // 초기화 성공 확인
    if (!s_hal->isInitialized()) {
        T_LOGE(TAG, "HAL 초기화 실패 (spi_device가 nullptr)");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void lora_hal_deinit(void)
{
    if (s_hal) {
        delete s_hal;
        s_hal = nullptr;
    }
}

esp_err_t lora_hal_spi_transfer(const uint8_t* out, uint8_t* in, size_t length)
{
    if (s_hal == nullptr || !s_hal->isInitialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    // const_cast는 RadioLib가 버퍼를 수정하지 않을 때 안전
    s_hal->spiTransfer(const_cast<uint8_t*>(out), length, in);
    return ESP_OK;
}

void lora_hal_pin_mode(uint32_t pin, bool is_input)
{
    if (s_hal) {
        s_hal->pinMode(pin, is_input ? 0 : 1);
    }
}

void lora_hal_digital_write(uint32_t pin, uint32_t level)
{
    if (s_hal) {
        s_hal->digitalWrite(pin, level);
    }
}

uint32_t lora_hal_digital_read(uint32_t pin)
{
    if (s_hal) {
        return s_hal->digitalRead(pin);
    }
    return 0;
}

bool lora_hal_wait_busy(uint32_t timeout_us)
{
    if (s_hal == nullptr) {
        return false;
    }

    uint32_t start = s_hal->micros();
    while (s_hal->digitalRead(EORA_S3_LORA_BUSY) == 1) {
        if (s_hal->micros() - start > timeout_us) {
            return false;
        }
        s_hal->delayMicroseconds(10);
    }
    return true;
}

esp_err_t lora_hal_attach_interrupt(uint32_t pin, void (*handler)(void))
{
    if (s_hal) {
        s_hal->attachInterrupt(pin, handler, GPIO_INTR_POSEDGE);
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

void lora_hal_detach_interrupt(uint32_t pin)
{
    if (s_hal) {
        s_hal->detachInterrupt(pin);
    }
}

void lora_hal_delay_ms(uint32_t ms)
{
    if (s_hal) {
        s_hal->delay(ms);
    }
}

void lora_hal_delay_us(uint32_t us)
{
    if (s_hal) {
        s_hal->delayMicroseconds(us);
    }
}

uint32_t lora_hal_millis(void)
{
    if (s_hal) {
        return s_hal->millis();
    }
    return 0;
}

uint32_t lora_hal_micros(void)
{
    if (s_hal) {
        return s_hal->micros();
    }
    return 0;
}

// RadioLib용 HAL 인스턴스 가져오기
// 초기화되지 않은 경우 nullptr 반환
RadioLibHal* lora_hal_get_instance(void)
{
    if (s_hal != nullptr && s_hal->isInitialized()) {
        return s_hal;
    }
    return nullptr;
}

} // extern "C"
