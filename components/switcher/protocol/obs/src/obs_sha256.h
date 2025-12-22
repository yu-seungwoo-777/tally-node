/**
 * @file obs_sha256.h
 * @brief SHA256 구현 (순수 C)
 *
 * RFC 6234 기반 SHA256 해시 함수
 */

#ifndef OBS_SHA256_H
#define OBS_SHA256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA256_BLOCK_SIZE  64
#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[SHA256_BLOCK_SIZE];
} sha256_ctx_t;

/**
 * @brief SHA256 컨텍스트 초기화
 */
void sha256_init(sha256_ctx_t* ctx);

/**
 * @brief 데이터 추가
 */
void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief 해시 완료 및 결과 출력
 */
void sha256_final(sha256_ctx_t* ctx, uint8_t digest[SHA256_DIGEST_SIZE]);

/**
 * @brief 단일 호출로 SHA256 계산
 */
void sha256(const uint8_t* data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* OBS_SHA256_H */
