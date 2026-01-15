/**
 * @file SwitcherService.cpp
 * @brief Switcher 서비스 구현 (이름 기반: Primary/Secondary)
 */

#include "switcher_service.h"
#include "t_log.h"
#include "event_bus.h"
#include "atem_driver.h"
#include "vmix_driver.h"
#include "NVSConfig.h"
#include "error_macros.h"
#include <cstring>

// =============================================================================
// Switcher 재연결 간격 (하드코딩)
// =============================================================================

#define SWITCHER_RETRY_INTERVAL_MS   5000        ///< 스위처 재연결 시도 간격 (5초)
#define SWITCHER_REFRESH_NO_CHANGE_MS 3600000    ///< Packed 변화 없을 때 refresh 간격 (1시간)

// ============================================================================
// 태그
// ============================================================================

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
static const char* TAG = "03_Switcher";
#pragma GCC diagnostic pop

// 전역 인스턴스 포인터 (이벤트 핸들러에서 사용)
static SwitcherService* g_switcher_service_instance = nullptr;

// 정적 멤버 정의
char SwitcherService::s_cached_eth_ip[16] = "";
char SwitcherService::s_cached_wifi_sta_ip[16] = "";

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
    , combined_packed_(TALLY_MAX_CHANNELS)  // RAII 자동 초기화
{
    // 전역 인스턴스 포인터 설정
    g_switcher_service_instance = this;
}

SwitcherService::~SwitcherService() {
    stop();  // 태스크 정지
    primary_.cleanup();
    secondary_.cleanup();
    // combined_packed_은 자동 정리 (RAII)

    // 전역 인스턴스 포인터 해제
    g_switcher_service_instance = nullptr;
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
// 이벤트 핸들러
// ============================================================================

/**
 * @brief 설정 데이터 변경 이벤트 핸들러
 *
 * 듀얼 모드, 오프셋, 또는 스위처 설정(IP, Port, Interface 등) 변경 시 재연결 트리거
 */
static esp_err_t onConfigDataEvent(const event_data_t* event) {
    RETURN_ERR_IF_NULL(event);

    if (!g_switcher_service_instance) {
        return ESP_ERR_INVALID_STATE;
    }

    const config_data_event_t* config = (const config_data_event_t*)event->data;

    // public 메서드를 통해 설정 변경 확인 및 재연결 처리
    g_switcher_service_instance->checkConfigAndReconnect(config);

    return ESP_OK;
}

/**
 * @brief 네트워크 상태 변경 이벤트 핸들러 (EVT_NETWORK_STATUS_CHANGED)
 *
 * Ethernet/WiFi IP를 캐시하여 AtemDriver 설정에 사용
 * 네트워크 연결 시 스위처가 미리 생성되어 있으면 재설정
 */
static esp_err_t onNetworkStatusEvent(const event_data_t* event) {
    RETURN_ERR_IF_NULL(event);

    const network_status_event_t* net_status = (const network_status_event_t*)event->data;

    // public static 메서드를 통해 캐시 갱신
    const char* eth_ip = (net_status->eth_connected && net_status->eth_ip[0] != '\0')
                         ? net_status->eth_ip : nullptr;
    const char* wifi_ip = (net_status->sta_connected && net_status->sta_ip[0] != '\0')
                          ? net_status->sta_ip : nullptr;

    bool needs_reconfigure = SwitcherService::updateNetworkIPCache(eth_ip, wifi_ip);

    // 새 네트워크 연결이 감지되고 스위처가 있으면 재설정
    if (needs_reconfigure && g_switcher_service_instance) {
        g_switcher_service_instance->reconfigureSwitchersForNetwork();
    }

    return ESP_OK;
}

// ============================================================================
// 초기화
// ============================================================================

bool SwitcherService::init() {
    T_LOGI(TAG, "SwitcherService init (Primary/Secondary mode)");

    // Primary 스위처 초기화 및 연결 시작
    if (primary_.adapter) {
        if (!primary_.adapter->initialize()) {
            T_LOGE(TAG, "Primary init failed");
            return false;
        }
        primary_.adapter->connect();
    }

    // Secondary 스위처 초기화 및 연결 시작
    if (secondary_.adapter) {
        if (!secondary_.adapter->initialize()) {
            T_LOGE(TAG, "Secondary init failed");
            return false;
        }
        secondary_.adapter->connect();
    }

    T_LOGI(TAG, "SwitcherService init complete");

    // 초기 상태 이벤트 발행
    publishSwitcherStatus();

    return true;
}

// ============================================================================
// 스위처 설정
// ============================================================================

bool SwitcherService::setAtem(switcher_role_t role, const char* name, const char* ip, uint16_t port, uint8_t camera_limit, tally_network_if_t network_interface, bool debug_packet) {
    SwitcherInfo* info = getSwitcherInfo(role);
    if (!info) {
        T_LOGE(TAG, "invalid role: %d", static_cast<int>(role));
        return false;
    }

    // 기존 어댑터 정리
    info->cleanup();

    // camera_limit 저장
    info->camera_limit = camera_limit;

    // AtemConfig 설정
    AtemConfig config;
    config.name = name ? name : switcher_role_to_string(role);
    config.ip = ip ? ip : "";
    config.port = (port > 0) ? port : ATEM_DEFAULT_PORT;
    config.camera_limit = camera_limit;
    config.debug_packet = debug_packet;

    // 인터페이스별 로컬 바인딩 IP 설정 (캐시된 IP 사용)
    if (network_interface == TALLY_NET_ETHERNET) {
        if (s_cached_eth_ip[0] != '\0') {
            config.local_bind_ip = s_cached_eth_ip;
            T_LOGI(TAG, "Ethernet interface using: %s", s_cached_eth_ip);
        } else {
            // Ethernet 선택했지만 연결 안됨 -> WiFi로 폴백
            T_LOGW(TAG, "Ethernet interface selected but not connected");
            if (s_cached_wifi_sta_ip[0] != '\0') {
                config.local_bind_ip = s_cached_wifi_sta_ip;
                T_LOGW(TAG, "  -> fallback to WiFi STA: %s", s_cached_wifi_sta_ip);
            } else {
                T_LOGW(TAG, "  -> WiFi also not connected, using INADDR_ANY (connection may fail)");
            }
        }
    } else if (network_interface == TALLY_NET_WIFI) {
        if (s_cached_wifi_sta_ip[0] != '\0') {
            config.local_bind_ip = s_cached_wifi_sta_ip;
            T_LOGI(TAG, "WiFi STA interface using: %s", s_cached_wifi_sta_ip);
        } else {
            // WiFi 선택했지만 연결 안됨 -> Ethernet으로 폴백
            T_LOGW(TAG, "WiFi STA interface selected but not connected");
            if (s_cached_eth_ip[0] != '\0') {
                config.local_bind_ip = s_cached_eth_ip;
                T_LOGW(TAG, "  -> fallback to Ethernet: %s", s_cached_eth_ip);
            } else {
                T_LOGW(TAG, "  -> Ethernet also not connected, using INADDR_ANY (connection may fail)");
            }
        }
    }
    // TALLY_NET_AUTO인 경우 local_bind_ip는 비워둠 (INADDR_ANY 사용)

    // AtemDriver 생성
    auto driver = std::unique_ptr<AtemDriver>(new AtemDriver(config));

    // Tally 콜백 설정
    driver->setTallyCallback([this, role]() {
        onSwitcherTallyChange(role);
    });

    // 연결 상태 콜백 설정
    driver->setConnectionCallback([this, role](connection_state_t state) {
        // 연결 상태 저장
        SwitcherInfo* info = getSwitcherInfo(role);
        if (info) {
            bool was_connected = info->is_connected;
            bool now_connected = (state == CONNECTION_STATE_READY || state == CONNECTION_STATE_CONNECTED);
            info->is_connected = now_connected;

            // 연결 완료 시 항상 이벤트 발행 (camera_limit 등 최신 상태 반영)
            if (now_connected && !was_connected) {
                publishSwitcherStatus();
            }
        }

        if (connection_callback_) {
            connection_callback_(state);
        }
    });

    // 어댑터 설정
    info->adapter = std::move(driver);
    // last_packed은 RAII로 자동 관리됨
    info->has_changed = false;
    info->last_reconnect_attempt = 0;
    info->last_packed_change_time = 0;  // 첫 Tally 데이터 받을 때까지 대기
    info->is_connected = false;

    // 설정 정보 저장 (이벤트 발행용)
    strncpy(info->type, "ATEM", sizeof(info->type) - 1);
    strncpy(info->ip, config.ip.c_str(), sizeof(info->ip) - 1);
    info->port = config.port;
    info->network_interface = network_interface;

    // 인터페이스 로그
    const char* if_str = "Auto";
    if (network_interface == TALLY_NET_WIFI) if_str = "WiFi";
    else if (network_interface == TALLY_NET_ETHERNET) if_str = "Ethernet";

    T_LOGD(TAG, "%s ATEM switcher configured: %s (%s:%d, if=%s)",
             switcher_role_to_string(role), config.name.c_str(), config.ip.c_str(), config.port, if_str);

    // 설정 변경 후 상태 이벤트 발행
    publishSwitcherStatus();

    return true;
}

bool SwitcherService::setVmix(switcher_role_t role, const char* name, const char* ip, uint16_t port, uint8_t camera_limit) {
    SwitcherInfo* info = getSwitcherInfo(role);
    if (!info) {
        T_LOGE(TAG, "invalid role: %d", static_cast<int>(role));
        return false;
    }

    // 기존 어댑터 정리
    info->cleanup();

    // camera_limit 저장
    info->camera_limit = camera_limit;

    // VmixConfig 설정
    VmixConfig config;
    config.name = name ? name : switcher_role_to_string(role);
    config.ip = ip ? ip : "";
    config.port = (port > 0) ? port : VMIX_DEFAULT_PORT;
    config.camera_limit = camera_limit;

    // VmixDriver 생성
    auto driver = std::unique_ptr<VmixDriver>(new VmixDriver(config));

    // Tally 콜백 설정
    driver->setTallyCallback([this, role]() {
        onSwitcherTallyChange(role);
    });

    // 연결 상태 콜백 설정
    driver->setConnectionCallback([this, role](connection_state_t state) {
        SwitcherInfo* info = getSwitcherInfo(role);
        if (info) {
            bool was_connected = info->is_connected;
            bool now_connected = (state == CONNECTION_STATE_READY || state == CONNECTION_STATE_CONNECTED);
            info->is_connected = now_connected;

            // 연결 완료 시 항상 이벤트 발행 (camera_limit 등 최신 상태 반영)
            if (now_connected && !was_connected) {
                publishSwitcherStatus();
            }
        }

        if (connection_callback_) {
            connection_callback_(state);
        }
    });

    // 어댑터 설정
    info->adapter = std::move(driver);
    // last_packed은 RAII로 자동 관리됨
    info->has_changed = false;
    info->last_reconnect_attempt = 0;
    info->last_packed_change_time = 0;
    info->is_connected = false;

    // 설정 정보 저장
    strncpy(info->type, "vMix", sizeof(info->type) - 1);
    strncpy(info->ip, config.ip.c_str(), sizeof(info->ip) - 1);
    info->port = config.port;

    T_LOGI(TAG, "%s vMix switcher configured: %s (%s:%d)",
            switcher_role_to_string(role), config.name.c_str(), config.ip.c_str(), config.port);

    publishSwitcherStatus();

    return true;
}

void SwitcherService::removeSwitcher(switcher_role_t role) {
    SwitcherInfo* info = getSwitcherInfo(role);
    if (!info) {
        return;
    }

    T_LOGI(TAG, "%s switcher removed", switcher_role_to_string(role));
    info->cleanup();
    // combined_packed_은 자동 정리 (RAII)
}

// ============================================================================
// 루프 처리
// ============================================================================

void SwitcherService::loop() {
    // Primary 처리
    if (primary_.adapter) {
        // 연결 상태 확인 및 자동 재연결 (인터페이스 연결 상태 확인 후)
        connection_state_t state = primary_.adapter->getConnectionState();
        if (state == CONNECTION_STATE_DISCONNECTED) {
            // 인터페이스 연결 상태 확인
            bool interface_connected = isInterfaceConnected(primary_.network_interface);
            if (interface_connected) {
                uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (now - primary_.last_reconnect_attempt > SWITCHER_RETRY_INTERVAL_MS) {
                    T_LOGD(TAG, "Primary reconnect attempt (interface connected)");
                    primary_.adapter->connect();
                    primary_.last_reconnect_attempt = now;
                }
            }
        }
        // 어댑터 루프 처리
        primary_.adapter->loop();
        // packed 데이터 변경 감지
        checkSwitcherChange(SWITCHER_ROLE_PRIMARY);
    }

    // Secondary 처리 (듀얼 모드 활성화 시에만)
    if (dual_mode_enabled_ && secondary_.adapter) {
        connection_state_t state = secondary_.adapter->getConnectionState();
        if (state == CONNECTION_STATE_DISCONNECTED) {
            // 인터페이스 연결 상태 확인
            bool interface_connected = isInterfaceConnected(secondary_.network_interface);
            if (interface_connected) {
                uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (now - secondary_.last_reconnect_attempt > SWITCHER_RETRY_INTERVAL_MS) {
                    T_LOGD(TAG, "Secondary reconnect attempt (interface connected)");
                    secondary_.adapter->connect();
                    secondary_.last_reconnect_attempt = now;
                }
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
        T_LOGW(TAG, "task already running");
        return true;
    }

    // 이벤트 버스 구독 (설정 변경 감지)
    event_bus_subscribe(EVT_CONFIG_DATA_CHANGED, onConfigDataEvent);
    T_LOGD(TAG, "event bus subscribe: EVT_CONFIG_DATA_CHANGED");

    // 이벤트 버스 구독 (네트워크 상태 변경 감지 - IP 캐시용)
    event_bus_subscribe(EVT_NETWORK_STATUS_CHANGED, onNetworkStatusEvent);
    T_LOGD(TAG, "event bus subscribe: EVT_NETWORK_STATUS_CHANGED");

    // 플래그 먼저 설정 (태스크가 즉시 시작되도록)
    task_running_ = true;

    // 정적 태스크 생성 (스택 크기: 4KB → 8KB)
    task_handle_ = xTaskCreateStatic(
        switcher_task,            // 태스크 함수
        "switcher_task",          // 태스크 이름
        8192,                     // 스택 크기 (8KB, 스택 오버플로우 방지)
        this,                     // 파라미터 (this 포인터)
        8,                        // 우선순위 (lwIP보다 높게)
        task_stack_,              // 스택 버퍼
        &task_buffer_             // 태스크 핸들
    );

    if (task_handle_ == nullptr) {
        T_LOGE(TAG, "task create failed");
        task_running_ = false;
        event_bus_unsubscribe(EVT_CONFIG_DATA_CHANGED, onConfigDataEvent);
        return false;
    }

    T_LOGD(TAG, "task start (priority: 8, 10ms period)");
    return true;
}

void SwitcherService::stop() {
    if (!task_running_) {
        return;
    }

    T_LOGI(TAG, "task stop requested");
    task_running_ = false;

    // 태스크가 스스로 종료하도록 대기
    if (task_handle_ != nullptr) {
        // 태스크 함수 내에서 vTaskDelete(NULL) 호출하므로
        // 여기서는 핸들만 nullptr로 설정
        vTaskDelay(pdMS_TO_TICKS(50));  // 태스크 종료 대기
        task_handle_ = nullptr;
    }

    // 이벤트 버스 구독 해제
    event_bus_unsubscribe(EVT_CONFIG_DATA_CHANGED, onConfigDataEvent);
    T_LOGI(TAG, "event bus unsubscribe: EVT_CONFIG_DATA_CHANGED");

    T_LOGI(TAG, "task stop complete");
}

void SwitcherService::switcher_task(void* param) {
    SwitcherService* service = static_cast<SwitcherService*>(param);
    static UBaseType_t last_high_watermark = 0;
    uint32_t loop_count = 0;

    T_LOGI(TAG, "task loop start (stack size: 8192)");

    while (service->task_running_) {
        service->taskLoop();
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 주기

        // 500회마다 스택 사용량 확인 (약 5초마다)
        if (++loop_count >= 500) {
            UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
            if (watermark < 512 || watermark != last_high_watermark) {
                T_LOGD(TAG, "Stack high water mark: %u bytes", watermark);
                last_high_watermark = watermark;
            }
            loop_count = 0;
        }
    }

    T_LOGI(TAG, "task loop end");
    vTaskDelete(NULL);
}

// ============================================================================
// 재연결 API
// ============================================================================

void SwitcherService::reconnectAll() {
    T_LOGI(TAG, "switcher reconnect start");
    if (primary_.adapter && primary_.adapter->getConnectionState() == CONNECTION_STATE_DISCONNECTED) {
        T_LOGD(TAG, "Primary reconnect attempt");
        primary_.adapter->connect();
    }
    if (secondary_.adapter && secondary_.adapter->getConnectionState() == CONNECTION_STATE_DISCONNECTED) {
        T_LOGD(TAG, "Secondary reconnect attempt");
        secondary_.adapter->connect();
    }
}

// ============================================================================
// 설정 변경 시 재연결 트리거 (이벤트 핸들러에서 호출)
// ============================================================================

void SwitcherService::triggerReconnect() {
    T_LOGI(TAG, "switcher reconnect triggered by config change (dual=%d)", dual_mode_enabled_);

    // Primary 재연결
    if (primary_.adapter) {
        connection_state_t state = primary_.adapter->getConnectionState();
        if (state != CONNECTION_STATE_DISCONNECTED) {
            T_LOGI(TAG, "Primary disconnect and reconnect");
            primary_.adapter->disconnect();
            primary_.adapter->connect();
        } else {
            T_LOGI(TAG, "Primary connect attempt");
            primary_.adapter->connect();
        }
    }

    // Secondary 처리
    if (secondary_.adapter) {
        if (dual_mode_enabled_) {
            // 듀얼 모드 ON: 연결되어 있으면 재연결, 끊어져 있으면 연결
            connection_state_t state = secondary_.adapter->getConnectionState();
            if (state != CONNECTION_STATE_DISCONNECTED) {
                T_LOGI(TAG, "Secondary disconnect and reconnect");
                secondary_.adapter->disconnect();
                secondary_.adapter->connect();
            } else {
                T_LOGI(TAG, "Secondary connect attempt");
                secondary_.adapter->connect();
            }
        } else {
            // 듀얼 모드 OFF: 연결되어 있으면 해제
            if (secondary_.adapter->getConnectionState() != CONNECTION_STATE_DISCONNECTED) {
                T_LOGI(TAG, "Dual mode disabled, Secondary disconnected");
                secondary_.adapter->disconnect();
            }
        }
    }
}

// ============================================================================
// 설정 데이터 변경 확인 및 재연결
// ============================================================================

void SwitcherService::checkConfigAndReconnect(const config_data_event_t* config) {
    if (!config) {
        return;
    }

    bool reconnect_needed = false;

    // 어댑터가 없으면 생성 (초기 설정)
    if (!primary_.adapter) {
        T_LOGI(TAG, "Primary adapter created (event-based init)");
        switch (config->primary_type) {
            case 0: // ATEM
            case 1: // OBS (OBS 드라이버가 없으므로 ATEM으로 대체)
                setAtem(SWITCHER_ROLE_PRIMARY, "Primary",
                        config->primary_ip, config->primary_port,
                        config->primary_camera_limit,
                        static_cast<tally_network_if_t>(config->primary_interface),
                        NVS_SWITCHER_PRI_DEBUG_PACKET);
                break;
            case 2: // vMix
                setVmix(SWITCHER_ROLE_PRIMARY, "Primary",
                        config->primary_ip, config->primary_port,
                        config->primary_camera_limit);
                break;
        }
        // 어댑터 초기화 및 연결
        if (primary_.adapter) {
            primary_.adapter->initialize();
            primary_.adapter->connect();
        }
    }

    if (!secondary_.adapter && config->dual_enabled) {
        T_LOGI(TAG, "Secondary adapter created (event-based init)");
        switch (config->secondary_type) {
            case 0: // ATEM
            case 1: // OBS (OBS 드라이버가 없으므로 ATEM으로 대체)
                setAtem(SWITCHER_ROLE_SECONDARY, "Secondary",
                        config->secondary_ip, config->secondary_port,
                        config->secondary_camera_limit,
                        static_cast<tally_network_if_t>(config->secondary_interface),
                        NVS_SWITCHER_SEC_DEBUG_PACKET);
                break;
            case 2: // vMix
                setVmix(SWITCHER_ROLE_SECONDARY, "Secondary",
                        config->secondary_ip, config->secondary_port,
                        config->secondary_camera_limit);
                break;
        }
        // 어댑터 초기화 및 연결
        if (secondary_.adapter) {
            secondary_.adapter->initialize();
            secondary_.adapter->connect();
        }
    }

    // 듀얼 모드 또는 오프셋 변경 확인
    bool dual_changed = (config->dual_enabled != dual_mode_enabled_);
    bool offset_changed = (config->secondary_offset != secondary_offset_);

    if (dual_changed || offset_changed) {
        T_LOGI(TAG, "Dual mode config changed: dual=%d->%d, offset=%d->%d",
                dual_mode_enabled_, config->dual_enabled,
                secondary_offset_, config->secondary_offset);

        // 내부 상태 업데이트
        setDualMode(config->dual_enabled);
        setSecondaryOffset(config->secondary_offset);

        // 듀얼 모드 변경 시에만 재연결 필요 (offset 변경은 로컬 계산용)
        if (dual_changed) {
            reconnect_needed = true;
        }
    }

    // 현재 타입 확인 (문자열 비교)
    auto getCurrentType = [](const char* type_str) -> int {
        if (strcmp(type_str, "ATEM") == 0) return 0;
        if (strcmp(type_str, "vMix") == 0) return 2;
        return 0; // default ATEM
    };

    // Primary 스위처 설정 변경 확인 (타입, IP, Port, Interface, Camera Limit)
    if (primary_.adapter) {
        int current_type = getCurrentType(primary_.type);
        bool type_changed = (current_type != config->primary_type);
        bool ip_changed = (strncmp(config->primary_ip, primary_.ip, sizeof(config->primary_ip)) != 0);
        bool port_changed = (config->primary_port != primary_.port);
        bool interface_changed = (config->primary_interface != primary_.network_interface);
        bool camera_limit_changed = (config->primary_camera_limit != primary_.camera_limit);

        if (type_changed || ip_changed || port_changed || interface_changed || camera_limit_changed) {
            // camera_limit 변경만 있는 경우 별도 로그
            if (camera_limit_changed && !(type_changed || ip_changed || port_changed || interface_changed)) {
                T_LOGI(TAG, "Primary camera_limit changed: %d -> %d",
                        primary_.camera_limit, config->primary_camera_limit);
            }
            const char* if_str = "Auto";
            if (config->primary_interface == 1) if_str = "Ethernet";
            else if (config->primary_interface == 2) if_str = "WiFi";

            T_LOGI(TAG, "Primary switcher config changed: %s -> %s, %s:%d(if=%d) -> %s:%d(if=%s)",
                    primary_.type, (config->primary_type == 0 ? "ATEM" : "vMix"),
                    primary_.ip, primary_.port, primary_.network_interface,
                    config->primary_ip, config->primary_port, if_str);

            // 타입에 따라 올바른 메서드 호출
            switch (config->primary_type) {
                case 0: // ATEM
                    setAtem(SWITCHER_ROLE_PRIMARY, "Primary",
                            config->primary_ip, config->primary_port,
                            config->primary_camera_limit,
                            static_cast<tally_network_if_t>(config->primary_interface),
                            NVS_SWITCHER_PRI_DEBUG_PACKET);
                    break;
                case 2: // vMix
                    setVmix(SWITCHER_ROLE_PRIMARY, "Primary",
                            config->primary_ip, config->primary_port,
                            config->primary_camera_limit);
                    break;
            }

            // 새 어댑터 연결 시작
            if (primary_.adapter) {
                primary_.adapter->connect();
            }
        }
    }

    // Secondary 스위처 설정 변경 확인 (듀얼 모드일 때만)
    if (config->dual_enabled && secondary_.adapter) {
        int current_type = getCurrentType(secondary_.type);
        bool type_changed = (current_type != config->secondary_type);
        bool ip_changed = (strncmp(config->secondary_ip, secondary_.ip, sizeof(config->secondary_ip)) != 0);
        bool port_changed = (config->secondary_port != secondary_.port);
        bool interface_changed = (config->secondary_interface != secondary_.network_interface);
        bool camera_limit_changed = (config->secondary_camera_limit != secondary_.camera_limit);

        if (type_changed || ip_changed || port_changed || interface_changed || camera_limit_changed) {
            // camera_limit 변경만 있는 경우 별도 로그
            if (camera_limit_changed && !(type_changed || ip_changed || port_changed || interface_changed)) {
                T_LOGI(TAG, "Secondary camera_limit changed: %d -> %d",
                        secondary_.camera_limit, config->secondary_camera_limit);
            }
            const char* if_str = "Auto";
            if (config->secondary_interface == 1) if_str = "Ethernet";
            else if (config->secondary_interface == 2) if_str = "WiFi";

            T_LOGI(TAG, "Secondary switcher config changed: %s -> %s, %s:%d(if=%d) -> %s:%d(if=%s)",
                    secondary_.type, (config->secondary_type == 0 ? "ATEM" : "vMix"),
                    secondary_.ip, secondary_.port, secondary_.network_interface,
                    config->secondary_ip, config->secondary_port, if_str);

            // 타입에 따라 올바른 메서드 호출
            switch (config->secondary_type) {
                case 0: // ATEM
                    setAtem(SWITCHER_ROLE_SECONDARY, "Secondary",
                            config->secondary_ip, config->secondary_port,
                            config->secondary_camera_limit,
                            static_cast<tally_network_if_t>(config->secondary_interface),
                            NVS_SWITCHER_SEC_DEBUG_PACKET);
                    break;
                case 2: // vMix
                    setVmix(SWITCHER_ROLE_SECONDARY, "Secondary",
                            config->secondary_ip, config->secondary_port,
                            config->secondary_camera_limit);
                    break;
            }

            // 새 어댑터 연결 시작
            if (secondary_.adapter) {
                secondary_.adapter->connect();
            }
        }
    }

    // 듀얼/오프셋 변경 시 재연결 트리거
    if (reconnect_needed) {
        triggerReconnect();
    }

    // 설정 변경 후 즉시 상태 발행 (camera_limit 변경 반영)
    publishSwitcherStatus();
}

void SwitcherService::taskLoop() {
    // ============================================================================
    // 어댑터 처리
    // ============================================================================

    // Primary 처리
    if (primary_.adapter) {
        // 연결 상태 확인 및 자동 재연결
        connection_state_t state = primary_.adapter->getConnectionState();
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (state == CONNECTION_STATE_DISCONNECTED) {
            // 비정상 연결 끊김 → 5초마다 재연결 시도
            if (now - primary_.last_reconnect_attempt > SWITCHER_RETRY_INTERVAL_MS) {
                T_LOGD(TAG, "Primary reconnect attempt");
                primary_.adapter->connect();
                primary_.last_reconnect_attempt = now;
            }
        }
        else if (state == CONNECTION_STATE_CONNECTED || state == CONNECTION_STATE_READY) {
            // 정상 연결 상태 → Packed 변화 없으면 health refresh
            if (primary_.last_packed_change_time > 0) {
                uint32_t no_change_duration = now - primary_.last_packed_change_time;
                if (no_change_duration > SWITCHER_REFRESH_NO_CHANGE_MS) {
                    T_LOGI(TAG, "Primary: no Tally change for %d min → Health refresh",
                           no_change_duration / 60000);
                    primary_.adapter->disconnect();
                    primary_.adapter->connect();
                    primary_.last_packed_change_time = now;
                    primary_.last_reconnect_attempt = now;
                }
            }
        }

        // 어댑터 루프 처리
        primary_.adapter->loop();

        // 네트워크 스택 오류 확인 (AtemDriver 타임아웃 감지)
        if (primary_.adapter->getType() == SWITCHER_TYPE_ATEM) {
            AtemDriver* atem = static_cast<AtemDriver*>(primary_.adapter.get());
            if (atem->checkAndClearNetworkRestart()) {
                T_LOGE(TAG, "network stack error detected - publishing network restart event");
                network_restart_request_t restart_req = {
                    .type = NETWORK_RESTART_ALL,
                    .ssid = "",
                    .password = ""
                };
                event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
            }
        }

        // packed 데이터 변경 감지
        checkSwitcherChange(SWITCHER_ROLE_PRIMARY);
    }

    // Secondary 처리 (듀얼 모드 활성화 시에만)
    if (dual_mode_enabled_ && secondary_.adapter) {
        // 연결 상태 확인 및 자동 재연결
        connection_state_t state = secondary_.adapter->getConnectionState();
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (state == CONNECTION_STATE_DISCONNECTED) {
            // 비정상 연결 끊김 → 5초마다 재연결 시도
            if (now - secondary_.last_reconnect_attempt > SWITCHER_RETRY_INTERVAL_MS) {
                T_LOGD(TAG, "Secondary reconnect attempt");
                secondary_.adapter->connect();
                secondary_.last_reconnect_attempt = now;
            }
        }
        else if (state == CONNECTION_STATE_CONNECTED || state == CONNECTION_STATE_READY) {
            // 정상 연결 상태 → Packed 변화 없으면 health refresh
            if (secondary_.last_packed_change_time > 0) {
                uint32_t no_change_duration = now - secondary_.last_packed_change_time;
                if (no_change_duration > SWITCHER_REFRESH_NO_CHANGE_MS) {
                    T_LOGI(TAG, "Secondary: no Tally change for %d min → Health refresh",
                           no_change_duration / 60000);
                    secondary_.adapter->disconnect();
                    secondary_.adapter->connect();
                    secondary_.last_packed_change_time = now;
                    secondary_.last_reconnect_attempt = now;
                }
            }
        }

        secondary_.adapter->loop();

        // 네트워크 스택 오류 확인 (AtemDriver 타임아웃 감지)
        if (secondary_.adapter->getType() == SWITCHER_TYPE_ATEM) {
            AtemDriver* atem_sec = static_cast<AtemDriver*>(secondary_.adapter.get());
            if (atem_sec->checkAndClearNetworkRestart()) {
                T_LOGE(TAG, "Secondary network stack error detected - publishing network restart event");
                network_restart_request_t restart_req = {
                    .type = NETWORK_RESTART_ALL,
                    .ssid = "",
                    .password = ""
                };
                event_bus_publish(EVT_NETWORK_RESTART_REQUEST, &restart_req, sizeof(restart_req));
            }
        }

        checkSwitcherChange(SWITCHER_ROLE_SECONDARY);
    }
    else if (!dual_mode_enabled_ && secondary_.adapter &&
             secondary_.adapter->getConnectionState() != CONNECTION_STATE_DISCONNECTED) {
        // 듀얼 모드 비활성화 시 Secondary 연결 해제
        secondary_.adapter->disconnect();
    }

    // ============================================================================
    // 주기적 상태 발행 (1초마다 체크, 5초마다 발행)
    // ============================================================================
    static uint32_t s_last_status_publish = 0;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - s_last_status_publish > 5000) {
        publishSwitcherStatus();
        s_last_status_publish = now;
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
    // 싱글모드: Primary만 반환
    if (!dual_mode_enabled_) {
        if (!primary_.adapter) {
            packed_data_t empty = {nullptr, 0, 0};
            return empty;
        }
        return primary_.adapter->getPackedTally();
    }

    // 듀얼모드: Primary 또는 Secondary 중 하나라도 있으면 처리
    if (!primary_.adapter && !secondary_.adapter) {
        packed_data_t empty = {nullptr, 0, 0};
        return empty;
    }

    // Primary 데이터 가져오기
    packed_data_t primary_data = {nullptr, 0, 0};
    bool has_primary = false;
    if (primary_.adapter) {
        primary_data = primary_.adapter->getPackedTally();
        has_primary = packed_data_is_valid(&primary_data);
    }

    // Secondary 데이터 가져오기
    packed_data_t secondary_data = {nullptr, 0, 0};
    bool has_secondary = false;
    if (secondary_.adapter) {
        secondary_data = secondary_.adapter->getPackedTally();
        has_secondary = packed_data_is_valid(&secondary_data);
    }

    // 둘 다 없으면 빈 값 반환
    if (!has_primary && !has_secondary) {
        packed_data_t empty = {nullptr, 0, 0};
        return empty;
    }

    // Secondary만 있으면 Secondary만 반환
    if (!has_primary && has_secondary) {
        // Secondary를 offset 위치에 맞춰 반환
        uint8_t effective_offset = (secondary_offset_ > 0) ? secondary_offset_ : TALLY_MAX_CHANNELS;

        // Secondary 데이터 중 20채널 내에 들어가는 개수 계산
        uint8_t secondary_fitting = 0;
        for (uint8_t i = 0; i < secondary_data.channel_count; i++) {
            uint8_t target = i + 1 + effective_offset;
            if (target <= TALLY_MAX_CHANNELS) {
                secondary_fitting = i + 1;
            } else {
                break;
            }
        }

        // offset이 0이면 Secondary를 1번부터 매핑
        if (effective_offset >= TALLY_MAX_CHANNELS) {
            // offset이 크면 Secondary를 1번 채널부터 사용
            combined_packed_.resize(secondary_data.channel_count);
            for (uint8_t i = 0; i < secondary_data.channel_count; i++) {
                uint8_t flags = packed_data_get_channel(&secondary_data, i + 1);
                combined_packed_.setChannel(i + 1, flags);
            }
        } else {
            // offset만큼 떨어진 위치에 매핑
            combined_packed_.resize(effective_offset + secondary_fitting);
            for (uint8_t i = 0; i < secondary_fitting; i++) {
                uint8_t flags = packed_data_get_channel(&secondary_data, i + 1);
                uint8_t target_channel = i + 1 + effective_offset;
                combined_packed_.setChannel(target_channel, flags);
            }
        }
        return *combined_packed_.get();
    }

    // 전체 채널 수 계산
    // - Primary는 offset까지만 사용
    // - Secondary는 offset부터 시작 (offset 위치부터 Secondary 1번이 매핑)
    // - 예: offset=4, Primary=4, Secondary=6 → 채널 1-4(P), 5-9(S) = 9채널
    // - 예: offset=1, Primary=4, Secondary=6 → 채널 1(P), 2-6(S, 1번과 겹침) = 6채널

    // 유효한 offset 계산 (0이면 전체 Primary 사용, 즉 싱글모드와 동일)
    uint8_t effective_offset = (secondary_offset_ > 0) ? secondary_offset_ : primary_data.channel_count;

    // Primary가 사용하는 채널 수 (offset으로 제한)
    uint8_t primary_channels = (primary_data.channel_count < effective_offset)
                                ? primary_data.channel_count
                                : effective_offset;

    // Secondary 중 실제로 20채널 내에 들어가는 개수 계산
    uint8_t secondary_fitting = 0;
    for (uint8_t i = 0; i < secondary_data.channel_count; i++) {
        uint8_t target = i + 1 + effective_offset;
        if (target <= TALLY_MAX_CHANNELS) {
            secondary_fitting = i + 1;  // i는 0-based, fitting은 1-based
        } else {
            break;  // 20을 초과하면 종료
        }
    }

    // 최대 사용 채널 번호 계산
    uint8_t max_channel_used = primary_channels;  // Primary 최대 채널
    if (secondary_fitting > 0) {
        uint8_t secondary_max = effective_offset + secondary_fitting;
        if (secondary_max > max_channel_used) {
            max_channel_used = secondary_max;
        }
    }

    // 결합 데이터 생성 (resize로 재초기화)
    combined_packed_.resize(max_channel_used);

    // Primary 데이터 복사
    for (uint8_t i = 0; i < primary_channels; i++) {
        uint8_t flags = packed_data_get_channel(&primary_data, i + 1);
        combined_packed_.setChannel(i + 1, flags);
    }

    // Secondary 데이터 복사 (offset 적용, 20채널 제한)
    for (uint8_t i = 0; i < secondary_fitting; i++) {
        uint8_t flags = packed_data_get_channel(&secondary_data, i + 1);
        uint8_t target_channel = i + 1 + effective_offset;

        // 기존 값과 OR 결합
        uint8_t existing = combined_packed_.getChannel(target_channel);
        combined_packed_.setChannel(target_channel, existing | flags);
    }

    return *combined_packed_.get();
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
    T_LOGI(TAG, "dual mode: %s", enabled ? "enabled" : "disabled");
    // combined_packed_은 자동 정리 (RAII)

    // 설정 변경 후 상태 이벤트 발행
    publishSwitcherStatus();
}

void SwitcherService::setSecondaryOffset(uint8_t offset) {
    secondary_offset_ = offset;
    if (offset > 19) {
        secondary_offset_ = 19;
    }
    T_LOGI(TAG, "Secondary offset: %d", secondary_offset_);
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

    // 변경 감지 (RAII 래퍼 사용)
    if (!packed_data_equals(&current_packed, info->last_packed.get())) {
        // 현재 데이터 복사 (RAII)
        info->last_packed.copyFrom(PackedData());  // 임시 객체로 해제 후 복사
        packed_data_copy(info->last_packed.get(), &current_packed);
        info->has_changed = true;

        // Health refresh 타이머 리셋 (Tally 변화 있으면 reset)
        info->last_packed_change_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // hex 문자열 생성 (DEBUG)
        if (current_packed.data && current_packed.data_size > 0) {
            char hex_str[64] = "";
            for (uint8_t i = 0; i < current_packed.data_size && i < 10; i++) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%02X", current_packed.data[i]);
                strcat(hex_str, buf);
                if (i < current_packed.data_size - 1) strcat(hex_str, " ");
            }
            T_LOGD(TAG, "%s packed changed: [%s] (%d channels, %d bytes)",
                     switcher_role_to_string(role), hex_str, current_packed.channel_count, current_packed.data_size);
        }
    }
}

void SwitcherService::onSwitcherTallyChange(switcher_role_t role) {
    // TODO: 이벤트로 테스트 모드 상태 확인 필요
    // 테스트 모드가 동작 중이면 스위처 Tally를 무시
    // if (tally_test_service_is_running()) {
    //     return;
    // }

    // 어댑터에서 변경 알림 받음
    checkSwitcherChange(role);
    // combined_packed_은 자동 관리 (RAII)

    // 병합된 Tally 값 가져오기
    packed_data_t combined = getCombinedTally();
    if (!packed_data_is_valid(&combined)) {
        return;
    }

    // 병합된 Tally 값 출력
    char hex_str[16];
    packed_data_to_hex(&combined, hex_str, sizeof(hex_str));

    char tally_str[64];
    packed_data_format_tally(&combined, tally_str, sizeof(tally_str));

    T_LOGI(TAG, "Combined Tally: %s", tally_str);
    T_LOGD(TAG, "  raw: [%s] (%d channels, %d bytes)",
             hex_str, combined.channel_count, combined.data_size);

    // ========================================================================
    // 이벤트 발행 (LoRa 송신은 LoRaService가 이벤트 구독하여 처리)
    // ========================================================================

    // stack 변수 사용 (이벤트 버스가 복사)
    tally_event_data_t tally_event;
    memset(&tally_event, 0, sizeof(tally_event));
    tally_event.source = 0;  // 0=Primary (듀얼 모드 시 결합됨)
    tally_event.channel_count = combined.channel_count;
    memcpy(tally_event.tally_data, combined.data, combined.data_size);
    tally_event.tally_value = packed_data_to_uint64(&combined);

    event_bus_publish(EVT_TALLY_STATE_CHANGED, &tally_event, sizeof(tally_event));

    // Tally 변경 시 상태 이벤트도 발행 (웹 서버 등에서 사용)
    publishSwitcherStatus();
}

// ============================================================================
// 네트워크 IP 캐시 관리
// ============================================================================

bool SwitcherService::updateNetworkIPCache(const char* eth_ip, const char* wifi_sta_ip) {
    bool was_empty_eth = (s_cached_eth_ip[0] == '\0');
    bool was_empty_wifi = (s_cached_wifi_sta_ip[0] == '\0');
    bool was_filled_eth = !was_empty_eth;
    bool was_filled_wifi = !was_empty_wifi;
    bool needs_reconfigure = false;

    // Ethernet IP 캐시
    if (eth_ip && eth_ip[0] != '\0') {
        strncpy(s_cached_eth_ip, eth_ip, sizeof(s_cached_eth_ip) - 1);
        s_cached_eth_ip[sizeof(s_cached_eth_ip) - 1] = '\0';
        T_LOGI(TAG, "Ethernet IP cached: %s", s_cached_eth_ip);
        if (was_empty_eth) {
            T_LOGI(TAG, "Ethernet new connection detected, switcher reconfigure needed");
            needs_reconfigure = true;
        }
    } else {
        // Ethernet 연결 해제 - 캐시 비움
        if (was_filled_eth) {
            T_LOGI(TAG, "Ethernet disconnected, cache cleared");
            s_cached_eth_ip[0] = '\0';
            needs_reconfigure = true;  // 폴백을 위해 재설정 필요
        }
    }

    // WiFi STA IP 캐시
    if (wifi_sta_ip && wifi_sta_ip[0] != '\0') {
        strncpy(s_cached_wifi_sta_ip, wifi_sta_ip, sizeof(s_cached_wifi_sta_ip) - 1);
        s_cached_wifi_sta_ip[sizeof(s_cached_wifi_sta_ip) - 1] = '\0';
        T_LOGI(TAG, "WiFi STA IP cached: %s", s_cached_wifi_sta_ip);
        if (was_empty_wifi) {
            T_LOGI(TAG, "WiFi STA new connection detected, switcher reconfigure needed");
            needs_reconfigure = true;
        }
    } else {
        // WiFi 연결 해제 - 캐시 비움
        if (was_filled_wifi) {
            T_LOGI(TAG, "WiFi STA disconnected, cache cleared");
            s_cached_wifi_sta_ip[0] = '\0';
            needs_reconfigure = true;  // 폴백을 위해 재설정 필요
        }
    }

    return needs_reconfigure;
}

void SwitcherService::reconfigureSwitchersForNetwork() {
    // Primary 스위처가 ATEM이고 설정된 인터페이스가 있으면 재설정
    if (primary_.adapter && primary_.adapter->getType() == SWITCHER_TYPE_ATEM) {
        tally_network_if_t iface = static_cast<tally_network_if_t>(primary_.network_interface);
        if (iface != TALLY_NET_AUTO) {
            T_LOGD(TAG, "Primary switcher network reconfigure (if=%d)", iface);
            setAtem(SWITCHER_ROLE_PRIMARY, "Primary",
                    primary_.ip, primary_.port, primary_.camera_limit, iface, NVS_SWITCHER_PRI_DEBUG_PACKET);
            if (primary_.adapter) {
                primary_.adapter->connect();
            }
        }
    }

    // Secondary 스위처가 ATEM이고 설정된 인터페이스가 있으면 재설정
    if (secondary_.adapter && secondary_.adapter->getType() == SWITCHER_TYPE_ATEM) {
        tally_network_if_t iface = static_cast<tally_network_if_t>(secondary_.network_interface);
        if (iface != TALLY_NET_AUTO) {
            T_LOGD(TAG, "Secondary switcher network reconfigure (if=%d)", iface);
            setAtem(SWITCHER_ROLE_SECONDARY, "Secondary",
                    secondary_.ip, secondary_.port, secondary_.camera_limit, iface, NVS_SWITCHER_SEC_DEBUG_PACKET);
            if (secondary_.adapter) {
                secondary_.adapter->connect();
            }
        }
    }
}

void SwitcherService::publishSwitcherStatus() {
    // stack 변수 사용 (이벤트 버스가 복사)
    switcher_status_event_t status;
    memset(&status, 0, sizeof(status));

    // 스위처 상태 이벤트 발행
    status.dual_mode = dual_mode_enabled_;
    status.s1_connected = primary_.is_connected;
    status.s2_connected = secondary_.is_connected;
    status.s1_port = primary_.port;
    status.s2_port = secondary_.port;

    // 문자열 복사 시 null termination 보장
    strncpy(status.s1_type, primary_.type, sizeof(status.s1_type) - 1);
    status.s1_type[sizeof(status.s1_type) - 1] = '\0';
    strncpy(status.s2_type, secondary_.type, sizeof(status.s2_type) - 1);
    status.s2_type[sizeof(status.s2_type) - 1] = '\0';
    strncpy(status.s1_ip, primary_.ip, sizeof(status.s1_ip) - 1);
    status.s1_ip[sizeof(status.s1_ip) - 1] = '\0';
    strncpy(status.s2_ip, secondary_.ip, sizeof(status.s2_ip) - 1);
    status.s2_ip[sizeof(status.s2_ip) - 1] = '\0';

    // Tally 데이터 (개별 상태) - last_packed가 없으면 어댑터에서 직접 가져옴
    if (primary_.adapter) {
        packed_data_t s1_packed;
        if (primary_.last_packed.get()->data) {
            s1_packed = *primary_.last_packed.get();
        } else {
            s1_packed = primary_.adapter->getPackedTally();
        }
        if (s1_packed.data && s1_packed.data_size > 0) {
            status.s1_channel_count = s1_packed.channel_count;
            memcpy(status.s1_tally_data, s1_packed.data,
                   s1_packed.data_size > 8 ? 8 : s1_packed.data_size);
        }
    }
    status.s1_camera_limit = primary_.camera_limit;

    if (secondary_.adapter) {
        packed_data_t s2_packed;
        if (secondary_.last_packed.get()->data) {
            s2_packed = *secondary_.last_packed.get();
        } else {
            s2_packed = secondary_.adapter->getPackedTally();
        }
        if (s2_packed.data && s2_packed.data_size > 0) {
            status.s2_channel_count = s2_packed.channel_count;
            memcpy(status.s2_tally_data, s2_packed.data,
                   s2_packed.data_size > 8 ? 8 : s2_packed.data_size);
        }
    }
    status.s2_camera_limit = secondary_.camera_limit;

    event_bus_publish(EVT_SWITCHER_STATUS_CHANGED, &status, sizeof(status));
}

// ============================================================================
// 인터페이스 연결 상태 확인
// ============================================================================

bool SwitcherService::isInterfaceConnected(uint8_t network_interface) {
    tally_network_if_t iface = static_cast<tally_network_if_t>(network_interface);

    switch (iface) {
        case TALLY_NET_ETHERNET:
            // Ethernet만 확인
            return (s_cached_eth_ip[0] != '\0');

        case TALLY_NET_WIFI:
            // WiFi만 확인
            return (s_cached_wifi_sta_ip[0] != '\0');

        case TALLY_NET_AUTO:
        default:
            // 둘 중 하나라도 연결되어 있으면 true
            return (s_cached_eth_ip[0] != '\0' || s_cached_wifi_sta_ip[0] != '\0');
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
    return static_cast<SwitcherService*>(handle)->init();
}

bool switcher_service_set_atem(switcher_service_handle_t handle,
                                switcher_role_t role,
                                const char* name,
                                const char* ip,
                                uint16_t port,
                                uint8_t camera_limit,
                                tally_network_if_t network_interface,
                                bool debug_packet) {
    if (!handle) return false;
    return static_cast<SwitcherService*>(handle)->setAtem(role, name, ip, port, camera_limit, network_interface, debug_packet);
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

void switcher_service_reconnect_all(switcher_service_handle_t handle) {
    if (!handle) return;
    static_cast<SwitcherService*>(handle)->reconnectAll();
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
