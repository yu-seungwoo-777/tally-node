/**
 * @file SwitcherManager.cpp
 * @brief 스위처 통합 관리 Manager 구현 (TX 전용)
 */

// TX 모드에서만 빌드
#ifdef DEVICE_MODE_TX

#include "SwitcherManager.h"
#include "log.h"
#include "log_tags.h"
#include "esp_log.h"
#include <cstdio>

extern "C" {
#include "sw_platform.h"
}

// DisplayManager 알림을 위한 extern 선언 (TX 모드에서만)
#ifdef DEVICE_MODE_TX
extern "C" {
    void DisplayManager_onSwitcherConfigChanged(void);
}
#endif

static const char* TAG = TAG_SWITCHER;

// 정적 멤버 초기화
SwitcherManager::SwitcherContext SwitcherManager::s_switchers[SWITCHER_INDEX_MAX] = {};
bool SwitcherManager::s_initialized = false;
SwitcherManager::SwitcherConnectedCallback SwitcherManager::s_connected_callback = nullptr;

// Tally 모니터링 상수
static constexpr uint32_t TALLY_NO_CHANGE_TIMEOUT_MS = 3600000;  // 1시간 (60 * 60 * 1000)

esp_err_t SwitcherManager::init()
{
    if (s_initialized) {
        LOG_0(TAG, "이미 초기화됨");
        return ESP_OK;
    }


    // 각 스위처 초기화
    for (int i = 0; i < SWITCHER_INDEX_MAX; i++) {
        switcher_index_t idx = (switcher_index_t)i;
        SwitcherContext* ctx = &s_switchers[i];

        // ConfigCore에서 설정 로드
        ctx->config = ConfigCore::getSwitcher(idx);
        ctx->handle = nullptr;
        ctx->initialized = false;
        ctx->conn_state = STATE_DISCONNECTED;
        ctx->connect_start_ms = 0;
        ctx->last_reconnect_attempt_ms = 0;
        ctx->was_connected = false;
        ctx->topology_printed = false;

        // Tally 모니터링 변수 초기화
        ctx->last_tally_packed = 0;
        ctx->last_tally_update_ms = 0;
        ctx->tally_monitored = false;

        // 듀얼 모드 체크: 싱글 모드면 Secondary 스킵
        bool dual_mode = ConfigCore::getDualMode();
        if (i == SWITCHER_INDEX_SECONDARY && !dual_mode) {
            LOG_0(TAG, "스위처 %d: 싱글 모드로 비활성화됨", i);
            continue;
        }

        // 인터페이스 타입 문자열
        const char* if_name = "Unknown";
        if (ctx->config.interface == SWITCHER_INTERFACE_WIFI_STA) {
            if_name = "WiFi STA";
        } else if (ctx->config.interface == SWITCHER_INTERFACE_ETHERNET) {
            if_name = "Ethernet";
        }

        LOG_0(TAG, "");
        LOG_0(TAG, "스위처 %s", i == SWITCHER_INDEX_PRIMARY ? "PRIMARY" : "SECONDARY");
        LOG_0(TAG, "- 타입: %s", switcher_type_name(ctx->config.type));
        LOG_0(TAG, "- 인터페이스: %s", if_name);
        LOG_0(TAG, "- 주소: %s:%d", ctx->config.ip, ctx->config.port);

        // 스위처 생성
        if (ctx->config.password[0] != '\0') {
            ctx->handle = switcher_create_with_password(
                ctx->config.type,
                ctx->config.ip,
                ctx->config.port,
                ctx->config.password);
        } else {
            ctx->handle = switcher_create(
                ctx->config.type,
                ctx->config.ip,
                ctx->config.port);
        }

        if (!ctx->handle) {
            LOG_0(TAG, "스위처 %d 생성 실패", i);
            continue;
        }

        // Camera Offset 설정
        switcher_set_camera_offset(ctx->handle, ctx->config.camera_offset);
        LOG_0(TAG, "- Camera Offset: %d", ctx->config.camera_offset);

        // Camera Limit 설정
        switcher_set_camera_limit(ctx->handle, ctx->config.camera_limit);
        LOG_0(TAG, "- Camera Limit: %d", ctx->config.camera_limit);

        LOG_0(TAG, "- 핸들 생성 완료 (연결은 startConnect()에서 시작)");

        ctx->initialized = true;
    }

    s_initialized = true;
    return ESP_OK;
}

void SwitcherManager::startConnect()
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return;
    }

    // 모든 스위처 연결 시작 (비차단)
    for (int i = 0; i < SWITCHER_INDEX_MAX; i++) {
        SwitcherContext* ctx = &s_switchers[i];

        if (!ctx->initialized || !ctx->handle) {
            continue;
        }

        // 연결 상태 초기화
        ctx->conn_state = STATE_DISCONNECTED;
        ctx->connect_start_ms = 0;
        ctx->last_reconnect_attempt_ms = 0;
        ctx->was_connected = false;
        ctx->topology_printed = false;
    }

    LOG_0(TAG, "스위처 연결 시작");
}

void SwitcherManager::loop()
{
    if (!s_initialized) {
        return;
    }

    uint32_t now = sw_platform_millis();

    // 각 스위처 상태 머신 실행
    for (int i = 0; i < SWITCHER_INDEX_MAX; i++) {
        SwitcherContext* ctx = &s_switchers[i];
        const char* sw_name = (i == SWITCHER_INDEX_PRIMARY) ? "PRIMARY" : "SECONDARY";

        if (!ctx->initialized || !ctx->handle) {
            continue;
        }

        switch (ctx->conn_state) {
            case STATE_DISCONNECTED:
                // 재연결 주기 체크 (5초마다)
                if (now - ctx->last_reconnect_attempt_ms >= RECONNECT_INTERVAL_MS) {
                    LOG_0(TAG, "스위처 %s 연결 시도...", sw_name);

                    // 논블로킹 연결 시작
                    int ret = switcher_connect_start(ctx->handle);
                    if (ret == SWITCHER_OK || ret == 1) {
                        // 연결 시작 성공
                        LOG_1(TAG, "스위처 %s 연결 시작", sw_name);
                        ctx->conn_state = STATE_CONNECTING;
                        ctx->connect_start_ms = now;
                    } else {
                        LOG_1(TAG, "스위처 %s 연결 시작 실패", sw_name);
                    }

                    ctx->last_reconnect_attempt_ms = now;
                }
                break;

            case STATE_CONNECTING:
                // 논블로킹 연결 진행 체크
                {
                    int ret = switcher_connect_check(ctx->handle);
                    if (ret == SWITCHER_OK || ret == 1) {
                        // 연결 진행 중 또는 완료 - 프로토콜 메시지 처리를 위해 loop 호출
                        // (예: OBS WebSocket Hello 메시지 수신 및 인증)
                        // ret == 1: TCP/WebSocket 연결 완료, 인증 진행 중
                        // ret == 0: 모든 연결 및 인증 완료
                        switcher_loop(ctx->handle);

                        // 실제 연결 상태 재확인 (인증 완료 여부 포함)
                        bool is_connected = switcher_is_connected(ctx->handle);

                        if (is_connected) {
                            // 완전히 연결됨 (WebSocket + 인증 완료)
                            const char* type_name = switcher_type_name(ctx->config.type);
                            LOG_0(TAG, "스위처 %s (%s) 연결 완료!", sw_name, type_name);
                            ctx->conn_state = STATE_CONNECTED;
                            ctx->was_connected = true;

                            // 콜백은 초기화 완료(토폴로지 출력) 후 STATE_CONNECTED에서 호출
                        } else {
                            // WebSocket 연결됨 but 인증 대기 중 - 계속 진행
                            // connect_check()가 OK를 반환했지만 is_connected가 false인 경우
                            // (예: OBS WebSocket 연결 후 인증 진행 중)
                        }
                    } else if (ret == SWITCHER_ERROR) {
                        // 연결 실패
                        LOG_1(TAG, "스위처 %s 연결 실패", sw_name);
                        ctx->conn_state = STATE_DISCONNECTED;
                        ctx->last_reconnect_attempt_ms = now;
                    }
                    // ret == 1이면 계속 진행 중

                    // 연결 타임아웃 체크 (최대 30초 - 인증 포함)
                    if (ctx->conn_state == STATE_CONNECTING &&
                        now - ctx->connect_start_ms > 30000) {
                        LOG_1(TAG, "스위처 %s 연결 타임아웃", sw_name);
                        ctx->conn_state = STATE_DISCONNECTED;
                        ctx->last_reconnect_attempt_ms = now;
                    }
                }
                break;

            case STATE_CONNECTED:
                {
                    bool is_connected = switcher_is_connected(ctx->handle);
                    if (is_connected) {
                        // 토폴로지 정보 출력 (초기화 완료 후 한 번만)
                        if (!ctx->topology_printed && switcher_is_initialized(ctx->handle)) {
                            switcher_info_t info;
                            if (switcher_get_info(ctx->handle, &info) == SWITCHER_OK) {
                                LOG_0(TAG, "");
                                LOG_0(TAG, "========================================");
                                LOG_0(TAG, "스위처 %s 토폴로지 정보", sw_name);
                                LOG_0(TAG, "========================================");
                                LOG_0(TAG, "제품명: %s", info.product_name);
                                LOG_0(TAG, "카메라 개수: %d", info.num_cameras);
                                LOG_0(TAG, "Mix Effect 수: %d", info.num_mes);

                                // 매핑 정보 출력
                                uint8_t offset = switcher_get_camera_offset(ctx->handle);
                                uint8_t limit = switcher_get_camera_limit(ctx->handle);
                                uint8_t effective = switcher_get_effective_camera_count(ctx->handle);
                                LOG_0(TAG, "Camera Offset: %d", offset);
                                LOG_0(TAG, "Camera Limit: %d", limit);
                                LOG_0(TAG, "Effective Count: %d", effective);

                                // 상태 정보 출력
                                switcher_state_t state;
                                if (switcher_get_state(ctx->handle, &state) == SWITCHER_OK) {
                                    LOG_0(TAG, "연결 상태: %s", state.connected ? "연결됨" : "연결 안됨");
                                    LOG_0(TAG, "Program Input: %d", state.program_input);
                                    LOG_0(TAG, "Preview Input: %d", state.preview_input);
                                    LOG_0(TAG, "Tally Packed: 0x%016llX", (unsigned long long)state.tally_packed);
                                }

                                LOG_0(TAG, "========================================");
                                LOG_0(TAG, "");
                            }
                            ctx->topology_printed = true;

                            // 초기화 완료 후 콜백 호출 (카메라 맵핑 출력)
                            if (s_connected_callback) {
                                s_connected_callback((switcher_index_t)i);
                            }
                        }

                        // 정상 동작 - loop 실행
                        switcher_loop(ctx->handle);

                        // Tally Packed 값 모니터링
                        uint64_t current_tally_packed = switcher_get_tally_packed(ctx->handle);

                        // 모니터링이 시작되지 않았다면 시작
                        if (!ctx->tally_monitored) {
                            ctx->last_tally_packed = current_tally_packed;
                            ctx->last_tally_update_ms = now;
                            ctx->tally_monitored = true;
                            LOG_1(TAG, "스위처 %s Tally 모니터링 시작 (0x%016llX)",
                                     sw_name, (unsigned long long)current_tally_packed);
                        }
                        // Tally 값이 변경되었으면 시간 업데이트
                        else if (current_tally_packed != ctx->last_tally_packed) {
                            ctx->last_tally_packed = current_tally_packed;
                            ctx->last_tally_update_ms = now;
                            LOG_1(TAG, "스위처 %s Tally 변경 감지 (0x%016llX)",
                                     sw_name, (unsigned long long)current_tally_packed);
                        }

                        ctx->was_connected = true;
                    } else {
                        // 연결 끊김 감지
                        if (ctx->was_connected) {
                            LOG_0(TAG, "스위처 %s 연결 끊김", sw_name);

                            // 맵핑 정보 재출력 (연결 끊김 후 현재 맵핑 상태 표시)
                            if (s_connected_callback) {
                                s_connected_callback((switcher_index_t)i);
                            }
                        }

                        ctx->conn_state = STATE_DISCONNECTED;
                        ctx->last_reconnect_attempt_ms = now;  // 즉시 재연결 시도
                        ctx->was_connected = false;
                        ctx->topology_printed = false;  // 재연결 시 토폴로지 재출력

                        // Tally 모니터링 리셋
                        ctx->tally_monitored = false;
                        ctx->last_tally_packed = 0;
                        ctx->last_tally_update_ms = 0;
                    }
                }
                break;
        }
    }

    // Tally Packed 변화 감지 및 재시작 체크
    checkTallyPackedChangeAndRestart();
}

bool SwitcherManager::isConnected(switcher_index_t index)
{
    if (index >= SWITCHER_INDEX_MAX) {
        return false;
    }

    SwitcherContext* ctx = &s_switchers[index];
    if (!ctx->initialized || !ctx->handle) {
        return false;
    }

    return switcher_is_connected(ctx->handle);
}

esp_err_t SwitcherManager::getState(switcher_index_t index, switcher_state_t* state)
{
    if (!state || index >= SWITCHER_INDEX_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    SwitcherContext* ctx = &s_switchers[index];
    if (!ctx->initialized || !ctx->handle) {
        return ESP_FAIL;
    }

    if (switcher_get_state(ctx->handle, state) != SWITCHER_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

uint64_t SwitcherManager::getTallyPacked(switcher_index_t index)
{
    if (index >= SWITCHER_INDEX_MAX) {
        return 0;
    }

    SwitcherContext* ctx = &s_switchers[index];
    if (!ctx->initialized || !ctx->handle) {
        return 0;
    }

    if (!switcher_is_connected(ctx->handle)) {
        return 0;
    }

    // Tally packed 값 가져오기
    uint64_t tally_packed = switcher_get_tally_packed(ctx->handle);

    // 스위처 정보 및 Tally 상세 로그
    uint8_t offset = switcher_get_camera_offset(ctx->handle);
    const char* sw_name = (index == SWITCHER_INDEX_PRIMARY) ? "PRIMARY" : "SECONDARY";

    // 디버그: 스위처별 Tally 정보
    LOG_1(TAG, "SwitcherManager::getTallyPacked() - %s", sw_name);
    LOG_1(TAG, "  - Offset: %d", offset);
    LOG_1(TAG, "  - Tally Packed (from switcher): 0x%016llX", tally_packed);

    // Tally 디코딩 정보
    uint8_t pgm[20], pvw[20];
    uint8_t pgm_count = 0, pvw_count = 0;
    switcher_tally_unpack(ctx->handle, pgm, &pgm_count, pvw, &pvw_count);

    if (pgm_count > 0 || pvw_count > 0) {
        char pgm_str[64] = {0}, pvw_str[64] = {0};
        char* p = pgm_str;
        for (uint8_t i = 0; i < pgm_count && i < 10; i++) {
            if (i > 0) *p++ = ',';
            p += sprintf(p, "%d", pgm[i]);
        }
        p = pvw_str;
        for (uint8_t i = 0; i < pvw_count && i < 10; i++) {
            if (i > 0) *p++ = ',';
            p += sprintf(p, "%d", pvw[i]);
        }
        LOG_1(TAG, "  - Tally State: PGM[%s] / PVW[%s]",
              pgm_count > 0 ? pgm_str : "--",
              pvw_count > 0 ? pvw_str : "--");

        // 바이너리 비트 맵 정보
        LOG_1(TAG, "  - Binary (first 16 bits): 0b%016llb", tally_packed & 0xFFFF);
    }

    return tally_packed;
}

switcher_t* SwitcherManager::getHandle(switcher_index_t index)
{
    if (index >= SWITCHER_INDEX_MAX) {
        return nullptr;
    }

    SwitcherContext* ctx = &s_switchers[index];
    if (!ctx->initialized) {
        return nullptr;
    }

    return ctx->handle;
}

esp_err_t SwitcherManager::cut(switcher_index_t index)
{
    if (index >= SWITCHER_INDEX_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    SwitcherContext* ctx = &s_switchers[index];
    if (!ctx->initialized || !ctx->handle) {
        return ESP_FAIL;
    }

    int ret = switcher_cut(ctx->handle);
    return (ret == SWITCHER_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t SwitcherManager::autoTransition(switcher_index_t index)
{
    if (index >= SWITCHER_INDEX_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    SwitcherContext* ctx = &s_switchers[index];
    if (!ctx->initialized || !ctx->handle) {
        return ESP_FAIL;
    }

    int ret = switcher_auto(ctx->handle);
    return (ret == SWITCHER_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t SwitcherManager::setProgram(switcher_index_t index, uint16_t input)
{
    if (index >= SWITCHER_INDEX_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    SwitcherContext* ctx = &s_switchers[index];
    if (!ctx->initialized || !ctx->handle) {
        return ESP_FAIL;
    }

    int ret = switcher_set_program(ctx->handle, input);
    return (ret == SWITCHER_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t SwitcherManager::setPreview(switcher_index_t index, uint16_t input)
{
    if (index >= SWITCHER_INDEX_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    SwitcherContext* ctx = &s_switchers[index];
    if (!ctx->initialized || !ctx->handle) {
        return ESP_FAIL;
    }

    int ret = switcher_set_preview(ctx->handle, input);
    return (ret == SWITCHER_OK) ? ESP_OK : ESP_FAIL;
}

bool SwitcherManager::isInitialized()
{
    return s_initialized;
}

bool SwitcherManager::isDualMode()
{
    if (!s_initialized) {
        return false;
    }

    // ConfigCore의 dual_mode 설정 반환
    return ConfigCore::getDualMode();
}

uint8_t SwitcherManager::getActiveSwitcherCount()
{
    if (!s_initialized) {
        return 0;
    }

    // 듀얼 모드면 2, 싱글 모드면 1
    return ConfigCore::getDualMode() ? 2 : 1;
}

esp_err_t SwitcherManager::restartAll()
{
    if (!s_initialized) {
        LOG_0(TAG, "재시작 실패: 초기화되지 않음");
        return ESP_ERR_INVALID_STATE;
    }

    LOG_0(TAG, "모든 스위처 재시작 시작...");

    // 기존 핸들 정리
    for (int i = 0; i < SWITCHER_INDEX_MAX; i++) {
        SwitcherContext* ctx = &s_switchers[i];

        if (ctx->handle) {
            LOG_0(TAG, "스위처 %d 핸들 삭제 중...", i);
            switcher_destroy(ctx->handle);
            ctx->handle = nullptr;
            ctx->initialized = false;
        }
    }

    // 새 설정 로드 및 초기화
    for (int i = 0; i < SWITCHER_INDEX_MAX; i++) {
        switcher_index_t index = (switcher_index_t)i;
        SwitcherContext* ctx = &s_switchers[i];

        // ConfigCore에서 설정 로드
        ctx->config = ConfigCore::getSwitcher(index);
        ctx->initialized = false;
        ctx->conn_state = STATE_DISCONNECTED;
        ctx->connect_start_ms = 0;
        ctx->last_reconnect_attempt_ms = 0;
        ctx->was_connected = false;
        ctx->topology_printed = false;

        // Tally 모니터링 변수 초기화
        ctx->last_tally_packed = 0;
        ctx->last_tally_update_ms = 0;
        ctx->tally_monitored = false;

        // 듀얼 모드 체크: 싱글 모드면 Secondary 스킵
        bool dual_mode = ConfigCore::getDualMode();
        if (i == SWITCHER_INDEX_SECONDARY && !dual_mode) {
            LOG_0(TAG, "스위처 %d: 싱글 모드로 비활성화됨", i);
            continue;
        }

        // 인터페이스 타입 문자열
        const char* if_name = "Unknown";
        if (ctx->config.interface == SWITCHER_INTERFACE_WIFI_STA) {
            if_name = "WiFi STA";
        } else if (ctx->config.interface == SWITCHER_INTERFACE_ETHERNET) {
            if_name = "Ethernet";
        }

        LOG_0(TAG, "스위처 %d: %s", i,
                 i == SWITCHER_INDEX_PRIMARY ? "PRIMARY" : "SECONDARY");
        LOG_0(TAG, "  타입: %s", switcher_type_name(ctx->config.type));
        LOG_0(TAG, "  인터페이스: %s", if_name);
        LOG_0(TAG, "  주소: %s:%d", ctx->config.ip, ctx->config.port);

        // 핸들 생성
        if (ctx->config.password[0] != '\0') {
            ctx->handle = switcher_create_with_password(
                ctx->config.type,
                ctx->config.ip,
                ctx->config.port,
                ctx->config.password);
        } else {
            ctx->handle = switcher_create(
                ctx->config.type,
                ctx->config.ip,
                ctx->config.port);
        }

        if (!ctx->handle) {
            LOG_0(TAG, "스위처 %d 생성 실패", i);
            continue;
        }

        // camera_offset 및 camera_limit 설정
        switcher_set_camera_offset(ctx->handle, ctx->config.camera_offset);
        switcher_set_camera_limit(ctx->handle, ctx->config.camera_limit);

        ctx->initialized = true;
        LOG_0(TAG, "스위처 %d 핸들 생성 완료 (camera_offset=%d, camera_limit=%d)",
                 i, ctx->config.camera_offset, ctx->config.camera_limit);
    }

    // 연결 시작
    startConnect();

    LOG_0(TAG, "모든 스위처 재시작 완료");
    return ESP_OK;
}

void SwitcherManager::setConnectedCallback(SwitcherConnectedCallback callback)
{
    s_connected_callback = callback;
}

void SwitcherManager::checkTallyPackedChangeAndRestart()
{
    uint32_t now = sw_platform_millis();
    bool need_restart = false;

    // 모든 스위처의 Tally Packed 변화 확인
    for (int i = 0; i < SWITCHER_INDEX_MAX; i++) {
        SwitcherContext* ctx = &s_switchers[i];
        const char* sw_name = (i == SWITCHER_INDEX_PRIMARY) ? "PRIMARY" : "SECONDARY";

        // 초기화되지 않았거나 연결되지 않은 스위처는 스킵
        if (!ctx->initialized || !ctx->handle || !switcher_is_connected(ctx->handle)) {
            continue;
        }

        // Tally 모니터링이 시작되었는지 확인
        if (!ctx->tally_monitored) {
            continue;
        }

        // 1시간 동안 Tally 값이 변하지 않았는지 확인
        if (now - ctx->last_tally_update_ms >= TALLY_NO_CHANGE_TIMEOUT_MS) {
            LOG_0(TAG, "스위처 %s: 1시간 동안 Tally 값 변화 없음 (0x%016llX)",
                     sw_name, (unsigned long long)ctx->last_tally_packed);
            LOG_0(TAG, "스위처 연결이 끊어진 것으로 간주하고 재시작합니다.");
            need_restart = true;
            break;
        }
    }

    // 재시작 필요하면 restartAll() 호출
    if (need_restart) {
        LOG_0(TAG, "=== TALLLY 무변화 감지로 인한 스위처 재시작 ===");
        restartAll();
    }
}

#endif  // DEVICE_MODE_TX
