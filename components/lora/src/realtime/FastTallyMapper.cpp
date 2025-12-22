/**
 * @file FastTallyMapper.cpp
 * @brief 고성능 Tally 매퍼 구현
 */

#include "realtime/FastTallyMapper.h"
#include "log.h"
#include "log_tags.h"
#include <string.h>

// SwitcherManager는 C/C++ 혼용 가능
#include "SwitcherManager.h"

static const char* TAG = "TallyMap";

// 정적 멤버 초기화
SwitcherConfig FastTallyMapper::s_switcher_configs_[LORA_MAX_SWITCHERS] = {};
bool FastTallyMapper::s_initialized = false;
uint8_t FastTallyMapper::s_active_switchers = 0;

esp_err_t FastTallyMapper::init(const MappingTable& config)
{
    if (s_initialized) {
        LOG_0(TAG, "FastTallyMapper 이미 초기화됨");
        return ESP_OK;
    }

    LOG_0(TAG, "FastTallyMapper 초기화 시작");

    // 맵핑 테이블 복사 및 검증
    s_active_switchers = 0;
    for (uint8_t i = 0; i < LORA_MAX_SWITCHERS; i++) {
        s_switcher_configs_[i].offset = config.offsets[i];
        s_switcher_configs_[i].camera_count = config.limits[i];
        s_switcher_configs_[i].switcher_id = i;
        s_switcher_configs_[i].last_tally = 0;

        // 모든 스위처 활성화 (uint8_t offset은 항상 0 이상)
        s_switcher_configs_[i].enabled = true;
        s_active_switchers++;
        LOG_0(TAG, "스위처 %d: offset=%d, limit=%d", i, config.offsets[i], config.limits[i]);
    }

    // camera_count가 0이면 topology에서 실제 값 가져오기
    for (uint8_t i = 0; i < LORA_MAX_SWITCHERS; i++) {
        if (s_switcher_configs_[i].enabled && s_switcher_configs_[i].camera_count == 0) {
            uint8_t actual_cameras = 0;
            if (i == 0) {
                switcher_t* sw = SwitcherManager::getHandle(SWITCHER_INDEX_PRIMARY);
                if (sw && SwitcherManager::isConnected(SWITCHER_INDEX_PRIMARY)) {
                    actual_cameras = switcher_get_effective_camera_count(sw);
                }
            } else if (i == 1) {
                switcher_t* sw = SwitcherManager::getHandle(SWITCHER_INDEX_SECONDARY);
                if (sw && SwitcherManager::isConnected(SWITCHER_INDEX_SECONDARY)) {
                    actual_cameras = switcher_get_effective_camera_count(sw);
                }
            }
            if (actual_cameras > 0) {
                s_switcher_configs_[i].camera_count = actual_cameras;
                LOG_1(TAG, "스위처 %d 카메라 수 업데이트: %d개", i, actual_cameras);
            }
        }
    }

    s_initialized = true;
    LOG_0(TAG, "FastTallyMapper 초기화 완료: %d개 스위처 활성화", s_active_switchers);

    return ESP_OK;
}

void FastTallyMapper::updateSwitcherConfig(uint8_t switcher_id, const SwitcherConfig& config)
{
    if (switcher_id >= LORA_MAX_SWITCHERS) {
        LOG_0(TAG, "잘못된 스위처 ID: %d", switcher_id);
        return;
    }

    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return;
    }

    bool was_enabled = s_switcher_configs_[switcher_id].enabled;
    s_switcher_configs_[switcher_id] = config;

    // 활성 스위처 수 업데이트
    if (!was_enabled && config.enabled) {
        s_active_switchers++;
    } else if (was_enabled && !config.enabled) {
        s_active_switchers--;
    }

    LOG_0(TAG, "스위처 %d 설정 업데이트: enabled=%d, offset=%d, cameras=%d",
          switcher_id, config.enabled, config.offset, config.camera_count);
}


void FastTallyMapper::logMappingInfo()
{
    if (!s_initialized) {
        LOG_0(TAG, "초기화되지 않음");
        return;
    }

    // 실시간으로 총 채널 수 계산 (항상 topology에서 가져오기)
    uint8_t total_channels = 0;
    for (uint8_t i = 0; i < LORA_MAX_SWITCHERS; i++) {
        if (s_switcher_configs_[i].enabled) {
            uint8_t actual_cameras = 0;

            // 항상 topology에서 실제 카메라 수 가져오기
            if (i == 0) {
                switcher_t* sw = SwitcherManager::getHandle(SWITCHER_INDEX_PRIMARY);
                if (sw && SwitcherManager::isConnected(SWITCHER_INDEX_PRIMARY)) {
                    actual_cameras = switcher_get_effective_camera_count(sw);
                }
            } else if (i == 1) {
                switcher_t* sw = SwitcherManager::getHandle(SWITCHER_INDEX_SECONDARY);
                if (sw && SwitcherManager::isConnected(SWITCHER_INDEX_SECONDARY)) {
                    actual_cameras = switcher_get_effective_camera_count(sw);
                }
            }

            // 설정된 값도 업데이트
            if (actual_cameras > 0) {
                s_switcher_configs_[i].camera_count = actual_cameras;
            }

            uint8_t end_channel = s_switcher_configs_[i].offset + actual_cameras;
            if (end_channel > total_channels) {
                total_channels = end_channel;
            }
        }
    }

    LOG_0(TAG, "");
    LOG_0(TAG, "=================================");
    LOG_0(TAG, "Tally 맵핑 정보");
    LOG_0(TAG, "전체 채널 수: %d", total_channels);
    LOG_0(TAG, "활성 스위처: %d", s_active_switchers);

    for (uint8_t i = 0; i < LORA_MAX_SWITCHERS; i++) {
        if (s_switcher_configs_[i].enabled) {
            // 실제 카메라 개수는 스위처 토폴로지에서 가져옴
            uint8_t actual_cameras = 0;
            if (i == 0) {
                switcher_t* sw = SwitcherManager::getHandle(SWITCHER_INDEX_PRIMARY);
                if (sw && SwitcherManager::isConnected(SWITCHER_INDEX_PRIMARY)) {
                    actual_cameras = switcher_get_effective_camera_count(sw);
                }
            } else if (i == 1) {
                switcher_t* sw = SwitcherManager::getHandle(SWITCHER_INDEX_SECONDARY);
                if (sw && SwitcherManager::isConnected(SWITCHER_INDEX_SECONDARY)) {
                    actual_cameras = switcher_get_effective_camera_count(sw);
                }
            }
            LOG_0(TAG, "  스위처 %d: 오프셋=%d, 카메라=%d개 (설정=%d)",
                      i, s_switcher_configs_[i].offset, actual_cameras, s_switcher_configs_[i].camera_count);
        }
    }

    // 시각적 맵핑 (한 줄에 10개씩)
    const int CHANNELS_PER_LINE = 10;
    for (int line_start = 1; line_start <= total_channels; line_start += CHANNELS_PER_LINE) {
        int line_end = line_start + CHANNELS_PER_LINE - 1;
        if (line_end > total_channels) line_end = total_channels;

        // CAM 라인
        char cam_line[128] = "CAM: ";
        char* cam_ptr = cam_line + 5;

        for (int i = line_start; i <= line_end; i++) {
            cam_ptr += sprintf(cam_ptr, "%2d ", i);
        }

        // MAP 라인
        char map_line[128] = "MAP: ";
        char* map_ptr = map_line + 5;

        for (int i = line_start; i <= line_end; i++) {
            bool found = false;
            for (uint8_t s = 0; s < LORA_MAX_SWITCHERS; s++) {
                if (s_switcher_configs_[s].enabled) {
                    uint8_t start = s_switcher_configs_[s].offset + 1;
                    uint8_t end = start + s_switcher_configs_[s].camera_count - 1;

                    if (i >= start && i <= end) {
                        map_ptr += sprintf(map_ptr, "%c%-2d",
                                          'A' + s, i - s_switcher_configs_[s].offset);
                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                map_ptr += sprintf(map_ptr, "-- ");
            }
        }

        LOG_0(TAG, "%s", cam_line);
        LOG_0(TAG, "%s", map_line);
    }

    LOG_0(TAG, "=================================");
    LOG_0(TAG, "");
}

MappingTable FastTallyMapper::getCurrentMapping()
{
    MappingTable table = {};

    for (uint8_t i = 0; i < LORA_MAX_SWITCHERS; i++) {
        table.offsets[i] = s_switcher_configs_[i].offset;
        table.limits[i] = s_switcher_configs_[i].camera_count;
        table.channel_to_switcher[i] = s_switcher_configs_[i].enabled ? i : 0xFF;
    }

    table.active_switchers = s_active_switchers;

    return table;
}

bool FastTallyMapper::isInitialized()
{
    return s_initialized;
}

esp_err_t FastTallyMapper::reinit(const MappingTable& config)
{
    LOG_0(TAG, "FastTallyMapper 강제 재초기화");

    // 맵핑 테이블 복사 및 검증
    s_active_switchers = 0;
    for (uint8_t i = 0; i < LORA_MAX_SWITCHERS; i++) {
        s_switcher_configs_[i].offset = config.offsets[i];
        s_switcher_configs_[i].camera_count = config.limits[i];
        s_switcher_configs_[i].switcher_id = i;
        s_switcher_configs_[i].last_tally = 0;

        // 모든 스위처 활성화 (uint8_t offset은 항상 0 이상)
        s_switcher_configs_[i].enabled = true;
        s_active_switchers++;
        LOG_0(TAG, "스위처 %d: offset=%d, limit=%d", i, config.offsets[i], config.limits[i]);
    }

    // camera_count가 0이면 topology에서 실제 값 가져오기
    for (uint8_t i = 0; i < LORA_MAX_SWITCHERS; i++) {
        if (s_switcher_configs_[i].enabled && s_switcher_configs_[i].camera_count == 0) {
            uint8_t actual_cameras = 0;
            if (i == 0) {
                switcher_t* sw = SwitcherManager::getHandle(SWITCHER_INDEX_PRIMARY);
                if (sw && SwitcherManager::isConnected(SWITCHER_INDEX_PRIMARY)) {
                    actual_cameras = switcher_get_effective_camera_count(sw);
                }
            } else if (i == 1) {
                switcher_t* sw = SwitcherManager::getHandle(SWITCHER_INDEX_SECONDARY);
                if (sw && SwitcherManager::isConnected(SWITCHER_INDEX_SECONDARY)) {
                    actual_cameras = switcher_get_effective_camera_count(sw);
                }
            }
            if (actual_cameras > 0) {
                s_switcher_configs_[i].camera_count = actual_cameras;
                LOG_1(TAG, "스위처 %d 카메라 수 업데이트: %d개", i, actual_cameras);
            }
        }
    }

    // 강제 초기화 상태로 설정
    s_initialized = true;
    LOG_0(TAG, "FastTallyMapper 재초기화 완료: %d개 스위처 활성화", s_active_switchers);

    return ESP_OK;
}

// C 래퍼 함수 구현
extern "C" {
    esp_err_t FastTallyMapper_init(const MappingTable* config) {
        if (!config) return ESP_ERR_INVALID_ARG;
        return FastTallyMapper::init(*config);
    }

    uint64_t FastTallyMapper_mapTally(const uint64_t* switcher_tally, uint8_t count) {
        if (!switcher_tally || count == 0) return 0;
        return FastTallyMapper::mapTally(switcher_tally, count);
    }

    uint8_t FastTallyMapper_getTotalChannels(void) {
        return FastTallyMapper::getTotalChannels();
    }

    void FastTallyMapper_logMappingInfo(void) {
        FastTallyMapper::logMappingInfo();
    }
}