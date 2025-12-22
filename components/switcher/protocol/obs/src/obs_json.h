/**
 * @file obs_json.h
 * @brief 간단한 JSON 파서 (순수 C)
 *
 * OBS WebSocket 메시지 파싱에 필요한 최소한의 JSON 기능
 */

#ifndef OBS_JSON_H
#define OBS_JSON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* JSON 값 타입 */
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

/* JSON 값 (간단한 구조) */
typedef struct json_value {
    json_type_t type;
    union {
        bool bool_val;
        double num_val;
        struct {
            char* str;
            size_t len;
        } string;
        struct {
            struct json_value* items;
            size_t count;
        } array;
        struct {
            char** keys;
            struct json_value* values;
            size_t count;
        } object;
    } data;
} json_value_t;

/**
 * @brief JSON 문자열 파싱
 * @param json JSON 문자열
 * @param len 문자열 길이
 * @return 파싱된 JSON 값 (NULL: 실패)
 */
json_value_t* json_parse(const char* json, size_t len);

/**
 * @brief JSON 값 해제
 */
void json_free(json_value_t* value);

/**
 * @brief 객체에서 키로 값 찾기
 * @param obj JSON 객체
 * @param key 키 이름
 * @return 값 (NULL: 없음)
 */
json_value_t* json_object_get(const json_value_t* obj, const char* key);

/**
 * @brief 배열에서 인덱스로 값 가져오기
 */
json_value_t* json_array_get(const json_value_t* arr, size_t index);

/**
 * @brief 정수 값 가져오기
 */
int json_get_int(const json_value_t* val, int default_val);

/**
 * @brief 문자열 값 가져오기
 */
const char* json_get_string(const json_value_t* val);

/**
 * @brief bool 값 가져오기
 */
bool json_get_bool(const json_value_t* val, bool default_val);

/**
 * @brief 배열 크기 가져오기
 */
size_t json_array_size(const json_value_t* arr);

/*===========================================================================
 * JSON 생성 (간단한 빌더)
 *===========================================================================*/

#define JSON_BUILD_MAX 2048

typedef struct {
    char buffer[JSON_BUILD_MAX];
    size_t pos;
    int depth;
    bool need_comma;
} json_builder_t;

/**
 * @brief JSON 빌더 초기화
 */
void json_builder_init(json_builder_t* b);

/**
 * @brief 객체 시작
 */
void json_builder_object_start(json_builder_t* b);

/**
 * @brief 객체 종료
 */
void json_builder_object_end(json_builder_t* b);

/**
 * @brief 배열 시작
 */
void json_builder_array_start(json_builder_t* b);

/**
 * @brief 배열 종료
 */
void json_builder_array_end(json_builder_t* b);

/**
 * @brief 키 추가 (객체 내에서)
 */
void json_builder_key(json_builder_t* b, const char* key);

/**
 * @brief 문자열 값 추가
 */
void json_builder_string(json_builder_t* b, const char* value);

/**
 * @brief 정수 값 추가
 */
void json_builder_int(json_builder_t* b, int value);

/**
 * @brief bool 값 추가
 */
void json_builder_bool(json_builder_t* b, bool value);

/**
 * @brief null 값 추가
 */
void json_builder_null(json_builder_t* b);

/**
 * @brief 결과 문자열 가져오기
 */
const char* json_builder_get(json_builder_t* b);

/**
 * @brief 결과 문자열 길이
 */
size_t json_builder_len(json_builder_t* b);

#ifdef __cplusplus
}
#endif

#endif /* OBS_JSON_H */
