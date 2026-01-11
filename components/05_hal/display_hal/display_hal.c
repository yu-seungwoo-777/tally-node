/**
 * @file display_hal.c
 * @brief 디스플레이 HAL 구현
 *
 * SSD1306 OLED 디스플레이를 위한 하드웨어 추상화 계층입니다.
 * - I2C 인터페이스 사용 (U8g2 라이브러리가 실제 통신 담당)
 * - 전원 제어 기능 제공
 * - 핀 설정 조회 제공
 *
 * @note 실제 I2C 통신은 u8g2_esp32_hal에서 처리합니다.
 */

#include "display_hal.h"
#include "PinConfig.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "t_log.h"

static const char* TAG = "05_Display";

// ============================================================================
// I2C 설정 상수
// ============================================================================

/**
 * @brief I2C 마스터 버퍼 비활성화
 *
 * U8g2가 자체 버퍼를 사용하므로 ESP-IDF I2C 드라이버 버퍼는 비활성화합니다.
 */
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0

// ============================================================================
// 내부 상태 변수
// ============================================================================

/** 전원 상태 (true=켜짐, false=꺼짐) */
static bool s_power_on = false;

/** 초기화 완료 여부 */
static bool s_initialized = false;

// ============================================================================
// 공개 API 구현
// ============================================================================

/**
 * @brief 디스플레이 HAL 초기화
 *
 * 디스플레이 HAL을 초기화합니다.
 * 실제 I2C 통신은 U8g2 라이브러리가 담당하므로,
 * 여기서는 상태만 초기화합니다.
 *
 * @return ESP_OK 성공
 */
esp_err_t display_hal_init(void)
{
    if (s_initialized) {
        T_LOGD(TAG, "Already initialized");
        return ESP_OK;
    }

    T_LOGI(TAG, "Initializing Display HAL");
    T_LOGI(TAG, "I2C pins: SDA=%d, SCL=%d, Port=%d",
            EORA_S3_I2C_SDA, EORA_S3_I2C_SCL, EORA_S3_I2C_PORT);

    s_initialized = true;
    s_power_on = true;  // 기본적으로 켜짐 상태

    T_LOGI(TAG, "Display HAL initialized");
    return ESP_OK;
}

/**
 * @brief 디스플레이 HAL 해제
 */
void display_hal_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    T_LOGI(TAG, "Deinitializing Display HAL");

    s_initialized = false;
    T_LOGI(TAG, "Display HAL deinitialized");
}

/**
 * @brief I2C 핀 번호 조회
 *
 * 디스플레이가 사용하는 I2C 핀 번호를 반환합니다.
 *
 * @param out_sda SDA 핀 번호를 저장할 포인터 (NULL 가능)
 * @param out_scl SCL 핀 번호를 저장할 포인터 (NULL 가능)
 */
void display_hal_get_i2c_pins(int *out_sda, int *out_scl)
{
    if (out_sda != NULL) {
        *out_sda = EORA_S3_I2C_SDA;
    }
    if (out_scl != NULL) {
        *out_scl = EORA_S3_I2C_SCL;
    }
}

/**
 * @brief I2C 포트 번호 조회
 *
 * @return I2C 포트 번호
 */
int display_hal_get_i2c_port(void)
{
    return EORA_S3_I2C_PORT;
}

/**
 * @brief 전원 상태 설정
 *
 * 디스플레이 전원 상태를 설정합니다.
 * 실제 하드웨어 전원 제어가 없는 경우 소프트웨어 상태만 저장합니다.
 *
 * @param on true=켜기, false=끄기
 */
void display_hal_set_power(bool on)
{
    if (s_power_on != on) {
        s_power_on = on;
        T_LOGI(TAG, "Power state: %s", on ? "ON" : "OFF");
    }
}

/**
 * @brief 전원 상태 조회
 *
 * @return true 켜짐, false 꺼짐
 */
bool display_hal_get_power(void)
{
    return s_power_on;
}

/**
 * @brief 초기화 여부 확인
 *
 * @return true 초기화됨, false 초기화 안됨
 */
bool display_hal_is_initialized(void)
{
    return s_initialized;
}
