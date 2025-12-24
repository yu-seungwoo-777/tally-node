/**
 * @file tally_tx_app.cpp
 * @brief Tally 송신 앱 구현 (이름 기반: Primary/Secondary)
 */

#include "tally_tx_app.h"
#include "t_log.h"
#include "SwitcherService.h"
#include "NetworkService.h"
#include "SwitcherConfig.h"
#include "LoRaService.h"
#include "TallyTypes.h"
#include "LoRaConfig.h"
#include "esp_netif.h"
#include "esp_event.h"
#include <cstring>

// ============================================================================
// LoRa 송신 헬퍼
// ============================================================================

/**
 * @brief LoRa로 Tally 데이터 송신
 */
static void send_tally_via_lora(const packed_data_t* tally) {
    if (!packed_data_is_valid(tally)) {
        T_LOGW(TAG, "LoRa 송신 스킵: 잘못된 Tally 데이터");
        return;
    }

    char hex_str[16];
    packed_data_to_hex(tally, hex_str, sizeof(hex_str));

    esp_err_t ret = lora_service_send_tally(tally);
    if (ret == ESP_OK) {
        T_LOGI(TAG, "LoRa 송신: [%s] (%d채널)", hex_str, tally->channel_count);
    } else {
        T_LOGE(TAG, "LoRa 송신 실패: [%s] -> %s", hex_str, esp_err_to_name(ret));
    }
}

// ============================================================================
// 태그
// ============================================================================

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
static const char* TAG = "tally_tx_app";
#pragma GCC diagnostic pop
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================================
// 기본 설정 (SwitcherConfig.h 사용)
// ============================================================================

const tally_tx_config_t TALLY_TX_DEFAULT_CONFIG = {
    .switcher1_ip = SWITCHER_PRIMARY_IP,
    .switcher2_ip = nullptr,                      // Secondary 미사용
    .switcher_port = SWITCHER_PRIMARY_PORT,       // 0=기본값 사용
    .camera_limit = SWITCHER_PRIMARY_CAMERA_LIMIT,
    .dual_mode = SWITCHER_DUAL_MODE_ENABLED,
    .secondary_offset = SWITCHER_DUAL_MODE_OFFSET,
    .send_interval_ms = 1000,                     // 1초 간격
    .switcher1_interface = SWITCHER_PRIMARY_INTERFACE,  // Primary 인터페이스
    .switcher2_interface = SWITCHER_SECONDARY_INTERFACE, // Secondary 인터페이스
};

// ============================================================================
// 내부 상태
// ============================================================================

static struct {
    switcher_service_handle_t service;
    tally_tx_config_t config;
    bool running;
    bool initialized;
    packed_data_t last_tally;
} s_app = {
    .service = nullptr,
    .config = TALLY_TX_DEFAULT_CONFIG,
    .running = false,
    .initialized = false,
    .last_tally = {nullptr, 0, 0}
};

// ============================================================================
// 콜백 핸들러
// ============================================================================

static void on_tally_change(void) {
    // Tally 데이터 변경 시 즉시 LoRa 송신
    if (!s_app.initialized || !s_app.service) {
        return;
    }

    packed_data_t tally = switcher_service_get_combined_tally(s_app.service);
    send_tally_via_lora(&tally);
}

static void on_connection_change(connection_state_t state) {
    T_LOGI(TAG, "연결 상태 변경: %s", connection_state_to_string(state));
}

static void on_switcher_change(switcher_role_t role) {
    T_LOGI(TAG, "%s 스위처 변경 감지", switcher_role_to_string(role));
}

// ============================================================================
// 앱 API
// ============================================================================

bool tally_tx_app_init(const tally_tx_config_t* config) {
    if (s_app.initialized) {
        T_LOGW(TAG, "이미 초기화됨");
        return true;
    }

    T_LOGI(TAG, "Tally 송신 앱 초기화 중...");

    // 네트워크 스택 초기화 (LWIP tcpip_mbox 초기화)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "esp_netif_init 실패: %s", esp_err_to_name(ret));
        return false;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        T_LOGE(TAG, "이벤트 루프 생성 실패: %s", esp_err_to_name(ret));
        return false;
    }

    // 1. NetworkService 초기화 (스위처 통신을 위한 네트워크)
    esp_err_t net_ret = network_service_init();
    if (net_ret != ESP_OK) {
        T_LOGE(TAG, "NetworkService 초기화 실패: %s", esp_err_to_name(net_ret));
        return false;
    }
    T_LOGI(TAG, "NetworkService 초기화 완료");

    // 설정 복사
    if (config) {
        s_app.config = *config;
    } else {
        s_app.config = TALLY_TX_DEFAULT_CONFIG;
    }

    // SwitcherService 생성
    s_app.service = switcher_service_create();
    if (!s_app.service) {
        T_LOGE(TAG, "SwitcherService 생성 실패");
        return false;
    }

    // 콜백 설정
    switcher_service_set_tally_callback(s_app.service, on_tally_change);
    switcher_service_set_connection_callback(s_app.service, on_connection_change);
    switcher_service_set_switcher_change_callback(s_app.service, on_switcher_change);

    // Primary 스위처 설정
    if (!switcher_service_set_atem(s_app.service,
                                     SWITCHER_ROLE_PRIMARY,
                                     "Primary",
                                     s_app.config.switcher1_ip,
                                     s_app.config.switcher_port,
                                     s_app.config.camera_limit,
                                     static_cast<tally_network_if_t>(s_app.config.switcher1_interface))) {
        T_LOGE(TAG, "Primary 스위처 설정 실패");
        switcher_service_destroy(s_app.service);
        s_app.service = nullptr;
        return false;
    }

    // Secondary 스위처 설정 (듀얼모드인 경우)
    if (s_app.config.dual_mode && s_app.config.switcher2_ip) {
        if (!switcher_service_set_atem(s_app.service,
                                        SWITCHER_ROLE_SECONDARY,
                                        "Secondary",
                                        s_app.config.switcher2_ip,
                                        s_app.config.switcher_port,
                                        s_app.config.camera_limit,
                                        static_cast<tally_network_if_t>(s_app.config.switcher2_interface))) {
            T_LOGW(TAG, "Secondary 스위처 설정 실패 (싱글모드로 동작)");
        } else {
            // 듀얼모드 설정
            switcher_service_set_dual_mode(s_app.service, true);
            switcher_service_set_secondary_offset(s_app.service, s_app.config.secondary_offset);
        }
    }

    // 서비스 초기화
    if (!switcher_service_initialize(s_app.service)) {
        T_LOGE(TAG, "SwitcherService 초기화 실패");
        switcher_service_destroy(s_app.service);
        s_app.service = nullptr;
        return false;
    }

    // 태스크 시작 (별도 태스크에서 loop 실행)
    if (!switcher_service_start(s_app.service)) {
        T_LOGE(TAG, "SwitcherService 태스크 시작 실패");
        switcher_service_destroy(s_app.service);
        s_app.service = nullptr;
        return false;
    }
    T_LOGI(TAG, "SwitcherService 태스크 시작 (10ms 주기)");

    // LoRa 초기화
    lora_service_config_t lora_config = {
        .frequency = LORA_DEFAULT_FREQ,      // LoRaConfig.h 기본 주파수
        .spreading_factor = LORA_DEFAULT_SF, // LoRaConfig.h 기본 SF
        .coding_rate = LORA_DEFAULT_CR,      // LoRaConfig.h 기본 CR
        .bandwidth = LORA_DEFAULT_BW,        // LoRaConfig.h 기본 BW
        .tx_power = LORA_DEFAULT_TX_POWER,   // LoRaConfig.h 기본 전력
        .sync_word = LORA_DEFAULT_SYNC_WORD  // LoRaConfig.h 기본 SyncWord
    };
    esp_err_t lora_ret = lora_service_init(&lora_config);
    if (lora_ret != ESP_OK) {
        T_LOGW(TAG, "LoRa 초기화 실패: %s", esp_err_to_name(lora_ret));
    } else {
        T_LOGI(TAG, "LoRa 초기화 완료");
    }

    s_app.initialized = true;
    T_LOGI(TAG, "Tally 송신 앱 초기화 완료");

    // 인터페이스 로그
    const char* if1_str = (s_app.config.switcher1_interface == 1) ? "WiFi" :
                          (s_app.config.switcher1_interface == 2) ? "Ethernet" : "Auto";
    const char* if2_str = (s_app.config.switcher2_interface == 1) ? "WiFi" :
                          (s_app.config.switcher2_interface == 2) ? "Ethernet" : "Auto";

    T_LOGI(TAG, "  Primary: %s:%d (if=%s)", s_app.config.switcher1_ip,
             s_app.config.switcher_port > 0 ? s_app.config.switcher_port : 9910,
             if1_str);
    if (s_app.config.dual_mode && s_app.config.switcher2_ip) {
        T_LOGI(TAG, "  Secondary: %s:%d (if=%s, offset: %d)",
                 s_app.config.switcher2_ip,
                 s_app.config.switcher_port > 0 ? s_app.config.switcher_port : 9910,
                 if2_str,
                 s_app.config.secondary_offset);
    }
    T_LOGI(TAG, "  듀얼모드: %s, 송신 간격: %dms",
             s_app.config.dual_mode ? "활성화" : "비활성화",
             s_app.config.send_interval_ms);

    return true;
}

void tally_tx_app_start(void) {
    if (!s_app.initialized) {
        T_LOGE(TAG, "초기화되지 않음");
        return;
    }

    if (s_app.running) {
        T_LOGW(TAG, "이미 실행 중");
        return;
    }

    // LoRa 시작
    lora_service_start();

    s_app.running = true;
    T_LOGI(TAG, "Tally 송신 앱 시작");
}

void tally_tx_app_stop(void) {
    if (!s_app.running) {
        return;
    }

    // LoRa 정지
    lora_service_stop();

    s_app.running = false;
    T_LOGI(TAG, "Tally 송신 앱 정지");
}

void tally_tx_app_deinit(void) {
    tally_tx_app_stop();

    if (s_app.service) {
        switcher_service_destroy(s_app.service);
        s_app.service = nullptr;
    }

    if (s_app.last_tally.data) {
        packed_data_cleanup(&s_app.last_tally);
    }

    // LoRa 정리
    lora_service_deinit();

    // NetworkService 정리
    network_service_deinit();

    s_app.initialized = false;
    T_LOGI(TAG, "Tally 송신 앱 정리 완료");
}

void tally_tx_app_loop(void) {
    // SwitcherService 루프 처리는 내부 태스크에서 자동 실행됨
    // Tally 변경 시 on_tally_change() 콜백을 통해 즉시 LoRa 송신
    (void)s_app;
}

void tally_tx_app_print_status(void) {
    if (!s_app.initialized) {
        T_LOGI(TAG, "상태: 초기화되지 않음");
        return;
    }

    T_LOGI(TAG, "===== Tally 송신 앱 상태 =====");
    T_LOGI(TAG, "실행 중: %s", s_app.running ? "예" : "아니오");

    // Primary 상태
    switcher_status_t primary_status = switcher_service_get_switcher_status(s_app.service, SWITCHER_ROLE_PRIMARY);
    T_LOGI(TAG, "  Primary: %s, 카메라=%d, 업데이트=%dms",
             connection_state_to_string(primary_status.state),
             primary_status.camera_count,
             primary_status.last_update_time);

    // Secondary 상태
    switcher_status_t secondary_status = switcher_service_get_switcher_status(s_app.service, SWITCHER_ROLE_SECONDARY);
    T_LOGI(TAG, "  Secondary: %s, 카메라=%d, 업데이트=%dms",
             connection_state_to_string(secondary_status.state),
             secondary_status.camera_count,
             secondary_status.last_update_time);

    T_LOGI(TAG, "듀얼모드: %s (offset: %d)",
             switcher_service_is_dual_mode_enabled(s_app.service) ? "활성화" : "비활성화",
             switcher_service_get_secondary_offset(s_app.service));

    if (s_app.last_tally.data && s_app.last_tally.data_size > 0) {
        char hex_str[16];
        packed_data_to_hex(&s_app.last_tally, hex_str, sizeof(hex_str));
        T_LOGI(TAG, "마지막 Tally: [%s] (%d채널)",
                 hex_str, s_app.last_tally.channel_count);
    }

    T_LOGI(TAG, "==============================");
}

bool tally_tx_app_is_connected(void) {
    if (!s_app.initialized || !s_app.service) {
        return false;
    }

    // Primary 연결 확인
    switcher_status_t primary_status = switcher_service_get_switcher_status(s_app.service, SWITCHER_ROLE_PRIMARY);
    if (primary_status.state != CONNECTION_STATE_READY &&
        primary_status.state != CONNECTION_STATE_CONNECTED) {
        return false;
    }

    // Secondary 연결 확인 (듀얼모드인 경우)
    if (switcher_service_is_dual_mode_enabled(s_app.service)) {
        switcher_status_t secondary_status = switcher_service_get_switcher_status(s_app.service, SWITCHER_ROLE_SECONDARY);
        if (secondary_status.state != CONNECTION_STATE_READY &&
            secondary_status.state != CONNECTION_STATE_CONNECTED) {
            return false;
        }
    }

    return true;
}
