/**
 * @file obs_base64.c
 * @brief Base64 인코딩/디코딩 (순수 C)
 */

#include "obs_base64.h"

static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const uint8_t base64_decode_table[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
     52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255, 255,   0, 255, 255,
    255,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
     15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
    255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
     41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

size_t base64_encode_len(size_t input_len)
{
    return ((input_len + 2) / 3) * 4;
}

size_t base64_encode(const uint8_t* data, size_t len, char* output)
{
    size_t i, j;
    uint32_t triple;

    for (i = 0, j = 0; i < len;) {
        triple = (i < len ? data[i++] : 0) << 16;
        triple |= (i < len ? data[i++] : 0) << 8;
        triple |= (i < len ? data[i++] : 0);

        output[j++] = base64_table[(triple >> 18) & 0x3F];
        output[j++] = base64_table[(triple >> 12) & 0x3F];
        output[j++] = base64_table[(triple >> 6) & 0x3F];
        output[j++] = base64_table[triple & 0x3F];
    }

    /* 패딩 */
    size_t mod = len % 3;
    if (mod == 1) {
        output[j - 1] = '=';
        output[j - 2] = '=';
    } else if (mod == 2) {
        output[j - 1] = '=';
    }

    output[j] = '\0';
    return j;
}

size_t base64_decode_len(const char* input, size_t input_len)
{
    size_t len = (input_len / 4) * 3;

    if (input_len > 0) {
        if (input[input_len - 1] == '=') len--;
        if (input_len > 1 && input[input_len - 2] == '=') len--;
    }

    return len;
}

size_t base64_decode(const char* input, size_t input_len, uint8_t* output)
{
    size_t i, j;
    uint32_t quad;

    if (input_len % 4 != 0) {
        return 0;
    }

    size_t output_len = base64_decode_len(input, input_len);

    for (i = 0, j = 0; i < input_len;) {
        uint8_t a = base64_decode_table[(uint8_t)input[i++]];
        uint8_t b = base64_decode_table[(uint8_t)input[i++]];
        uint8_t c = base64_decode_table[(uint8_t)input[i++]];
        uint8_t d = base64_decode_table[(uint8_t)input[i++]];

        quad = (a << 18) | (b << 12) | (c << 6) | d;

        if (j < output_len) output[j++] = (uint8_t)(quad >> 16);
        if (j < output_len) output[j++] = (uint8_t)(quad >> 8);
        if (j < output_len) output[j++] = (uint8_t)quad;
    }

    return output_len;
}
