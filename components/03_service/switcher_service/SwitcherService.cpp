/**
 * @file SwitcherService.cpp
 * @brief Switcher 서비스 구현 (이름 기반: Primary/Secondary)
 */

#include "SwitcherService.h"
#include "t_log.h"
#include "event_bus.h"
#include "AtemDriver.h"
#include "VmixDriver.h"
#include "ObsDriver.h"
#include "SwitcherConfig.h"
#include "WiFiDriver.h"
#include "EthernetDriver.h"
#include <cstring>

// ============================================================================
// 태그
// ============================================================================

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
static const char* TAG = "SwitcherService";
#pragma GCC diagnostic pop
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================================
// 유틸리티 함수
// ============================================================================

extern "C" {

const char* switcher_role_to_string(switcher_role_t role) {
    switch (role) {
        case SWITCHER_ROLE_PRIMARY:   return "Primary";
        case SWITCHER_ROLE_SECONDARY: return "Secondary";
        default:                      return "Unknown";
    }
}

} // extern "C"

// ============================================================================
// C++ 구현
// ============================================================================

SwitcherService::SwitcherService()
    : primary_()
    , secondary_()
    , dual_mode_enabled_(false)
    , secondary_offset_(1)
    , tally_callback_(nullptr)
    , connection_callback_(nullptr)
    , change_callback_(nullptr)
    , task_handle_(nullptr)
    , task_running_(false)
    , tally_changed_(false)
    , switcher_changed_(false)
    , last_switcher_role_(SWITCHER_ROLE_PRIMARY)
{
    combined_packed_.data = nullptr;
    combined_packed_.data_size = 0;
    combined_packed_.channel_count = 0;
}

SwitcherService::~SwitcherService() {
    stop();  // 태스크 정지
    primary_.cleanup();
    secondary_.cleanup();
    packed_data_cleanup(&combined_packed_);
}

// ============================================================================
// 내부 헬퍼 메서드
// ============================================================================

SwitcherService::SwitcherInfo* SwitcherService::getSwitcherInfo(switcher_role_t role) {
    if (role == SWITCHER_ROLE_PRIMARY) {
        return &primary_;
    } else if (role == SWITCHER_ROLE_SECONDARY) {
        return &secondary_;
    }
    return nullptr;
}

const SwitcherService::SwitcherInfo* SwitcherService::getSwitcherInfo(switcher_role_t role) const {
    if (role == SWITCHER_ROLE_PRIMARY) {
        return &primary_;
    } else if (role == SWITCHER_ROLE_SECONDARY) {
        return &secondary_;
    }
    return nullptr;
}

// ============================================================================
// 초기화
// ============================================================================

bool SwitcherService::initialize() {
    T_LOGI(TAG, "SwitcherService 초기화 (Primary/Secondary 모드)");

    // Primary 스위처 초기화 및 연결 시작
    if (primary_.adapter) {
        if (!primary_.adapter->initialize()) {
            T_LOGE(TAG, "Primary 초기화 실패");
            return false;
        }
        T_LOGI(TAG, "Primary 연결 시작");
        primary_.adapter->connect();
    }

    // Secondary 스위처 초기화 및 연결 시작
    if (secondary_.adapter) {
        if (!secondary_.adapter->initialize()) {
            T_LOGE(TAG, "Secondary 초기화 실패");
            return false;
        }
        T_LOGI(TAG, "Secondary 연결 시작");
        secondary_.adapter->connect();
    }

    T_LOGI(TAG, "SwitcherService 초기화 완료");
    return true;
}

// ============================================================================
// 스위처 설정
// ============================================================================

bool SwitcherService::setAtem(switcher_role_t role, const char* name, const char* ip, uint16_t port, uint8_t camera_limit, tally_network_if_t network_interface) {
    SwitcherInfo* info = getSwitcherInfo(role);
    if (!info) {
        T_LOGE(TAG, "잘못된 역할: %d", static_cast<int>(role));
        return false;
    }

    // 기존 어댑터 정리
    info->cleanup();

    // AtemConfig 설정
    AtemConfig config;
    config.name = name ? name : switcher_role_to_string(role);
    config.ip = ip ? ip : "";
    config.port = (port > 0) ? port : ATEM_DEFAULT_PORT;
    config.camera_limit = camera_limit;
    config.interface = network_interface;

    // AtemDriver 생성
    auto driver = std::unique_ptr<AtemDriver>(new AtemDriver(config));

    // Tally 콜백 설정
    driver->setTallyCallback([this, role]() {
        onSwitcherTallyChange(role);
    });

    // 연결 상태 콜백 설정 (내부 로그는 DEBUG 레벨)
    driver->setConnectionCallback([this, role](connection_state_t state) {
        T_LOGD(TAG, "%s 연결 상태: %s", switcher_role_to_string(role),
                 connection_state_to_string(state));
        if (connection_callback_) {
            connection_callback_(state);
        }
    });

    // 어댑터 설정
    info->adapter = std::move(driver);
    info->last_packed.data = nullptr;
    info->last_packed.data_size = 0;
    info->last_packed.channel_count = 0;
    info->has_changed = false;
    info->last_reconnect_attempt = 0;

    // 인터페이스 로그
    const char* if_str = "Auto";
    if (network_interface == TALLY_NET_WIFI) if_str = "WiFi";
    else if (network_interface == TALLY_NET_ETHERNET) if_str = "Ethernet";

    T_LOGI(TAG, "%s ATEM 스위처 설정됨: %s (%s:%d, if=%s)",
             switcher_role_to_string(role), config.name.c_str(), config.ip.c_str(), config.port, if_str);

    return true;
}

void SwitcherService::removeSwitcher(switcher_role_t role) {
    SwitcherInfo* info = getSwitcherInfo(role);
    if (!info) {
        return;
    }

    T_LOGI(TAG, "%s 스위처 제거", switcher_role_to_string(role));
    info->cleanup();

    // 결합 데이터 캐시 정리
    packed_data_cleanup(&combined_packed_);
}

// ============================================================================
// 루프 처리
// ============================================================================

void SwitcherService::loop() {
    // Primary 처리
    if (primary_.adapter) {
        // 연결 상태 확인 및 자동 재연결
        connection_state_t state = primary_.adapter->getConnectionState();
        if (state == CONNECTION_STATE_DISCONNECTED) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - primary_.last_reconnect_attempt > SWITCHER_RETRY_INTERVAL_MS) {
                T_LOGI(TAG, "Primary 재연결 시도");
                primary_.adapter->connect();
                primary_.last_reconnect_attempt = now;
            }
        }
        // 어댑터 루프 처리
        primary_.adapter->loop();
        // packed 데이터 변경 감지
        checkSwitcherChange(SWITCHER_ROLE_PRIMARY);
    }

    // Secondary 처리
    if (secondary_.adapter) {
        connection_state_t state = secondary_.adapter->getConnectionState();
        if (state == CONNECTION_STATE_DISCONNECTED) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - secondary_.last_reconnect_attempt > SWITCHER_RETRY_INTERVAL_MS) {
                T_LOGI(TAG, "Secondary 재연결 시도");
                secondary_.adapter->connect();
                secondary_.last_reconnect_attempt = now;
            }
        }
        secondary_.adapter->loop();
        checkSwitcherChange(SWITCHER_ROLE_SECONDARY);
    }
}

// ============================================================================
// 태스크 관리
// ============================================================================

bool SwitcherService::start() {
    if (task_running_) {
        T_LOGW(TAG, "태스크가 이미 실행 중");
        return true;
    }

    // 플래그 먼저 설정 (태스크가 즉시 시작되도록)
    task_running_ = true;

    // 정적 태스크 생성
    task_handle_ = xTaskCreateStatic(
        taskFunction,             // 태스크 함수
        "switcher_svc",           // 태스크 이름
        4096,                     // 스택 크기
        this,                     // 파라미터 (this 포인터)
        8,                        // 우선순위 (lwIP보다 높게)
        task_stack_,              // 스택 버퍼
        &task_buffer_             // 태스크 핸들
    );

    if (task_handle_ == nullptr) {
        T_LOGE(TAG, "태스크 생성 실패");
        task_running_ = false;
        return false;
    }

    T_LOGI(TAG, "태스크 시작 (우선순위: 8, 10ms 주기)");
    return true;
}

void SwitcherService::stop() {
    if (!task_running_) {
        return;
    }

    T_LOGI(TAG, "태스크 정지 요청");
    task_running_ = false;

    // 태스크가 스스로 종료하도록 대기
    if (task_handle_ != nullptr) {
        // 태스크 함수 내에서 vTaskDelete(NULL) 호출하므로
        // 여기서는 핸들만 nullptr로 설정
        vTaskDelay(pdMS_TO_TICKS(50));  // 태스크 종료 대기
        task_handle_ = nullptr;
    }

    T_LOGI(TAG, "태스크 정지 완료");
}

void SwitcherService::taskFunction(void* param) {
    SwitcherService* service = static_cast<SwitcherService*>(param);

    T_LOGI(TAG, "태스크 루프 시작");

    while (service->task_running_) {
        service->taskLoop();
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 주기
    }

    T_LOGI(TAG, "태스크 루프 종료");
    vTaskDelete(NULL);
}

void SwitcherService::taskLoop() {
    // ============================================================================
    // 어댑터 처리
    // ============================================================================

    // Primary 처리
    if (primary_.adapter) {
        // 연결 상태 확인 및 자동 재연결
        connection_state_t state = primary_.adapter->getConnectionState();
        if (state == CONNECTION_STATE_DISCONNECTED) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - primary_.last_reconnect_attempt > SWITCHER_RETRY_INTERVAL_MS) {
                T_LOGD(TAG, "Primary 재연결 시도");
                primary_.adapter->connect();
                primary_.last_reconnect_attempt = now;
            }
        }
        // 어댑터 루프 처리
        primary_.adapter->loop();
        // packed 데이터 변경 감지
        checkSwitcherChange(SWITCHER_ROLE_PRIMARY);
    }

    // Secondary 처리
    if (secondary_.adapter) {
        connection_state_t state = secondary_.adapter->getConnectionState();
        if (state == CONNECTION_STATE_DISCONNECTED) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - secondary_.last_reconnect_attempt > SWITCHER_RETRY_INTERVAL_MS) {
                T_LOGD(TAG, "Secondary 재연결 시도");
                secondary_.adapter->connect();
                secondary_.last_reconnect_attempt = now;
            }
        }
        secondary_.adapter->loop();
        checkSwitcherChange(SWITCHER_ROLE_SECONDARY);
    }
}

// ============================================================================
// Tally 데이터 조회
// ============================================================================

packed_data_t SwitcherService::getCombinedTally() const {
    if (primary_.adapter) {
        return combineDualModeTally();
    }

    packed_data_t empty = {nullptr, 0, 0};
    return empty;
}

packed_data_t SwitcherService::combineDualModeTally() const {
    if (!primary_.adapter) {
        packed_data_t empty = {nullptr, 0, 0};
        return empty;
    }

    // Primary 데이터 가져오기
    packed_data_t primary_data = primary_.adapter->getPackedTally();

    if (!packed_data_is_valid(&primary_data)) {
        packed_data_t empty = {nullptr, 0, 0};
        return empty;
    }

    // 싱글모드: Primary만 반환
    if (!dual_mode_enabled_) {
        return primary_data;
    }

    // 듀얼모드: Secondary 처리
    if (!secondary_.adapter) {
        return primary_data;  // Secondary 없으면 Primary만
    }

    packed_data_t secondary_data = secondary_.adapter->getPackedTally();

    if (!packed_data_is_valid(&secondary_data)) {
        return primary_data;  // Secondary 데이터 없으면 Primary만
    }

    // 전체 채널 수 = min(Primary, offset) + Secondary
    uint8_t primary_channels = (primary_data.channel_count < secondary_offset_)
                                ? primary_data.channel_count
                                : secondary_offset_;
    uint8_t total_channels = primary_channels + secondary_data.channel_count;

    // 최대 20채널 제한
    if (total_channels > TALLY_MAX_CHANNELS) {
        total_channels = TALLY_MAX_CHANNELS;
    }

    // 결합 데이터 생성
    packed_data_cleanup(&combined_packed_);
    packed_data_init(&combined_packed_, total_channels);

    // Primary 데이터 복사 (min(primary, offset) 만큼만)
    for (uint8_t i = 0; i < primary_channels; i++) {
        uint8_t flags = packed_data_get_channel(&primary_data, i + 1);
        packed_data_set_channel(&combined_packed_, i + 1, flags);
    }

    // Secondary 데이터 복사 (offset 적용)
    for (uint8_t i = 0; i < secondary_data.channel_count; i++) {
        uint8_t flags = packed_data_get_channel(&secondary_data, i + 1);
        uint8_t target_channel = i + 1 + secondary_offset_;

        if (target_channel <= total_channels) {
            // 기존 값과 OR 결합
            uint8_t existing = packed_data_get_channel(&combined_packed_, target_channel);
            packed_data_set_channel(&combined_packed_, target_channel, existing | flags);
        }
    }

    return combined_packed_;
}

// ============================================================================
// 상태 조회
// ============================================================================

switcher_status_t SwitcherService::getPrimaryStatus() const {
    return getSwitcherStatus(SWITCHER_ROLE_PRIMARY);
}

switcher_status_t SwitcherService::getSecondaryStatus() const {
    return getSwitcherStatus(SWITCHER_ROLE_SECONDARY);
}

switcher_status_t SwitcherService::getSwitcherStatus(switcher_role_t role) const {
    switcher_status_t status;
    switcher_status_init(&status);

    const SwitcherInfo* info = getSwitcherInfo(role);
    if (!info || !info->adapter) {
        return status;
    }

    status.state = info->adapter->getConnectionState();
    status.camera_count = info->adapter->getCameraCount();
    status.last_update_time = info->adapter->getLastUpdateTime();
    status.tally_changed = info->has_changed;

    return status;
}

// ============================================================================
// 설정
// ============================================================================

void SwitcherService::setDualMode(bool enabled) {
    dual_mode_enabled_ = enabled;
    T_LOGI(TAG, "듀얼모드: %s", enabled ? "활성화" : "비활성화");

    if (!enabled) {
        packed_data_cleanup(&combined_packed_);
    }
}

void SwitcherService::setSecondaryOffset(uint8_t offset) {
    secondary_offset_ = offset;
    if (offset > 19) {
        secondary_offset_ = 19;
    }
    T_LOGI(TAG, "Secondary 오프셋: %d", secondary_offset_);
}

void SwitcherService::setTallyCallback(tally_callback_t callback) {
    tally_callback_ = callback;
}

void SwitcherService::setConnectionCallback(connection_callback_t callback) {
    connection_callback_ = callback;
}

void SwitcherService::setSwitcherChangeCallback(SwitcherChangeCallback callback) {
    change_callback_ = callback;
}

// ============================================================================
// 변경 감지
// ============================================================================

void SwitcherService::checkSwitcherChange(switcher_role_t role) {
    SwitcherInfo* info = getSwitcherInfo(role);
    if (!info || !info->adapter) {
        return;
    }

    packed_data_t current_packed = info->adapter->getPackedTally();

    // 변경 감지
    if (!packed_data_equals(&current_packed, &info->last_packed)) {
        // 이전 데이터 해제
        packed_data_cleanup(&info->last_packed);
        // 현재 데이터 복사
        packed_data_copy(&info->last_packed, &current_packed);
        info->has_changed = true;

        // hex 문자열 생성
        if (current_packed.data && current_packed.data_size > 0) {
            char hex_str[64] = "";
            for (uint8_t i = 0; i < current_packed.data_size && i < 10; i++) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%02X", current_packed.data[i]);
                strcat(hex_str, buf);
                if (i < current_packed.data_size - 1) strcat(hex_str, " ");
            }
            T_LOGD(TAG, "%s packed 변경: [%s] (%d채널, %d바이트)",
                     switcher_role_to_string(role), hex_str, current_packed.channel_count, current_packed.data_size);
        }
    }
}

void SwitcherService::onSwitcherTallyChange(switcher_role_t role) {
    // 어댑터에서 변경 알림 받음
    checkSwitcherChange(role);

    // 결합 데이터 업데이트 (듀얼모드인 경우)
    if (dual_mode_enabled_) {
        packed_data_cleanup(&combined_packed_);
    }

    // 병합된 Tally 값 출력 (공통 해석 함수 사용)
    packed_data_t combined = getCombinedTally();
    if (packed_data_is_valid(&combined)) {
        char hex_str[16];
        packed_data_to_hex(&combined, hex_str, sizeof(hex_str));

        char tally_str[64];
        packed_data_format_tally(&combined, tally_str, sizeof(tally_str));

        T_LOGI(TAG, "Combined Tally: [%s] (%d채널, %d바이트) → %s",
                 hex_str, combined.channel_count, combined.data_size, tally_str);

        // 이벤트 버스로 Tally 상태 변경 발행
        event_bus_publish(EVT_TALLY_STATE_CHANGED, combined.data, combined.data_size);
    }

    // 사용자 콜백 호출 (Tally 변경 알림)
    if (tally_callback_) {
        tally_callback_();
    }
}

// ============================================================================
// C 인터페이스 구현 (extern "C" 래퍼)
// ============================================================================

extern "C" {

switcher_service_handle_t switcher_service_create(void) {
    return static_cast<switcher_service_handle_t>(new SwitcherService());
}

void switcher_service_destroy(switcher_service_handle_t handle) {
    if (handle) {
        delete static_cast<SwitcherService*>(handle);
    }
}

bool switcher_service_initialize(switcher_service_handle_t handle) {
    if (!handle) return false;
    return static_cast<SwitcherService*>(handle)->initialize();
}

bool switcher_service_set_atem(switcher_service_handle_t handle,
                                switcher_role_t role,
                                const char* name,
                                const char* ip,
                                uint16_t port,
                                uint8_t camera_limit,
                                tally_network_if_t network_interface) {
    if (!handle) return false;
    return static_cast<SwitcherService*>(handle)->setAtem(role, name, ip, port, camera_limit, network_interface);
}

void switcher_service_remove_switcher(switcher_service_handle_t handle, switcher_role_t role) {
    if (!handle) return;
    static_cast<SwitcherService*>(handle)->removeSwitcher(role);
}

void switcher_service_loop(switcher_service_handle_t handle) {
    if (!handle) return;
    static_cast<SwitcherService*>(handle)->loop();
}

bool switcher_service_start(switcher_service_handle_t handle) {
    if (!handle) return false;
    return static_cast<SwitcherService*>(handle)->start();
}

void switcher_service_stop(switcher_service_handle_t handle) {
    if (!handle) return;
    static_cast<SwitcherService*>(handle)->stop();
}

bool switcher_service_is_running(switcher_service_handle_t handle) {
    if (!handle) return false;
    return static_cast<SwitcherService*>(handle)->isRunning();
}

packed_data_t switcher_service_get_combined_tally(switcher_service_handle_t handle) {
    if (!handle) {
        packed_data_t empty = {nullptr, 0, 0};
        return empty;
    }
    return static_cast<SwitcherService*>(handle)->getCombinedTally();
}

void switcher_service_free_packed_data(packed_data_t* packed) {
    if (packed) {
        packed_data_cleanup(packed);
    }
}

switcher_status_t switcher_service_get_switcher_status(switcher_service_handle_t handle, switcher_role_t role) {
    if (!handle) {
        switcher_status_t status;
        switcher_status_init(&status);
        return status;
    }

    SwitcherService* service = static_cast<SwitcherService*>(handle);
    if (role == SWITCHER_ROLE_PRIMARY) {
        return service->getPrimaryStatus();
    } else if (role == SWITCHER_ROLE_SECONDARY) {
        return service->getSecondaryStatus();
    }

    switcher_status_t status;
    switcher_status_init(&status);
    return status;
}

void switcher_service_set_dual_mode(switcher_service_handle_t handle, bool enabled) {
    if (!handle) return;
    static_cast<SwitcherService*>(handle)->setDualMode(enabled);
}

void switcher_service_set_secondary_offset(switcher_service_handle_t handle, uint8_t offset) {
    if (!handle) return;
    static_cast<SwitcherService*>(handle)->setSecondaryOffset(offset);
}

bool switcher_service_is_dual_mode_enabled(switcher_service_handle_t handle) {
    if (!handle) return false;
    return static_cast<SwitcherService*>(handle)->isDualModeEnabled();
}

uint8_t switcher_service_get_secondary_offset(switcher_service_handle_t handle) {
    if (!handle) return 0;
    return static_cast<SwitcherService*>(handle)->getSecondaryOffset();
}

void switcher_service_set_tally_callback(switcher_service_handle_t handle, tally_callback_t callback) {
    if (!handle) return;
    static_cast<SwitcherService*>(handle)->setTallyCallback(callback);
}

void switcher_service_set_connection_callback(switcher_service_handle_t handle, connection_callback_t callback) {
    if (!handle) return;
    static_cast<SwitcherService*>(handle)->setConnectionCallback(callback);
}

// C 콜백 래퍼 (스태틱 함수로 role을 C 콜백에 전달)
static switcher_service_handle_t g_service_handle = nullptr;
static void (*g_switcher_change_callback)(switcher_role_t role) = nullptr;

void switcher_service_set_switcher_change_callback(switcher_service_handle_t handle,
                                                     void (*callback)(switcher_role_t role)) {
    if (!handle) return;

    g_service_handle = handle;
    g_switcher_change_callback = callback;

    // C++ 람다로 콜백 설정
    static_cast<SwitcherService*>(handle)->setSwitcherChangeCallback(
        [](switcher_role_t role) {
            if (g_switcher_change_callback) {
                g_switcher_change_callback(role);
            }
        }
    );
}

// ============================================================================
// LoRa 패킷 해석 (수신 측)
// ============================================================================

bool switcher_service_parse_lora_packet(switcher_service_handle_t handle,
                                        const uint8_t* packet, size_t length,
                                        packed_data_t* tally) {
    if (!packet || !tally || length < 2) {
        return false;
    }

    // 1단계: 헤더 분리
    uint8_t header = packet[0];
    uint8_t channel_count = 0;
    switch (header) {
        case 0xF1: channel_count = 8; break;
        case 0xF2: channel_count = 12; break;
        case 0xF3: channel_count = 16; break;
        case 0xF4: channel_count = 20; break;
        default: return false;  // 잘못된 헤더
    }

    // 데이터 길이 확인
    uint8_t expected_data_length = (channel_count + 3) / 4;
    if (length < 1 + expected_data_length) {
        return false;
    }

    // 2단계: 데이터 해석
    packed_data_init(tally, channel_count);
    for (uint8_t i = 0; i < expected_data_length && i < tally->data_size; i++) {
        tally->data[i] = packet[1 + i];
    }

    return true;
}

uint8_t switcher_service_get_tally_state(const packed_data_t* tally, uint8_t channel) {
    if (!tally || channel < 1 || channel > tally->channel_count) {
        return 0;  // OFF
    }
    return packed_data_get_channel(tally, channel);
}

void switcher_service_get_pgm_channels(const packed_data_t* tally,
                                       uint8_t* pgm, uint8_t* pgm_count,
                                       uint8_t max_count) {
    if (!tally || !pgm || !pgm_count) {
        if (pgm_count) *pgm_count = 0;
        return;
    }

    uint8_t count = 0;
    for (uint8_t i = 0; i < tally->channel_count && i < max_count; i++) {
        uint8_t state = packed_data_get_channel(tally, i + 1);
        if (state == 0x01 || state == 0x03) {  // PGM or BOTH
            pgm[count++] = i + 1;
        }
    }
    *pgm_count = count;
}

void switcher_service_get_pvw_channels(const packed_data_t* tally,
                                       uint8_t* pvw, uint8_t* pvw_count,
                                       uint8_t max_count) {
    if (!tally || !pvw || !pvw_count) {
        if (pvw_count) *pvw_count = 0;
        return;
    }

    uint8_t count = 0;
    for (uint8_t i = 0; i < tally->channel_count && i < max_count; i++) {
        uint8_t state = packed_data_get_channel(tally, i + 1);
        if (state == 0x02 || state == 0x03) {  // PVW or BOTH
            pvw[count++] = i + 1;
        }
    }
    *pvw_count = count;
}

} // extern "C"
