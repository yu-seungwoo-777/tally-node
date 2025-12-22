/**
 * @file FastTallyMapper.h
 * @brief 고성능 Tally 매퍼 (실시간 처리)
 *
 * O(1) 맵핑 테이블 기반의 초고속 Tally 매퍼
 * - 60Hz 실시간 처리 지원
 * - 인라인 최적화
 * - 루프 언롤링
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "LoRaTypes.h"
#include "esp_err.h"

#ifdef __cplusplus

/**
 * @brief 고성능 Tally 매퍼
 *
 * 설계 원칙:
 * - O(1) 시간 복잡도
 * - 캐시 친화적 메모리 레이아웃
 * - 컴파일 타임 최적화
 */
class FastTallyMapper {
public:
    /**
     * @brief 초기화
     *
     * @param config 맵핑 설정
     * @return ESP_OK 성공, ESP_FAIL 실패
     */
    static esp_err_t init(const MappingTable& config);

    /**
     * @brief 스위처 설정 업데이트
     *
     * @param switcher_id 스위처 ID (0-3)
     * @param config 스위처 설정
     */
    static void updateSwitcherConfig(uint8_t switcher_id, const SwitcherConfig& config);

    /**
     * @brief Tally 데이터 맵핑 (고속)
     *
     * @param switcher_tally 스위처별 Tally 데이터 배열
     * @param count 활성 스위처 수
     * @return 맵핑된 Combined Tally
     */
    static inline uint64_t mapTally(const uint64_t* switcher_tally, uint8_t count) {
        uint64_t result = 0;

        // 루프 언롤링 최적화 (컴파일러 지시)
        #pragma GCC unroll 4
        for (uint8_t i = 0; i < LORA_MAX_SWITCHERS; ++i) {
            if (i < count && s_switcher_configs_[i].enabled) {
                // 오프셋이 너무 크면 스킵 (오버플로우 방지)
                if (s_switcher_configs_[i].offset >= 32) {
                    continue;  // 32*2 = 64비트를 초과하므로 스킵
                }

                uint8_t shift_bits = s_switcher_configs_[i].offset * 2;
                uint8_t channel_bits = s_switcher_configs_[i].camera_count * 2;

                // 64비트 오버플로우 체크
                if (shift_bits + channel_bits > 64) {
                    channel_bits = 64 - shift_bits;
                }

                if (channel_bits == 0) continue;

                // 시프트 연산으로 오프셋 적용
                uint64_t shifted = switcher_tally[i] << shift_bits;

                // 마스크로 채널 수 제한
                uint64_t mask = ((1ULL << channel_bits) - 1) << shift_bits;

                // OR 연산으로 조합
                uint64_t contribution = shifted & mask;
                result |= contribution;
            }
        }

        return result;
    }

    /**
     * @brief 채널 수 계산
     *
     * @return 총 채널 수
     */
    static inline uint8_t getTotalChannels() {
        uint8_t max_channel = 0;

        for (uint8_t i = 0; i < LORA_MAX_SWITCHERS; ++i) {
            if (s_switcher_configs_[i].enabled) {
                // 오버플로우 체크
                if (s_switcher_configs_[i].offset > LORA_MAX_CHANNELS) {
                    // 오프셋이 최대 채널을 초과하면 이 스위처는 무시
                    continue;
                }

                uint8_t end_channel = s_switcher_configs_[i].offset +
                                    s_switcher_configs_[i].camera_count;

                // 오버플로우 방지
                if (end_channel > LORA_MAX_CHANNELS) {
                    end_channel = LORA_MAX_CHANNELS;
                }

                if (end_channel > max_channel) {
                    max_channel = end_channel;
                }
            }
        }

        return max_channel;
    }

    /**
     * @brief 최대 채널 번호 반환 (패킷 타입 결정용)
     *
     * @return 최대 채널 번호 (1-20)
     */
    static inline uint8_t getMaxChannel() {
        return getTotalChannels();
    }

    /**
     * @brief 패킷 헤더 타입 결정
     *
     * @return 패킷 헤더 (0xF1, 0xF2, 0xF3, 0xF4)
     */
    static inline uint8_t getPacketHeader() {
        uint8_t max_channel = getMaxChannel();

        if (max_channel <= 8) return 0xF1;
        if (max_channel <= 12) return 0xF2;
        if (max_channel <= 16) return 0xF3;
        return 0xF4;  // 17-20 채널
    }

    /**
     * @brief 패킷 데이터 길이 반환
     *
     * @return 데이터 바이트 수 (2-5)
     */
    static inline uint8_t getDataLength() {
        uint8_t max_channel = getMaxChannel();
        return (max_channel + 3) / 4;  // 4채널당 1바이트
    }

    /**
     * @brief 맵핑 정보 로그 출력
     */
    static void logMappingInfo();

    /**
     * @brief 현재 맵핑 설정 가져오기
     *
     * @return 맵핑 테이블
     */
    static MappingTable getCurrentMapping();

    /**
     * @brief 초기화 여부 확인
     *
     * @return true: 초기화됨, false: 초기화 안 됨
     */
    static bool isInitialized();

    /**
     * @brief 강제 재초기화
     *
     * @param config 맵핑 설정
     * @return ESP_OK 성공, ESP_FAIL 실패
     */
    static esp_err_t reinit(const MappingTable& config);

private:
    // 싱글톤 패턴
    FastTallyMapper() = delete;
    ~FastTallyMapper() = delete;
    FastTallyMapper(const FastTallyMapper&) = delete;
    FastTallyMapper& operator=(const FastTallyMapper&) = delete;

    // 맵핑 설정 (캐시 친화적 배열)
    static SwitcherConfig s_switcher_configs_[LORA_MAX_SWITCHERS];
    static bool s_initialized;
    static uint8_t s_active_switchers;

    // 최적화 유틸리티
    static inline uint64_t applyOffset(uint64_t data, uint8_t offset) {
        return offset > 0 ? (data << (offset * 2)) : data;
    }

    static inline uint64_t applyMask(uint64_t data, uint8_t channels) {
        return channels > 0 ? (data & ((1ULL << (channels * 2)) - 1)) : 0;
    }
};

// C 래퍼 함수
extern "C" {
    esp_err_t FastTallyMapper_init(const MappingTable* config);
    uint64_t FastTallyMapper_mapTally(const uint64_t* switcher_tally, uint8_t count);
    uint8_t FastTallyMapper_getTotalChannels(void);
    void FastTallyMapper_logMappingInfo(void);
}

#endif // __cplusplus