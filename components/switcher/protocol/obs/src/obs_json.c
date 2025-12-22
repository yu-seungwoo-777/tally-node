/**
 * @file obs_json.c
 * @brief 간단한 JSON 파서 (순수 C)
 */

#include "obs_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * 파서 내부 함수
 *===========================================================================*/

typedef struct {
    const char* json;
    size_t len;
    size_t pos;
} json_parser_t;

static void skip_whitespace(json_parser_t* p)
{
    while (p->pos < p->len && isspace((unsigned char)p->json[p->pos])) {
        p->pos++;
    }
}

static char peek(json_parser_t* p)
{
    skip_whitespace(p);
    if (p->pos >= p->len) return '\0';
    return p->json[p->pos];
}

static char next(json_parser_t* p)
{
    skip_whitespace(p);
    if (p->pos >= p->len) return '\0';
    return p->json[p->pos++];
}

static bool match(json_parser_t* p, const char* str)
{
    size_t len = strlen(str);
    if (p->pos + len > p->len) return false;
    if (strncmp(&p->json[p->pos], str, len) == 0) {
        p->pos += len;
        return true;
    }
    return false;
}

static json_value_t* parse_value(json_parser_t* p);

static json_value_t* parse_string(json_parser_t* p)
{
    if (next(p) != '"') return NULL;

    size_t start = p->pos;
    size_t len = 0;

    /* 문자열 길이 계산 (이스케이프 처리) */
    while (p->pos < p->len && p->json[p->pos] != '"') {
        if (p->json[p->pos] == '\\' && p->pos + 1 < p->len) {
            p->pos += 2;
            len++;
        } else {
            p->pos++;
            len++;
        }
    }

    if (p->pos >= p->len) return NULL;
    p->pos++;  /* 닫는 따옴표 */

    json_value_t* val = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (!val) return NULL;

    val->type = JSON_STRING;
    val->data.string.str = (char*)malloc(len + 1);
    if (!val->data.string.str) {
        free(val);
        return NULL;
    }

    /* 문자열 복사 (이스케이프 처리) */
    size_t j = 0;
    for (size_t i = start; i < p->pos - 1 && j < len;) {
        if (p->json[i] == '\\' && i + 1 < p->pos - 1) {
            i++;
            switch (p->json[i]) {
                case 'n': val->data.string.str[j++] = '\n'; break;
                case 'r': val->data.string.str[j++] = '\r'; break;
                case 't': val->data.string.str[j++] = '\t'; break;
                case '\\': val->data.string.str[j++] = '\\'; break;
                case '"': val->data.string.str[j++] = '"'; break;
                case '/': val->data.string.str[j++] = '/'; break;
                default: val->data.string.str[j++] = p->json[i]; break;
            }
            i++;
        } else {
            val->data.string.str[j++] = p->json[i++];
        }
    }
    val->data.string.str[j] = '\0';
    val->data.string.len = j;

    return val;
}

static json_value_t* parse_number(json_parser_t* p)
{
    skip_whitespace(p);
    size_t start = p->pos;

    /* 숫자 파싱 */
    if (p->json[p->pos] == '-') p->pos++;

    while (p->pos < p->len && isdigit((unsigned char)p->json[p->pos])) {
        p->pos++;
    }

    if (p->pos < p->len && p->json[p->pos] == '.') {
        p->pos++;
        while (p->pos < p->len && isdigit((unsigned char)p->json[p->pos])) {
            p->pos++;
        }
    }

    if (p->pos < p->len && (p->json[p->pos] == 'e' || p->json[p->pos] == 'E')) {
        p->pos++;
        if (p->pos < p->len && (p->json[p->pos] == '+' || p->json[p->pos] == '-')) {
            p->pos++;
        }
        while (p->pos < p->len && isdigit((unsigned char)p->json[p->pos])) {
            p->pos++;
        }
    }

    char buf[64];
    size_t len = p->pos - start;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    strncpy(buf, &p->json[start], len);
    buf[len] = '\0';

    json_value_t* val = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (!val) return NULL;

    val->type = JSON_NUMBER;
    val->data.num_val = atof(buf);

    return val;
}

static json_value_t* parse_array(json_parser_t* p)
{
    if (next(p) != '[') return NULL;

    json_value_t* val = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (!val) return NULL;

    val->type = JSON_ARRAY;
    val->data.array.items = NULL;
    val->data.array.count = 0;

    /* 빈 배열 */
    if (peek(p) == ']') {
        p->pos++;
        return val;
    }

    /* 임시 배열 (최대 128개) */
    json_value_t* temp[128];
    size_t count = 0;

    while (count < 128) {
        json_value_t* item = parse_value(p);
        if (!item) {
            for (size_t i = 0; i < count; i++) json_free(temp[i]);
            free(val);
            return NULL;
        }
        temp[count++] = item;

        char c = peek(p);
        if (c == ']') {
            p->pos++;
            break;
        }
        if (c != ',') {
            for (size_t i = 0; i < count; i++) json_free(temp[i]);
            free(val);
            return NULL;
        }
        p->pos++;  /* 쉼표 건너뛰기 */
    }

    if (count > 0) {
        val->data.array.items = (json_value_t*)malloc(count * sizeof(json_value_t));
        if (val->data.array.items) {
            for (size_t i = 0; i < count; i++) {
                val->data.array.items[i] = *temp[i];
                free(temp[i]);
            }
            val->data.array.count = count;
        }
    }

    return val;
}

static json_value_t* parse_object(json_parser_t* p)
{
    if (next(p) != '{') return NULL;

    json_value_t* val = (json_value_t*)calloc(1, sizeof(json_value_t));
    if (!val) return NULL;

    val->type = JSON_OBJECT;
    val->data.object.keys = NULL;
    val->data.object.values = NULL;
    val->data.object.count = 0;

    /* 빈 객체 */
    if (peek(p) == '}') {
        p->pos++;
        return val;
    }

    /* 임시 저장 (최대 64개 키-값) */
    char* temp_keys[64];
    json_value_t* temp_values[64];
    size_t count = 0;

    while (count < 64) {
        /* 키 파싱 */
        json_value_t* key_val = parse_string(p);
        if (!key_val || key_val->type != JSON_STRING) {
            if (key_val) json_free(key_val);
            goto error;
        }

        char* key = key_val->data.string.str;
        key_val->data.string.str = NULL;
        json_free(key_val);

        /* 콜론 */
        if (next(p) != ':') {
            free(key);
            goto error;
        }

        /* 값 파싱 */
        json_value_t* value = parse_value(p);
        if (!value) {
            free(key);
            goto error;
        }

        temp_keys[count] = key;
        temp_values[count] = value;
        count++;

        char c = peek(p);
        if (c == '}') {
            p->pos++;
            break;
        }
        if (c != ',') goto error;
        p->pos++;  /* 쉼표 건너뛰기 */
    }

    if (count > 0) {
        val->data.object.keys = (char**)malloc(count * sizeof(char*));
        val->data.object.values = (json_value_t*)malloc(count * sizeof(json_value_t));

        if (val->data.object.keys && val->data.object.values) {
            for (size_t i = 0; i < count; i++) {
                val->data.object.keys[i] = temp_keys[i];
                val->data.object.values[i] = *temp_values[i];
                free(temp_values[i]);
            }
            val->data.object.count = count;
        }
    }

    return val;

error:
    for (size_t i = 0; i < count; i++) {
        free(temp_keys[i]);
        json_free(temp_values[i]);
    }
    free(val);
    return NULL;
}

static json_value_t* parse_value(json_parser_t* p)
{
    char c = peek(p);

    if (c == '"') {
        return parse_string(p);
    }
    if (c == '{') {
        return parse_object(p);
    }
    if (c == '[') {
        return parse_array(p);
    }
    if (c == '-' || isdigit((unsigned char)c)) {
        return parse_number(p);
    }
    if (match(p, "true")) {
        json_value_t* val = (json_value_t*)calloc(1, sizeof(json_value_t));
        if (val) {
            val->type = JSON_BOOL;
            val->data.bool_val = true;
        }
        return val;
    }
    if (match(p, "false")) {
        json_value_t* val = (json_value_t*)calloc(1, sizeof(json_value_t));
        if (val) {
            val->type = JSON_BOOL;
            val->data.bool_val = false;
        }
        return val;
    }
    if (match(p, "null")) {
        json_value_t* val = (json_value_t*)calloc(1, sizeof(json_value_t));
        if (val) {
            val->type = JSON_NULL;
        }
        return val;
    }

    return NULL;
}

/*===========================================================================
 * 공개 API
 *===========================================================================*/

json_value_t* json_parse(const char* json, size_t len)
{
    if (!json || len == 0) return NULL;

    json_parser_t p = { json, len, 0 };
    return parse_value(&p);
}

/* 내부용: 값의 내용만 해제 (value 자체는 해제하지 않음) */
static void json_free_contents(json_value_t* value)
{
    if (!value) return;

    switch (value->type) {
        case JSON_STRING:
            free(value->data.string.str);
            value->data.string.str = NULL;
            break;

        case JSON_ARRAY:
            for (size_t i = 0; i < value->data.array.count; i++) {
                json_free_contents(&value->data.array.items[i]);
            }
            free(value->data.array.items);
            value->data.array.items = NULL;
            break;

        case JSON_OBJECT:
            for (size_t i = 0; i < value->data.object.count; i++) {
                free(value->data.object.keys[i]);
                json_free_contents(&value->data.object.values[i]);
            }
            free(value->data.object.keys);
            free(value->data.object.values);
            value->data.object.keys = NULL;
            value->data.object.values = NULL;
            break;

        default:
            break;
    }
    value->type = JSON_NULL;
}

void json_free(json_value_t* value)
{
    if (!value) return;
    json_free_contents(value);
    free(value);
}

json_value_t* json_object_get(const json_value_t* obj, const char* key)
{
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;

    for (size_t i = 0; i < obj->data.object.count; i++) {
        if (strcmp(obj->data.object.keys[i], key) == 0) {
            return &obj->data.object.values[i];
        }
    }
    return NULL;
}

json_value_t* json_array_get(const json_value_t* arr, size_t index)
{
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    if (index >= arr->data.array.count) return NULL;
    return &arr->data.array.items[index];
}

int json_get_int(const json_value_t* val, int default_val)
{
    if (!val || val->type != JSON_NUMBER) return default_val;
    return (int)val->data.num_val;
}

const char* json_get_string(const json_value_t* val)
{
    if (!val || val->type != JSON_STRING) return NULL;
    return val->data.string.str;
}

bool json_get_bool(const json_value_t* val, bool default_val)
{
    if (!val || val->type != JSON_BOOL) return default_val;
    return val->data.bool_val;
}

size_t json_array_size(const json_value_t* arr)
{
    if (!arr || arr->type != JSON_ARRAY) return 0;
    return arr->data.array.count;
}

/*===========================================================================
 * JSON 빌더
 *===========================================================================*/

void json_builder_init(json_builder_t* b)
{
    b->pos = 0;
    b->depth = 0;
    b->need_comma = false;
    b->buffer[0] = '\0';
}

static void builder_append(json_builder_t* b, const char* str)
{
    size_t len = strlen(str);
    if (b->pos + len < JSON_BUILD_MAX - 1) {
        strcpy(&b->buffer[b->pos], str);
        b->pos += len;
    }
}

static void builder_comma(json_builder_t* b)
{
    if (b->need_comma) {
        builder_append(b, ",");
    }
    b->need_comma = false;
}

void json_builder_object_start(json_builder_t* b)
{
    builder_comma(b);
    builder_append(b, "{");
    b->depth++;
    b->need_comma = false;
}

void json_builder_object_end(json_builder_t* b)
{
    builder_append(b, "}");
    b->depth--;
    b->need_comma = true;
}

void json_builder_array_start(json_builder_t* b)
{
    builder_comma(b);
    builder_append(b, "[");
    b->depth++;
    b->need_comma = false;
}

void json_builder_array_end(json_builder_t* b)
{
    builder_append(b, "]");
    b->depth--;
    b->need_comma = true;
}

void json_builder_key(json_builder_t* b, const char* key)
{
    builder_comma(b);
    builder_append(b, "\"");
    builder_append(b, key);
    builder_append(b, "\":");
    b->need_comma = false;
}

void json_builder_string(json_builder_t* b, const char* value)
{
    builder_comma(b);
    builder_append(b, "\"");
    /* 간단한 이스케이프 */
    for (const char* p = value; *p; p++) {
        char c = *p;
        if (c == '"' || c == '\\') {
            char esc[3] = { '\\', c, '\0' };
            builder_append(b, esc);
        } else if (c == '\n') {
            builder_append(b, "\\n");
        } else if (c == '\r') {
            builder_append(b, "\\r");
        } else if (c == '\t') {
            builder_append(b, "\\t");
        } else {
            char ch[2] = { c, '\0' };
            builder_append(b, ch);
        }
    }
    builder_append(b, "\"");
    b->need_comma = true;
}

void json_builder_int(json_builder_t* b, int value)
{
    builder_comma(b);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    builder_append(b, buf);
    b->need_comma = true;
}

void json_builder_bool(json_builder_t* b, bool value)
{
    builder_comma(b);
    builder_append(b, value ? "true" : "false");
    b->need_comma = true;
}

void json_builder_null(json_builder_t* b)
{
    builder_comma(b);
    builder_append(b, "null");
    b->need_comma = true;
}

const char* json_builder_get(json_builder_t* b)
{
    return b->buffer;
}

size_t json_builder_len(json_builder_t* b)
{
    return b->pos;
}
