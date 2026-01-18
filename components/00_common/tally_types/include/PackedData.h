/**
 * @file PackedData.h
 * @brief RAII Wrapper for packed_data_t
 *
 * 스코프를 벗어나면 자동으로 메모리 정리
 * 이동 의미론(move semantics) 지원
 */

#ifndef PACKED_DATA_H
#define PACKED_DATA_H

#include "TallyTypes.h"
#include <stdint.h>
#include <utility>

#ifdef __cplusplus

/**
 * @brief RAII 래퍼 - PackedData 자동 메모리 관리
 *
 * 스코프를 벗어나면 자동으로 메모리 정리
 * 이동 의미론(move semantics) 지원
 */
class PackedData {
public:
    // ========================================================================
    // 생성자 / 소멸자
    // ========================================================================

    /**
     * @brief 생성자
     * @param channel_count 채널 수 (기본값: TALLY_MAX_CHANNELS)
     *
     * @note data_를 명시적으로 0으로 초기화하여 packed_data_init에서
     *       쓰레기값을 free()하는 문제 방지 (힙 손상 bugfix)
     */
    explicit PackedData(uint8_t channel_count = TALLY_MAX_CHANNELS) : data_{} {
        packed_data_init(&data_, channel_count);
    }

    /**
     * @brief 소멸자 - 자동 메모리 정리
     */
    ~PackedData() {
        packed_data_cleanup(&data_);
    }

    // ========================================================================
    // 복사 / 이동
    // ========================================================================

    // 복사 금지 (명확한 소유권 전달)
    PackedData(const PackedData&) = delete;
    PackedData& operator=(const PackedData&) = delete;

    /**
     * @brief 이동 생성자
     */
    PackedData(PackedData&& other) noexcept
        : data_(other.data_) {
        other.data_.data = nullptr;
        other.data_.data_size = 0;
        other.data_.channel_count = 0;
    }

    /**
     * @brief 이동 대입 연산자
     */
    PackedData& operator=(PackedData&& other) noexcept {
        if (this != &other) {
            packed_data_cleanup(&data_);
            data_ = other.data_;
            other.data_.data = nullptr;
            other.data_.data_size = 0;
            other.data_.channel_count = 0;
        }
        return *this;
    }

    // ========================================================================
    // 접근자 (Accessors)
    // ========================================================================

    /**
     * @brief 내부 데이터 포인터 반환 (const)
     */
    const packed_data_t* get() const { return &data_; }

    /**
     * @brief 내부 데이터 포인터 반환
     */
    packed_data_t* get() { return &data_; }

    /**
     * @brief 화살표 연산자 오버로딩
     */
    const packed_data_t* operator->() const { return &data_; }
    packed_data_t* operator->() { return &data_; }

    /**
     * @brief 역참조 연산자 오버로딩
     */
    const packed_data_t& operator*() const { return data_; }
    packed_data_t& operator*() { return data_; }

    // ========================================================================
    // 유틸리티 메서드
    // ========================================================================

    /**
     * @brief 채널 상태 설정
     * @param channel 채널 번호 (1-based)
     * @param flags 플래그 (0=OFF, 1=PGM, 2=PVW, 3=BOTH)
     */
    void setChannel(uint8_t channel, uint8_t flags) {
        packed_data_set_channel(&data_, channel, flags);
    }

    /**
     * @brief 채널 상태 조회
     * @param channel 채널 번호 (1-based)
     * @return 플래그 (0=OFF, 1=PGM, 2=PVW, 3=BOTH)
     */
    uint8_t getChannel(uint8_t channel) const {
        return packed_data_get_channel(&data_, channel);
    }

    /**
     * @brief 다른 PackedData와 비교
     * @param other 비교 대상
     * @return 동일하면 true
     */
    bool equals(const PackedData& other) const {
        return packed_data_equals(&data_, &other.data_);
    }

    /**
     * @brief 유효성 확인
     * @return 유효하면 true
     */
    bool isValid() const {
        return packed_data_is_valid(&data_);
    }

    /**
     * @brief 64비트 정수로 변환
     * @return 64비트 packed 값
     */
    uint64_t toUint64() const {
        return packed_data_to_uint64(&data_);
    }

    /**
     * @brief 16진수 문자열로 변환
     * @param buf 출력 버퍼
     * @param buf_size 버퍼 크기
     * @return 버퍼 포인터
     */
    char* toHex(char* buf, size_t buf_size) const {
        return packed_data_to_hex(&data_, buf, buf_size);
    }

    /**
     * @brief Tally 문자열로 포맷
     * @param buf 출력 버퍼
     * @param buf_size 버퍼 크기
     * @return 버퍼 포인터
     */
    char* formatTally(char* buf, size_t buf_size) const {
        return packed_data_format_tally(&data_, buf, buf_size);
    }

    /**
     * @brief 채널 수 조회
     */
    uint8_t channelCount() const { return data_.channel_count; }

    /**
     * @brief 채널 수 변경 (항상 재초기화)
     * @param new_count 새 채널 수
     *
     * @note 항상 재초기화하여 이전 데이터를 제거 (버그 방지)
     */
    void resize(uint8_t new_count) {
        packed_data_cleanup(&data_);
        packed_data_init(&data_, new_count);
    }

    /**
     * @brief 다른 PackedData에서 복사
     * @param other 복사 원본
     */
    void copyFrom(const PackedData& other) {
        packed_data_copy(&data_, &other.data_);
    }

    /**
     * @brief 64비트 정수에서 데이터 로드
     * @param value 64비트 packed 값
     * @param channel_count 채널 수
     */
    void fromUint64(uint64_t value, uint8_t channel_count) {
        packed_data_from_uint64(&data_, value, channel_count);
    }

private:
    packed_data_t data_;
};

// ============================================================================
// 비교 연산자
// ============================================================================

inline bool operator==(const PackedData& a, const PackedData& b) {
    return a.equals(b);
}

inline bool operator!=(const PackedData& a, const PackedData& b) {
    return !a.equals(b);
}

#endif // __cplusplus

#endif // PACKED_DATA_H
