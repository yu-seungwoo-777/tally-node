/**
 * @file result.hpp
 * @brief Result<T> 패턴 구현
 *
 * C++에서 예외 없이 에러를 처리하기 위한 Result 타입
 */

#pragma once

#include "esp_err.h"
#include <type_traits>
#include <optional>

namespace info {

/**
 * @brief 값이 없는 결과를 위한 더미 타입
 */
struct Void {};

/**
 * @brief Result<T> 패턴 구현
 *
 * 성공 또는 실패를 나타내는 타입 안전적인 결과 컨테이너
 * 예외를 사용하지 않고 에러 처리를 할 수 있음
 */
template<typename T = Void>
class Result {
public:
    /**
     * @brief 성공 결과 생성
     * @param value 성공 값
     * @return Result 인스턴스
     */
    static Result ok(T value) {
        Result r;
        r.value_ = std::move(value);
        r.err_ = ESP_OK;
        return r;
    }

    /**
     * @brief 성공 결과 생성 (Void 타입용)
     * @return Result 인스턴스
     */
    static Result ok() {
        static_assert(std::is_same_v<T, Void>, "Use ok(value) for non-void types");
        Result r;
        r.err_ = ESP_OK;
        return r;
    }

    /**
     * @brief 실패 결과 생성
     * @param err 에러 코드
     * @return Result 인스턴스
     */
    static Result fail(esp_err_t err) {
        Result r;
        r.err_ = err;
        return r;
    }

    // 기본 생성자 (실패 상태로 초기화)
    Result() = default;

    // 복사 생성자
    Result(const Result& other) = default;

    // 이동 생성자
    Result(Result&& other) noexcept = default;

    // 복사 대입 연산자
    Result& operator=(const Result& other) = default;

    // 이동 대입 연산자
    Result& operator=(Result&& other) noexcept = default;

    /**
     * @brief 성공 상태 확인
     * @return 성공이면 true
     */
    bool is_ok() const { return err_ == ESP_OK; }

    /**
     * @brief 실패 상태 확인
     * @return 실패이면 true
     */
    bool is_err() const { return err_ != ESP_OK; }

    /**
     * @brief bool 변환 (성공 상태)
     */
    explicit operator bool() const { return is_ok(); }

    /**
     * @brief 값 접근 (성공 시에만 유효)
     * @return 값에 대한 참조
     * @note 실패 시에는 미정의 동작
     */
    const T& value() const & {
        return value_.value();
    }

    T& value() & {
        return value_.value();
    }

    const T&& value() const && {
        return std::move(value_.value());
    }

    T&& value() && {
        return std::move(value_.value());
    }

    /**
     * @brief 에러 코드 접근
     * @return ESP 에러 코드
     */
    esp_err_t error() const { return err_; }

    /**
     * @brief 성공 시 값 반환, 실패 시 기본값 반환
     * @param default_val 실패 시 반환할 기본값
     * @return 성공 값 또는 기본값
     */
    T value_or(const T& default_val) const & {
        return is_ok() ? value_.value() : default_val;
    }

    T value_or(T&& default_val) const && {
        return is_ok() ? std::move(value_.value()) : std::move(default_val);
    }

    /**
     * @brief 에러 메시지 가져오기
     * @return 에러 메시지 문자열
     */
    const char* error_str() const {
        return esp_err_to_name(err_);
    }

private:
    std::optional<T> value_;
    esp_err_t err_ = ESP_FAIL;
};

// Void 특수화를 위한 타입 별칭
using VoidResult = Result<Void>;

// 편의 함수들
template<typename T>
inline Result<T> Ok(T value) {
    return Result<T>::ok(std::move(value));
}

inline VoidResult Ok() {
    return VoidResult::ok();
}

template<typename T>
inline Result<T> Err(esp_err_t err) {
    return Result<T>::fail(err);
}

} // namespace info