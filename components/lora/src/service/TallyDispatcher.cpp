/**
 * @file TallyDispatcher.cpp
 * @brief Tally 데이터 흐름 제어 구현
 */

#include "service/TallyDispatcher.h"
#include "manager/LoRaManager.h"
#include "protocol/LoRaPacket.h"
#include "realtime/FastTallyMapper.h"
#include "log.h"
#include "log_tags.h"
#include "esp_timer.h"

#ifdef DEVICE_MODE_TX
#include "SwitcherManager.h"
#endif

#ifdef DEVICE_MODE_RX
extern "C" {
#include "ConfigCore.h"
#include "WS2812Core.h"
}
#endif

#include "info/info_manager.h"
#include <string.h>

static const char* TAG = TAG_COMM;

// 정적 멤버 초기화
bool TallyDispatcher::s_initialized = false;

#ifdef DEVICE_MODE_TX
uint64_t TallyDispatcher::s_last_tally_primary = 0;
uint64_t TallyDispatcher::s_last_tally_secondary = 0;
uint64_t TallyDispatcher::s_last_combined_tally = 0;
uint64_t TallyDispatcher::s_last_tx_time = 0;
#endif
#ifndef DEVICE_MODE_TX
uint64_t TallyDispatcher::s_last_rx_time = 0;
#endif

esp_err_t TallyDispatcher::init()
{
    if (s_initialized) {
        LOG_0(TAG, "TallyDispatcher 이미 초기화됨");
        return ESP_OK;
    }

    // LoRa 수신 콜백 등록
    LoRaManager::setReceiveCallback(onLoRaReceived);

#ifdef DEVICE_MODE_TX
    // 스위처 연결 완료 콜백 등록
    SwitcherManager::setConnectedCallback([](switcher_index_t index) {
        LOG_0(TAG, "스위처 연결됨: %d", index);

        // 스위처 연결 후 FastTallyMapper 재초기화
        LOG_0(TAG, "스위처 연결 후 FastTallyMapper 재초기화");
        reinitializeMapper();
    });

    // 스위처 연결 시작
    SwitcherManager::startConnect();

    // 이미 연결된 스위처가 있을 수 있으므로 즉시 맵핑 정보 초기화 시도
    LOG_0(TAG, "FastTallyMapper 초기화 시도");
    logMappingInfo();
#endif

    s_initialized = true;
    LOG_0(TAG, "TallyDispatcher 초기화 완료");
    return ESP_OK;
}

void TallyDispatcher::processTallyChanges()
{
    if (!s_initialized) {
        return;
    }

#ifdef DEVICE_MODE_TX
    // Tally 변경 감지 및 맵핑
    uint64_t tally_primary = SwitcherManager::getTallyPacked(SWITCHER_INDEX_PRIMARY);
    uint64_t tally_secondary = SwitcherManager::getTallyPacked(SWITCHER_INDEX_SECONDARY);

    // 변경 감지
    if (tally_primary != s_last_tally_primary || tally_secondary != s_last_tally_secondary) {
        uint64_t start_time = esp_timer_get_time();

        // 디버그: 각 스위처의 Tally 값 확인
        LOG_0(TAG, "=== Tally 데이터 변경 ===");
        LOG_0(TAG, "Primary Tally   : 0x%016llX", tally_primary);
        LOG_0(TAG, "Secondary Tally : 0x%016llX", tally_secondary);

        // 스위처별 Tally 디코딩 정보
        char pgm_str[64] = {0}, pvw_str[64] = {0};
        uint8_t pgm[16], pvw[16];
        uint8_t pgm_count = 0, pvw_count = 0;

        // Primary 디코딩
        decodeTally(tally_primary, pgm, &pgm_count, pvw, &pvw_count, 8);
        char* p = pgm_str;
        for (uint8_t i = 0; i < pgm_count; i++) {
            if (i > 0) *p++ = ',';
            p += sprintf(p, "%d", pgm[i]);
        }
        p = pvw_str;
        for (uint8_t i = 0; i < pvw_count; i++) {
            if (i > 0) *p++ = ',';
            p += sprintf(p, "%d", pvw[i]);
        }
        LOG_0(TAG, "Primary (A)    : PGM[%s] / PVW[%s]", pgm_str, pvw_str);

        // Secondary 핸들 가져오기
        switcher_t* sw_secondary = SwitcherManager::getHandle(SWITCHER_INDEX_SECONDARY);

        // Secondary 디코딩 (switcher_tally_unpack 사용)
        if (sw_secondary) {
            // 이미 offset이 적용된 디코딩 결과를 사용
            switcher_tally_unpack(sw_secondary, pgm, &pgm_count, pvw, &pvw_count);
            p = pgm_str;
            for (uint8_t i = 0; i < pgm_count; i++) {
                if (i > 0) *p++ = ',';
                p += sprintf(p, "%d", pgm[i]);
            }
            p = pvw_str;
            for (uint8_t i = 0; i < pvw_count; i++) {
                if (i > 0) *p++ = ',';
                p += sprintf(p, "%d", pvw[i]);
            }
            LOG_0(TAG, "Secondary (B)  : PGM[%s] / PVW[%s]", pgm_str, pvw_str);
        } else {
            LOG_0(TAG, "Secondary (B)  : -- / -- (disconnected)");
        }

        uint64_t combined;
        uint8_t channel_count = 0;

        // FastTallyMapper가 초기화된 경우에만 사용
        LOG_0(TAG, "FastTallyMapper 초기화 상태: %s", FastTallyMapper::isInitialized() ? "초기화됨" : "초기화 안됨");

        if (FastTallyMapper::isInitialized()) {
            // 원본 Tally 데이터를 그대로 FastTallyMapper에 전달
            uint64_t switcher_tally[LORA_MAX_SWITCHERS] = { tally_primary, tally_secondary };

            // 디버그: 원본 Tally 데이터 확인
            LOG_0(TAG, "원본 Tally - Primary: 0x%016llX, Secondary: 0x%016llX", switcher_tally[0], switcher_tally[1]);

            // FastTallyMapper가 오프셋 적용 및 결합 수행
            combined = FastTallyMapper::mapTally(switcher_tally, 2);

            // 디버그: 맵핑 후 결과
            LOG_0(TAG, "FastTallyMapper 결과");
            LOG_0(TAG, "Combined Tally   : 0x%016llX", combined);

            // Combined Tally 디코딩
            uint8_t max_channel = FastTallyMapper::getMaxChannel();
            decodeTally(combined, pgm, &pgm_count, pvw, &pvw_count, max_channel);
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
            LOG_0(TAG, "Total (A+B)     : PGM[%s] / PVW[%s] (max_channel=%d)", pgm_str, pvw_str, max_channel);

            // 디버그: 각 채널의 Tally 상태 출력
            LOG_1(TAG, "채널별 Tally 상태 (max=%d):", max_channel);
            for (uint8_t i = 0; i < max_channel; i++) {
                uint8_t tally = (combined >> (i * 2)) & 0x03;
                if (tally != 0) {
                    LOG_1(TAG, "  채널 %2d: tally=0x%02X (PGM=%s, PVW=%s)", i+1, tally,
                              (tally & 0x01) ? "O" : "X",  // PGM은 bit 0 (0x01)
                              (tally & 0x02) ? "O" : "X"); // PVW는 bit 1 (0x02)
                }
            }

            // FastTallyMapper에서 실제 사용하는 최대 채널 수 사용
            channel_count = FastTallyMapper::getMaxChannel();
        } else {
            // FastTallyMapper가 초기화되지 않았다면 기존 방식 사용
            // Primary만 사용 (최대 16채널)
            combined = tally_primary;
            switcher_t* sw_primary = SwitcherManager::getHandle(SWITCHER_INDEX_PRIMARY);
            channel_count = sw_primary ? switcher_get_effective_camera_count(sw_primary) : 0;
            if (channel_count == 0) channel_count = 16;  // 기본값
        }

        // 중복 체크
        if (combined != s_last_combined_tally) {
            // 로깅
            char tally_str[128];
            formatTallyString(combined, tally_str, sizeof(tally_str), channel_count);
            LOG_0(TAG, "Tally 변경: %s (ch=%d, combined=0x%016llX)", tally_str, channel_count, combined);

            // LoRa 패킷 생성 및 전송
            uint8_t packet[LORA_TALLY_PACKET_MAX_SIZE];
            size_t packet_size;

            // 디버그: 패킷 생성 정보
            LOG_0(TAG, "=== LoRa 패킷 송신 ===");
            LOG_0(TAG, "패킷 생성 - Combined: 0x%016llX", combined);
            LOG_0(TAG, "         - Channel Count: %d", channel_count);

            if (FastTallyMapper::isInitialized()) {
                // FastTallyMapper에서 패킷 타입 결정
                uint8_t header = FastTallyMapper::getPacketHeader();
                uint8_t data_length = FastTallyMapper::getDataLength();

                LOG_0(TAG, "패킷 타입: 0x%02X (%d채널, 데이터 %d바이트)",
                      header, FastTallyMapper::getMaxChannel(), data_length);

                // 패킷 생성: [Header][Data...]
                packet[0] = header;
                for (uint8_t i = 0; i < data_length && i < 5; i++) {
                    packet[1 + i] = (combined >> (i * 8)) & 0xFF;
                }
                packet_size = 1 + data_length;
            } else {
                // 기존 방식 (FastTallyMapper 미초기화)
                packet_size = LoRaPacket::createTallyPacket(combined, channel_count, packet, sizeof(packet));
            }

            // 패킷 내용 확인
            if (packet_size > 0) {
                char hex_str[64] = {0};
                char* p = hex_str;
                for (size_t i = 0; i < packet_size && i < 16; i++) {
                    p += sprintf(p, "%02X ", packet[i]);
                }
                LOG_0(TAG, "송신 데이터: [%s] Size: %d bytes", hex_str, packet_size);

                // 패킷 구조 분석
                if (packet_size >= 2) {
                    LOG_0(TAG, "패킷 헤더:");
                    LOG_0(TAG, "  - Type: 0x%02X (%s)", packet[0],
                          packet[0] == 0xF1 ? "TALLY(8CH)" :
                          packet[0] == 0xF2 ? "TALLY(12CH)" :
                          packet[0] == 0xF3 ? "TALLY(16CH)" :
                          packet[0] == 0xF4 ? "TALLY(20CH)" :
                          packet[0] == 0xAA ? "TALLY" :
                          packet[0] == 0xBB ? "STATUS" : "UNKNOWN");

                    // F1-F4 패킷에서는 채널 수가 헤더에 포함됨
                    if (packet[0] == 0xF1) {
                        LOG_0(TAG, "  - Channel Count: 8");
                    } else if (packet[0] == 0xF2) {
                        LOG_0(TAG, "  - Channel Count: 12");
                    } else if (packet[0] == 0xF3) {
                        LOG_0(TAG, "  - Channel Count: 16");
                    } else if (packet[0] == 0xF4) {
                        LOG_0(TAG, "  - Channel Count: 20");
                    } else {
                        LOG_0(TAG, "  - Channel Count: %d", packet[1]);
                    }

                    if (packet_size > 2) {
                        LOG_0(TAG, "  - Tally Data:");
                        char data_hex[32] = {0};
                        p = data_hex;
                        for (size_t i = 2; i < packet_size && i < 10; i++) {
                            p += sprintf(p, "%02X ", packet[i]);
                        }
                        LOG_0(TAG, "    [%s]", data_hex);

                        // Little-endian으로 읽은 값
                        uint64_t received_tally = 0;
                        for (size_t i = 2; i < packet_size && i < 10; i++) {
                            received_tally |= ((uint64_t)packet[i]) << ((i-2) * 8);
                        }
                        LOG_0(TAG, "  - Reconstructed: 0x%016llX", received_tally);
                    }
                }
            }

            if (packet_size > 0) {
                esp_err_t err = LoRaManager::transmit(packet, packet_size);
                if (err == ESP_OK) {
                    s_last_tx_time = esp_timer_get_time();
                    s_last_combined_tally = combined;
                    info_manager_increment_packet_tx();
                } else {
                    info_manager_increment_error_count();
                }
            }
        }

        // 처리 시간 측정
        uint64_t elapsed = esp_timer_get_time() - start_time;
        LOG_1(TAG, "Tally 처리 시간: %llu us", elapsed);

        // 상태 업데이트
        s_last_tally_primary = tally_primary;
        s_last_tally_secondary = tally_secondary;
    }
#endif

    // Watchdog 체크
    uint64_t now = esp_timer_get_time();
#ifdef DEVICE_MODE_TX
    if (s_last_tx_time > 0 && (now - s_last_tx_time) > 30000000) {  // 30초
        // Heartbeat 전송
        uint8_t packet[8];
        size_t size = LoRaPacket::createHeartbeatPacket(packet, sizeof(packet));
        if (size > 0) {
            LoRaManager::transmit(packet, size);
            s_last_tx_time = now;
            LOG_0(TAG, "Heartbeat 전송");
        }
    }
#else
    if (s_last_rx_time > 0 && (now - s_last_rx_time) > 45000000) {  // 45초
        // 수신 모드 재시작
        LoRaManager::startReceive();
        s_last_rx_time = now;
        LOG_0(TAG, "수신 모드 재시작");
    }
#endif
}

void TallyDispatcher::onLoRaReceived(const uint8_t* data, size_t length)
{
    LOG_0(TAG, "LoRa 수신: %d bytes", length);

    // 수신 통계 증가
    info_manager_increment_packet_rx();

#ifndef DEVICE_MODE_TX
    s_last_rx_time = esp_timer_get_time();
#endif

    // LoRa RSSI/SNR 정보를 InfoManager에 업데이트
    lora_status_t lora_status = LoRaManager::getStatus();
    if (lora_status.is_initialized) {
        // InfoManager 단위: 0.1dB (uint32_t)
        // RSSI는 음수일 수 있으므로 보정 필요
        uint32_t rssi_val;
        if (lora_status.rssi < 0) {
            rssi_val = (uint32_t)((lora_status.rssi + 1000) * 10); // -100을 9000으로 저장
        } else {
            rssi_val = (uint32_t)(lora_status.rssi * 10);
        }
        uint32_t snr_val = (uint32_t)(lora_status.snr * 10);

        // InfoManager에 직접 설정
        info_manager_set_lora_rssi(rssi_val);
        info_manager_set_lora_snr(snr_val);

        LOG_1(TAG, "LoRa RSSI/SNR 업데이트: %.1fdBm, %.1fdB", lora_status.rssi, lora_status.snr);
    }

    // 패킷 타입 확인
    lora_packet_type_t packet_type = (lora_packet_type_t)data[0];

    switch (packet_type) {
        case LORA_PACKET_TALLY:
            if (length >= sizeof(tally_packet_header_t)) {
                onTallyPacketReceived(data, length);
            }
            break;

        case LORA_PACKET_CONFIG_CHANGE:
            if (length >= sizeof(config_change_packet_t)) {
                onConfigChangePacketReceived((const config_change_packet_t*)data);
            }
            break;

        case LORA_PACKET_STATUS:
            if (length >= sizeof(status_packet_t)) {
                onStatusPacketReceived((const status_packet_t*)data);
            }
            break;

        default:
            // F1-F4 새로운 Tally 패킷 확인
            if (data[0] == 0xF1 || data[0] == 0xF2 || data[0] == 0xF3 || data[0] == 0xF4) {
                if (length >= 2) {
                    onTallyPacketReceived(data, length);
                }
            } else {
                LOG_0(TAG, "알 수 없는 패킷: 0x%02X", data[0]);
            }
            break;
    }
}

void TallyDispatcher::onTallyPacketReceived(const uint8_t* data, size_t length)
{
    uint64_t combined_tally;
    uint8_t channel_count;

    // 디버그: 원시 패킷 데이터
    LOG_0(TAG, "=== LoRa 패킷 수신 ===");
    char hex_str[64] = {0};
    char* p = hex_str;
    for (size_t i = 0; i < length && i < 16; i++) {
        p += sprintf(p, "%02X ", data[i]);
    }
    LOG_0(TAG, "수신 데이터: [%s] Size: %d bytes", hex_str, length);

    // 패킷 구조 분석
    if (length >= 2) {
        LOG_0(TAG, "패킷 헤더:");
        LOG_0(TAG, "  - Type: 0x%02X (%s)", data[0],
              data[0] == 0xF1 ? "TALLY(8CH)" :
              data[0] == 0xF2 ? "TALLY(12CH)" :
              data[0] == 0xF3 ? "TALLY(16CH)" :
              data[0] == 0xF4 ? "TALLY(20CH)" :
              data[0] == 0xAA ? "TALLY" :
              data[0] == 0xBB ? "STATUS" : "UNKNOWN");

        // F1-F4 패킷에서는 채널 수가 헤더에 포함됨
        if (data[0] == 0xF1) {
            LOG_0(TAG, "  - Channel Count: 8");
        } else if (data[0] == 0xF2) {
            LOG_0(TAG, "  - Channel Count: 12");
        } else if (data[0] == 0xF3) {
            LOG_0(TAG, "  - Channel Count: 16");
        } else if (data[0] == 0xF4) {
            LOG_0(TAG, "  - Channel Count: 20");
        } else {
            LOG_0(TAG, "  - Channel Count: %d", data[1]);
        }

        if (length > 2) {
            LOG_0(TAG, "  - Raw Tally Data:");
            char data_hex[32] = {0};
            p = data_hex;
            for (size_t i = 2; i < length && i < 10; i++) {
                p += sprintf(p, "%02X ", data[i]);
            }
            LOG_0(TAG, "    [%s]", data_hex);
        }
    }

    // 먼저 새로운 F1-F4 패킷인지 확인
    if (LoRaPacket::isNewTallyPacket(data, length)) {
        // 새로운 F1-F4 패킷 파싱
        esp_err_t err = LoRaPacket::parseNewTallyPacket(data, length, &combined_tally, &channel_count);
        if (err != ESP_OK) {
            LOG_0(TAG, "새로운 Tally 패킷 파싱 실패: %d", err);
            return;
        }

        uint8_t header = data[0];
        LOG_0(TAG, "새로운 패킷 수신: 헤더=0x%02X (%d채널, 데이터 %d바이트)",
              header, channel_count, LoRaPacket::getDataLengthFromHeader(header));
    } else {
        // 기존 0xAA 패킷 파싱
        esp_err_t err = LoRaPacket::parseTallyPacket(data, length, &combined_tally, &channel_count);
        if (err != ESP_OK) {
            LOG_0(TAG, "Tally 패킷 파싱 실패: %d", err);
            return;
        }

        // Heartbeat 체크
        if (channel_count == 0) {
            LOG_1(TAG, "Heartbeat 수신");
            return;
        }
    }

    // 디버그: 파싱된 정보
    LOG_0(TAG, "파싱 결과:");
    LOG_0(TAG, "  - Channel Count: %d", channel_count);
    LOG_0(TAG, "  - Combined Tally: 0x%016llX", combined_tally);

    // Combined Tally 디코딩
    char pgm_str[64] = {0}, pvw_str[64] = {0};
    uint8_t pgm[16], pvw[16];
    uint8_t pgm_count = 0, pvw_count = 0;

    decodeTally(combined_tally, pgm, &pgm_count, pvw, &pvw_count, channel_count);
    p = pgm_str;
    for (uint8_t i = 0; i < pgm_count && i < 10; i++) {
        if (i > 0) *p++ = ',';
        p += sprintf(p, "%d", pgm[i]);
    }
    p = pvw_str;
    for (uint8_t i = 0; i < pvw_count && i < 10; i++) {
        if (i > 0) *p++ = ',';
        p += sprintf(p, "%d", pvw[i]);
    }
    LOG_0(TAG, "  - Decoded: PGM[%s] / PVW[%s]", pgm_str, pvw_str);

    // Tally 데이터 처리
    decodeAndDistributeTally(combined_tally, channel_count);
}

void TallyDispatcher::onConfigChangePacketReceived(const config_change_packet_t* packet)
{
    LOG_0(TAG, "설정 변경: %.1f MHz, Sync: 0x%02X", packet->frequency, packet->sync_word);

    // 설정 적용
    if (packet->frequency > 0) {
        LoRaManager::setFrequency(packet->frequency);
    }
    if (packet->sync_word > 0) {
        LoRaManager::setSyncWord(packet->sync_word);
    }

    // 수신 모드 재시작
    LoRaManager::startReceive();

    // NVS에 저장 (구현 필요)
    LOG_0(TAG, "설정 변경 완료");
}

void TallyDispatcher::onStatusPacketReceived(const status_packet_t* packet)
{
    LOG_0(TAG, "상태 수신: Device %d, Battery: %d%%, RSSI: %d dBm",
          packet->device_id, packet->battery_level, packet->rssi);

    // TODO: DeviceManager에 상태 정보 전달
}

void TallyDispatcher::decodeAndDistributeTally(uint64_t combined_tally, uint8_t channel_count)
{
    // DisplayManager 업데이트
    updateDisplayData(combined_tally, channel_count);

#ifdef DEVICE_MODE_RX
    // LED 업데이트
    updateLedData(combined_tally, channel_count);
#endif
}

void TallyDispatcher::updateDisplayData(uint64_t combined_tally, uint8_t channel_count)
{
    uint8_t pgm[32], pvw[32];
    uint8_t pgm_count = 0, pvw_count = 0;

    decodeTally(combined_tally, pgm, &pgm_count, pvw, &pvw_count, channel_count);

    // 디버그: 디코딩 결과
    char pgm_str[64] = {0};
    char pvw_str[64] = {0};
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

    LOG_0(TAG, "=== Display 업데이트 ===");
    LOG_0(TAG, "Combined: 0x%016llX", combined_tally);
    LOG_0(TAG, "디코딩 결과 - PGM: %s / PVW: %s", pgm_str, pvw_str);
    LOG_0(TAG, "DisplayManager 호출 - setTallyData + updateTallyData");

    // DisplayManager C 인터페이스 호출
    DisplayManager_setTallyData(pgm, pgm_count, pvw, pvw_count);
    DisplayManager_updateTallyData(pgm, pgm_count, pvw, pvw_count, channel_count);
}

#ifdef DEVICE_MODE_RX
void TallyDispatcher::updateLedData(uint64_t combined_tally, uint8_t channel_count)
{
    uint8_t camera_id = ConfigCore::getCameraId();

    if (camera_id > 0 && camera_id <= channel_count) {
        uint8_t camera_offset = (camera_id - 1) * 2;
        uint8_t tally_state = (combined_tally >> camera_offset) & 0x03;

        switch (tally_state) {
            case 0x02:  // Preview (ATEM 표준: 10)
                LOG_0(TAG, "LED: 초록색 (PVW)");
                WS2812Core_setState(WS2812_PREVIEW);
                break;
            case 0x01:  // Program (ATEM 표준: 01)
            case 0x03:  // Both (ATEM 표준: 11)
                LOG_0(TAG, "LED: 빨간색 (PGM)");
                WS2812Core_setState(WS2812_PROGRAM);
                break;
            default:
                LOG_0(TAG, "LED: OFF");
                WS2812Core_setState(WS2812_OFF);
                break;
        }
    }
}
#endif

void TallyDispatcher::decodeTally(uint64_t packed, uint8_t* pgm, uint8_t* pgm_count,
                                 uint8_t* pvw, uint8_t* pvw_count, uint8_t max_channels)
{
    uint8_t pi = 0, vi = 0;

    if (max_channels > LORA_MAX_CHANNELS) {
        max_channels = LORA_MAX_CHANNELS;
    }

    for (uint8_t i = 0; i < max_channels; i++) {
        uint8_t tally = (packed >> (i * 2)) & 0x03;

        if (tally == 0x01 || tally == 0x03) {  // Program or Both (ATEM 표준)
            if (pgm && pi < 32) {
                pgm[pi] = i + 1;
                pi++;
            }
        }

        if (tally == 0x02 || tally == 0x03) {  // Preview or Both (ATEM 표준)
            if (pvw && vi < 32) {
                pvw[vi] = i + 1;
                vi++;
            }
        }
    }

    if (pgm_count) *pgm_count = pi;
    if (pvw_count) *pvw_count = vi;
}

void TallyDispatcher::formatTallyString(uint64_t packed, char* buffer, size_t buffer_size,
                                       uint8_t max_channels)
{
    if (!buffer || buffer_size == 0) return;

    uint8_t pgm[32], pvw[32];
    uint8_t pgm_count = 0, pvw_count = 0;

    decodeTally(packed, pgm, &pgm_count, pvw, &pvw_count, max_channels);

    char pgm_str[64] = "--";
    char pvw_str[64] = "--";

    if (pgm_count > 0) {
        char* p = pgm_str;
        for (uint8_t i = 0; i < pgm_count && i < 16; i++) {
            if (i > 0) *p++ = ',';
            p += snprintf(p, pgm_str + sizeof(pgm_str) - p, "%d", pgm[i]);
        }
    }

    if (pvw_count > 0) {
        char* p = pvw_str;
        for (uint8_t i = 0; i < pvw_count && i < 16; i++) {
            if (i > 0) *p++ = ',';
            p += snprintf(p, pvw_str + sizeof(pvw_str) - p, "%d", pvw[i]);
        }
    }

    snprintf(buffer, buffer_size, "PGM: %s / PVW: %s", pgm_str, pvw_str);
}

bool TallyDispatcher::isInitialized()
{
    return s_initialized;
}

void TallyDispatcher::forceUpdate()
{
    if (!s_initialized) return;

#ifdef DEVICE_MODE_TX
    // 현재 Tally 강제 전송
    uint64_t tally_primary = SwitcherManager::getTallyPacked(SWITCHER_INDEX_PRIMARY);
    uint64_t tally_secondary = SwitcherManager::getTallyPacked(SWITCHER_INDEX_SECONDARY);

    uint64_t combined;
    uint8_t channel_count;

    if (FastTallyMapper::isInitialized()) {
        uint64_t switcher_tally[LORA_MAX_SWITCHERS] = { tally_primary, tally_secondary };
        combined = FastTallyMapper::mapTally(switcher_tally, 2);

        // FastTallyMapper에서 실제 사용하는 최대 채널 수 사용
        channel_count = FastTallyMapper::getMaxChannel();
    } else {
        combined = tally_primary;
        switcher_t* sw_primary = SwitcherManager::getHandle(SWITCHER_INDEX_PRIMARY);
        channel_count = sw_primary ? switcher_get_effective_camera_count(sw_primary) : 0;
        if (channel_count == 0) channel_count = 16;
    }

    // 새로운 패킷 구조 생성 (F1-F4 헤더)
    uint8_t packet[LORA_TALLY_PACKET_MAX_SIZE];
    size_t packet_size;

    if (FastTallyMapper::isInitialized()) {
        // FastTallyMapper에서 패킷 타입 결정
        uint8_t header = FastTallyMapper::getPacketHeader();
        uint8_t data_length = FastTallyMapper::getDataLength();

        LOG_0(TAG, "패킷 타입: 0x%02X (%d채널, 데이터 %d바이트)",
              header, FastTallyMapper::getMaxChannel(), data_length);

        // 패킷 생성: [Header][Data...]
        packet[0] = header;
        for (uint8_t i = 0; i < data_length && i < 5; i++) {
            packet[1 + i] = (combined >> (i * 8)) & 0xFF;
        }
        packet_size = 1 + data_length;
    } else {
        // 기존 방식 (FastTallyMapper 미초기화)
        packet_size = LoRaPacket::createTallyPacket(combined, channel_count, packet, sizeof(packet));
    }

    if (packet_size > 0) {
        LoRaManager::transmit(packet, packet_size);
        LOG_0(TAG, "강제 Tally 업데이트 전송");
    }

    // 마지막 상태 업데이트
    s_last_tally_primary = tally_primary;
    s_last_tally_secondary = tally_secondary;
#endif
}

#ifdef DEVICE_MODE_TX
void TallyDispatcher::getCurrentTally(uint8_t* pgm, uint8_t* pgm_count,
                                     uint8_t* pvw, uint8_t* pvw_count)
{
    if (!s_initialized || !pgm || !pvw) {
        if (pgm_count) *pgm_count = 0;
        if (pvw_count) *pvw_count = 0;
        return;
    }

    uint64_t tally_primary = SwitcherManager::getTallyPacked(SWITCHER_INDEX_PRIMARY);
    uint64_t tally_secondary = SwitcherManager::getTallyPacked(SWITCHER_INDEX_SECONDARY);

    uint64_t combined;
    uint8_t channel_count;

    if (FastTallyMapper::isInitialized()) {
        uint64_t switcher_tally[LORA_MAX_SWITCHERS] = { tally_primary, tally_secondary };
        combined = FastTallyMapper::mapTally(switcher_tally, 2);

        // FastTallyMapper에서 실제 사용하는 최대 채널 수 사용
        channel_count = FastTallyMapper::getMaxChannel();
    } else {
        combined = tally_primary;
        switcher_t* sw_primary = SwitcherManager::getHandle(SWITCHER_INDEX_PRIMARY);
        channel_count = sw_primary ? switcher_get_effective_camera_count(sw_primary) : 0;
        if (channel_count == 0) channel_count = 16;
    }

    decodeTally(combined, pgm, pgm_count, pvw, pvw_count, channel_count);
}

void TallyDispatcher::logMappingInfo()
{
    // FastTallyMapper 초기화 (아직 초기화되지 않았다면)
    LOG_1(TAG, "FastTallyMapper 초기화 상태 확인: %s", FastTallyMapper::isInitialized() ? "초기화됨" : "초기화 안됨");

    if (!FastTallyMapper::isInitialized()) {
        LOG_1(TAG, "FastTallyMapper 초기화 시작");
        // Switcher 정보로 맵핑 테이블 생성
        MappingTable table = {};
        uint8_t active_count = 0;

        // Primary 스위처 설정
        switcher_t* sw_primary = SwitcherManager::getHandle(SWITCHER_INDEX_PRIMARY);
        if (sw_primary && SwitcherManager::isConnected(SWITCHER_INDEX_PRIMARY)) {
            table.offsets[0] = switcher_get_camera_offset(sw_primary);
            table.limits[0] = switcher_get_effective_camera_count(sw_primary);
            table.channel_to_switcher[0] = 0;  // Primary 스위처
            active_count++;
            LOG_0(TAG, "Primary 스위처: 연결됨 (offset=%d, cameras=%d)", table.offsets[0], table.limits[0]);
        } else {
            // 미연결 상태에서는 저장된 값 사용 (기본값은 0)
            table.offsets[0] = switcher_get_camera_offset(sw_primary);
            table.limits[0] = switcher_get_camera_limit(sw_primary);  // 저장된 limit 값
            table.channel_to_switcher[0] = 0;
            LOG_0(TAG, "Primary 스위처: 미연결 (저장된 값: offset=%d, limit=%d)", table.offsets[0], table.limits[0]);
        }

        // Secondary 스위처 설정
        switcher_t* sw_secondary = SwitcherManager::getHandle(SWITCHER_INDEX_SECONDARY);
        if (sw_secondary && SwitcherManager::isConnected(SWITCHER_INDEX_SECONDARY)) {
            table.offsets[1] = switcher_get_camera_offset(sw_secondary);
            table.limits[1] = switcher_get_effective_camera_count(sw_secondary);
            table.channel_to_switcher[1] = 1;  // Secondary 스위처
            active_count++;
            LOG_0(TAG, "Secondary 스위처: 연결됨 (offset=%d, cameras=%d)", table.offsets[1], table.limits[1]);
        } else {
            // 미연결 상태에서는 저장된 값 사용 (기본값은 0)
            table.offsets[1] = switcher_get_camera_offset(sw_secondary);
            table.limits[1] = switcher_get_camera_limit(sw_secondary);  // 저장된 limit 값
            table.channel_to_switcher[1] = 1;
            LOG_0(TAG, "Secondary 스위처: 미연결 (저장된 값: offset=%d, limit=%d)", table.offsets[1], table.limits[1]);
        }

        table.active_switchers = active_count;

        // FastTallyMapper 초기화
        esp_err_t err = FastTallyMapper::init(table);
        if (err == ESP_OK) {
            LOG_0(TAG, "FastTallyMapper 초기화됨 (%d개 스위처 활성)", active_count);
        } else {
            LOG_0(TAG, "FastTallyMapper 초기화 실패: %d", err);
        }
    }

    // FastTallyMapper::logMappingInfo();  // 이미 initializeMapper()에서 출력하므로 주석 처리
}

void TallyDispatcher::reinitializeMapper()
{
    LOG_0(TAG, "FastTallyMapper 재초기화 시작");

    // Switcher 정보로 맵핑 테이블 생성
    MappingTable table = {};
    uint8_t active_count = 0;

    // Primary 스위처
    switcher_t* sw_primary = SwitcherManager::getHandle(SWITCHER_INDEX_PRIMARY);
    if (sw_primary && SwitcherManager::isConnected(SWITCHER_INDEX_PRIMARY)) {
        table.offsets[0] = switcher_get_camera_offset(sw_primary);
        // limits는 사용자 설정 값 유지 (0=auto), effective_count는 실제 값
        table.limits[0] = switcher_get_camera_limit(sw_primary);
        table.channel_to_switcher[0] = 0;  // Primary 스위처
        active_count++;
        uint8_t effective = switcher_get_effective_camera_count(sw_primary);
        LOG_0(TAG, "Primary: 연결됨 (offset=%d, limit=%d, effective=%d)", table.offsets[0], table.limits[0], effective);
    } else {
        // 미연결 상태에서도 저장된 값 사용
        table.offsets[0] = switcher_get_camera_offset(sw_primary);
        table.limits[0] = switcher_get_camera_limit(sw_primary);
        table.channel_to_switcher[0] = 0;  // Primary 스위처
        LOG_0(TAG, "Primary: 미연결 (저장된 값: offset=%d, limit=%d)", table.offsets[0], table.limits[0]);
    }

    // Secondary 스위처
    switcher_t* sw_secondary = SwitcherManager::getHandle(SWITCHER_INDEX_SECONDARY);
    if (sw_secondary && SwitcherManager::isConnected(SWITCHER_INDEX_SECONDARY)) {
        table.offsets[1] = switcher_get_camera_offset(sw_secondary);
        // limits는 사용자 설정 값 유지 (0=auto), effective_count는 실제 값
        table.limits[1] = switcher_get_camera_limit(sw_secondary);
        table.channel_to_switcher[1] = 1;  // Secondary 스위처
        active_count++;
        uint8_t effective = switcher_get_effective_camera_count(sw_secondary);
        LOG_0(TAG, "Secondary: 연결됨 (offset=%d, limit=%d, effective=%d)", table.offsets[1], table.limits[1], effective);
    } else {
        // 미연결 상태에서도 저장된 값 사용
        table.offsets[1] = switcher_get_camera_offset(sw_secondary);
        table.limits[1] = switcher_get_camera_limit(sw_secondary);
        table.channel_to_switcher[1] = 1;  // Secondary 스위처
        LOG_0(TAG, "Secondary: 미연결 (저장된 값: offset=%d, limit=%d)", table.offsets[1], table.limits[1]);
    }

    table.active_switchers = active_count;

    // FastTallyMapper 강제 재초기화
    esp_err_t err = FastTallyMapper::reinit(table);
    if (err == ESP_OK) {
        LOG_0(TAG, "FastTallyMapper 재초기화 성공 (%d개 스위처 활성)", active_count);
    } else {
        LOG_0(TAG, "FastTallyMapper 재초기화 실패: %d", err);
    }

    FastTallyMapper::logMappingInfo();
}
#endif

// C 래퍼 함수 구현
extern "C" {
    bool TallyDispatcher_isInitialized(void) {
        return TallyDispatcher::isInitialized();
    }

    esp_err_t TallyDispatcher_init(void) {
        return TallyDispatcher::init();
    }

    void TallyDispatcher_processTallyChanges(void) {
        TallyDispatcher::processTallyChanges();
    }

    void TallyDispatcher_onLoRaReceived(const uint8_t* data, size_t length) {
        TallyDispatcher::onLoRaReceived(data, length);
    }

    void TallyDispatcher_forceUpdate(void) {
        TallyDispatcher::forceUpdate();
    }
}