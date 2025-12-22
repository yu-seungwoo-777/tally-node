/**
 * @file obs_base64.h
 * @brief Base64 인코딩/디코딩 (순수 C)
 */

#ifndef OBS_BASE64_H
#define OBS_BASE64_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Base64 인코딩된 문자열 길이 계산
 * @param input_len 입력 데이터 길이
 * @return 인코딩된 문자열 길이 (널 문자 제외)
 */
size_t base64_encode_len(size_t input_len);

/**
 * @brief Base64 인코딩
 * @param data 입력 데이터
 * @param len 입력 길이
 * @param output 출력 버퍼 (base64_encode_len(len) + 1 크기 필요)
 * @return 인코딩된 문자열 길이
 */
size_t base64_encode(const uint8_t* data, size_t len, char* output);

/**
 * @brief Base64 디코딩된 데이터 길이 계산
 * @param input Base64 문자열
 * @param input_len 문자열 길이
 * @return 디코딩된 데이터 길이
 */
size_t base64_decode_len(const char* input, size_t input_len);

/**
 * @brief Base64 디코딩
 * @param input Base64 문자열
 * @param input_len 문자열 길이
 * @param output 출력 버퍼
 * @return 디코딩된 데이터 길이
 */
size_t base64_decode(const char* input, size_t input_len, uint8_t* output);

#ifdef __cplusplus
}
#endif

#endif /* OBS_BASE64_H */
