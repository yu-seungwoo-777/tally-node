/**
 * @file tally_tx_app.cpp
 * @brief Tally 송신 앱 구현 (이름 기반: Primary/Secondary)
 */

#include "tally_tx_app.h"
#include "SwitcherService.h"
#include "NetworkService.h"
#include "SwitcherConfig.h"
#include "t_log.h"
#include <cstring>

// ============================================================================
// 태그
// ============================================================================

static const char* TAG = "tally_tx_app";
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
    uint32_t last_send_time;
    packed_data_t last_tally;
} s_app = {
    .service = nullptr,
    .config = TALLY_TX_DEFAULT_CONFIG,
    .running = false,
    .initialized = false,
    .last_send_time = 0,
    .last_tally = {nullptr, 0, 0}
};

// ============================================================================
// 콜백 핸들러
// ============================================================================

static void on_tally_change(void) {
    // Tally 데이터 변경 시 로그 출력
    T_LOGI(TAG, "Tally 데이터 변경 감지");
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

    s_app.running = true;
    s_app.last_send_time = 0;
    T_LOGI(TAG, "Tally 송신 앱 시작");
}

void tally_tx_app_stop(void) {
    if (!s_app.running) {
        return;
    }

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

    // NetworkService 정리
    network_service_deinit();

    s_app.initialized = false;
    T_LOGI(TAG, "Tally 송신 앱 정리 완료");
}

void tally_tx_app_loop(void) {
    if (!s_app.running || !s_app.service) {
        return;
    }

    // SwitcherService 루프 처리는 내부 태스크에서 자동 실행됨
    // (별도 FreeRTOS 태스크로 10ms 주기 실행)

    // 주기적 Tally 송신
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - s_app.last_send_time >= s_app.config.send_interval_ms) {
        s_app.last_send_time = now;

        // 결합된 Tally 데이터 가져오기
        packed_data_t tally = switcher_service_get_combined_tally(s_app.service);

        if (packed_data_is_valid(&tally)) {
            // 이전 데이터 해제
            if (s_app.last_tally.data) {
                packed_data_cleanup(&s_app.last_tally);
            }

            // 데이터 복사 (caller가 소유한 데이터이므로)
            packed_data_copy(&s_app.last_tally, &tally);

            // LoRa 송신 (TODO: LoRaService 연동)
            // lora_service_send_tally(&tally);

            // hex 문자열 생성
            char hex_str[64] = "";
            for (uint8_t i = 0; i < tally.data_size && i < 8; i++) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%02X", tally.data[i]);
                strcat(hex_str, buf);
                if (i < tally.data_size - 1) strcat(hex_str, " ");
            }

            T_LOGI(TAG, "Tally 송신: [%s] (%d채널, %d바이트)",
                     hex_str, tally.channel_count, tally.data_size);
        } else {
            T_LOGD(TAG, "Tally 데이터 유효하지 않음 (대기 중...)");
        }
    }
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
        char hex_str[64] = "";
        for (uint8_t i = 0; i < s_app.last_tally.data_size && i < 8; i++) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%02X", s_app.last_tally.data[i]);
            strcat(hex_str, buf);
            if (i < s_app.last_tally.data_size - 1) strcat(hex_str, " ");
        }
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
