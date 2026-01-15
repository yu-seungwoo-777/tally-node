# Web Server Refactoring Guide

## Overview

Tally Node 웹서버 코드의 반복 패턴을 제거하고 유지보수성을 향상시키기 위한 리팩토링 가이드입니다.

## Current Codebase Analysis

### 파일 구조

```
components/02_presentation/web_server/
├── web_server.cpp              # 메인 서버 (672줄)
├── web_server_events.cpp       # 이벤트 핸들러
├── web_server_cache.cpp        # 캐시 관리
├── web_server_json.h           # JSON 헬퍼
└── handlers/
    ├── api_status.cpp          # 상태 API
    ├── api_config.cpp          # 설정 API
    ├── api_devices.cpp         # 디바이스 API
    ├── api_lora.cpp            # LoRa API
    ├── api_test.cpp            # 테스트 API
    ├── api_led.cpp             # LED API
    ├── api_license.cpp         # 라이선스 API
    ├── api_notices.cpp         # 공지사항 API
    └── api_static.cpp          # 정적 파일
```

### 현재 코드 현황

| 항목 | 현재 | 문제점 |
|------|------|--------|
| **URI 구조체 정의** | 40개 × 5줄 = 200줄 | 중복 선언 |
| **URI 등록 코드** | 40개 × 1줄 = 40줄 | 수동 등록 |
| **OPTIONS 핸들러** | 23개 중복 | 동일 코드 반복 |
| **CORS 헤더 설정** | 각 핸들러마다 3줄 | 중복 |
| **JSON 바디 파싱** | POST 핸들러마다 10줄 | 패턴 반복 |
| **JSON 응답 생성** | 각 핸들러마다 8줄 | 패턴 반복 |

**총 중복 코드**: 약 350줄

---

## Identified Repetitive Patterns

### 1. URI 핸들러 등록 반복

**위치**: `web_server.cpp:36-464`

```cpp
// 구조체 정의 (200줄)
static const httpd_uri_t uri_index = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t uri_css = {
    .uri = "/css/styles.css",
    .method = HTTP_GET,
    .handler = css_handler,
    .user_ctx = nullptr
};
// ... 38개 더

// 등록 (40줄)
httpd_register_uri_handler(s_server, &uri_index);
httpd_register_uri_handler(s_server, &uri_css);
// ... 38개 더
```

### 2. CORS 헤더 설정 반복

**위치**: 모든 API 핸들러 파일

```cpp
// api_status.cpp, api_config.cpp, api_devices.cpp 등
static void set_cors_headers(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}
```

### 3. JSON 바디 파싱 반복

**위치**: POST 핸들러들

```cpp
// api_config.cpp, api_led.cpp 등
char buf[512];
int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
if (ret <= 0) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
    return ESP_FAIL;
}
buf[ret] = '\0';

cJSON* root = cJSON_Parse(buf);
if (!root) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_FAIL;
}
```

### 4. JSON 응답 생성 반복

**위치**: GET 핸들러들

```cpp
cJSON* root = cJSON_CreateObject();
cJSON_AddStringToObject(root, "field1", value1);
cJSON_AddNumberToObject(root, "field2", value2);

char* json_str = cJSON_PrintUnformatted(root);
httpd_resp_set_type(req, "application/json");
httpd_resp_send(req, json_str, strlen(json_str));

cJSON_free(json_str);
cJSON_Delete(root);
```

---

## Refactoring Solution

### 1. X-Macro 패턴으로 URI 등록 자동화

**신규 파일**: `web_server_routes.h`

```cpp
#pragma once

// ============================================================================
// URI 라우팅 테이블 (중앙 정의)
// ============================================================================

#define ROUTE_LIST(X)                                                      \
    /* 정적 파일 */                                                        \
    X("/",                   HTTP_GET,  index_handler,        nullptr)    \
    X("/css/styles.css",     HTTP_GET,  css_handler,          nullptr)    \
    X("/js/app.bundle.js",   HTTP_GET,  js_handler,           nullptr)    \
    X("/vendor/alpine.js",   HTTP_GET,  alpine_handler,       nullptr)    \
    X("/favicon.ico",        HTTP_GET,  favicon_handler,      nullptr)    \
                                                                             \
    /* API - Status */                                                     \
    X("/api/status",         HTTP_GET,  api_status_handler,   nullptr)    \
    X("/api/reboot",         HTTP_POST, api_reboot_handler,   nullptr)    \
    X("/api/reboot/broadcast", HTTP_POST, api_reboot_broadcast_handler, nullptr) \
                                                                             \
    /* API - Config */                                                     \
    X("/api/config/network/ap",      HTTP_POST, api_config_post_handler, nullptr) \
    X("/api/config/network/wifi",    HTTP_POST, api_config_post_handler, nullptr) \
    X("/api/config/network/ethernet", HTTP_POST, api_config_post_handler, nullptr) \
    X("/api/config/switcher/primary",   HTTP_POST, api_config_post_handler, nullptr) \
    X("/api/config/switcher/secondary", HTTP_POST, api_config_post_handler, nullptr) \
    X("/api/config/switcher/dual",      HTTP_POST, api_config_post_handler, nullptr) \
    X("/api/config/device/rf",          HTTP_POST, api_config_post_handler, nullptr) \
                                                                             \
    /* API - LoRa */                                                       \
    X("/api/lora/scan",       HTTP_GET,  api_lora_scan_handler,   nullptr) \
    X("/api/lora/scan/start", HTTP_POST, api_lora_scan_start_handler, nullptr) \
    X("/api/lora/scan/stop",  HTTP_POST, api_lora_scan_stop_handler,  nullptr) \
                                                                             \
    /* API - Devices */                                                    \
    X("/api/devices",         HTTP_GET,  api_devices_handler,    nullptr) \
    X("/api/device/delete",   HTTP_POST, api_delete_device_handler, nullptr) \
                                                                             \
    /* API - License */                                                    \
    X("/api/license/validate", HTTP_POST, api_license_validate_handler, nullptr) \
                                                                             \
    /* API - Test */                                                       \
    X("/api/test/internet",       HTTP_POST, api_test_internet_handler, nullptr) \
    X("/api/test/license-server", HTTP_POST, api_test_license_server_handler, nullptr) \
    X("/api/test/start",          HTTP_POST, api_test_start_handler, nullptr) \
    X("/api/test/stop",           HTTP_POST, api_test_stop_handler, nullptr) \
                                                                             \
    /* API - Notices */                                                    \
    X("/api/notices",         HTTP_GET,  api_notices_handler,    nullptr) \
                                                                             \
    /* API - Device Control */                                             \
    X("/api/device/brightness", HTTP_POST, api_device_brightness_handler, nullptr) \
    X("/api/device/camera-id",  HTTP_POST, api_device_camera_id_handler,  nullptr) \
                                                                             \
    /* API - LED */                                                        \
    X("/api/led/colors",      HTTP_GET,  api_led_colors_get_handler, nullptr) \
    X("/api/led/colors",      HTTP_POST, api_led_colors_post_handler, nullptr) \
                                                                             \
    /* TX 전용 */                                                           \
    X("/api/brightness/broadcast", HTTP_POST, api_brightness_broadcast_handler, nullptr) \
    X("/api/device/ping",          HTTP_POST, api_device_ping_handler, nullptr) \
    X("/api/device/stop",          HTTP_POST, api_device_stop_handler, nullptr) \
    X("/api/device/reboot",        HTTP_POST, api_device_reboot_handler, nullptr) \
    X("/api/device/status-request", HTTP_POST, api_status_request_handler, nullptr) \
                                                                             \
    /* CORS OPTIONS */                                                     \
    X("/api/status",         HTTP_OPTIONS, options_handler, nullptr)        \
    X("/api/reboot",         HTTP_OPTIONS, options_handler, nullptr)        \
    X("/api/reboot/broadcast", HTTP_OPTIONS, options_handler, nullptr)      \
    X("/api/config",         HTTP_OPTIONS, options_handler, nullptr)        \
    X("/api/lora",           HTTP_OPTIONS, options_handler, nullptr)        \
    X("/api/devices",        HTTP_OPTIONS, options_handler, nullptr)        \
    X("/api/license/validate", HTTP_OPTIONS, options_handler, nullptr)      \
    X("/api/test",           HTTP_OPTIONS, options_handler, nullptr)        \
    X("/api/test/internet",  HTTP_OPTIONS, options_handler, nullptr)        \
    X("/api/test/license-server", HTTP_OPTIONS, options_handler, nullptr)  \
    X("/api/notices",        HTTP_OPTIONS, options_handler, nullptr)        \
    X("/api/device/brightness", HTTP_OPTIONS, options_handler, nullptr)    \
    X("/api/device/camera-id",  HTTP_OPTIONS, options_handler, nullptr)    \
    X("/api/led/colors",     HTTP_OPTIONS, options_handler, nullptr)

// ============================================================================
// 구조체 배열 생성 매크로
// ============================================================================

#define X(uri, method, handler, ctx)    { .uri = uri, .method = method, .handler = handler, .user_ctx = ctx }
static const httpd_uri_t g_routes[] = {
    ROUTE_LIST(X)
};
#undef X

#define ROUTE_COUNT (sizeof(g_routes) / sizeof(g_routes[0]))
```

### 2. 공통 헬퍼 함수 집중화

**신규 파일**: `web_server_helpers.h`

```cpp
#pragma once

#include "esp_http_server.h"
#include "cJSON.h"
#include "t_log.h"

// ============================================================================
// CORS 헤더
// ============================================================================

static inline void set_cors_headers(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

// ============================================================================
// JSON 응답 헬퍼
// ============================================================================

static inline esp_err_t send_json_response(httpd_req_t* req, cJSON* json) {
    if (!json) {
        return ESP_ERR_INVALID_ARG;
    }

    char* json_str = cJSON_PrintUnformatted(json);
    if (!json_str) {
        cJSON_Delete(json);
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_free(json_str);
    cJSON_Delete(json);
    return ESP_OK;
}

static inline esp_err_t send_json_error(httpd_req_t* req, const char* message) {
    cJSON* json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "error", message);
    }
    return send_json_response(req, json);
}

// ============================================================================
// JSON 요청 파싱 헬퍼
// ============================================================================

static inline cJSON* parse_json_body(httpd_req_t* req, char* buf, size_t buf_len) {
    int total_len = req->content_len;

    if (total_len >= buf_len) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too large");
        return nullptr;
    }

    int ret = httpd_req_recv(req, buf, buf_len - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return nullptr;
    }
    buf[ret] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }
    return root;
}

// ============================================================================
// OPTIONS 핸들러 (공통)
// ============================================================================

static esp_err_t options_handler(httpd_req_t* req) {
    set_cors_headers(req);
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}
```

### 3. web_server.cpp 간소화

**변경 전**:

```cpp
// web_server_start() 함수
esp_err_t web_server_start(void) {
    // ... 서버 시작 코드 ...

    // 정적 파일 등록 (5줄)
    httpd_register_uri_handler(s_server, &uri_index);
    httpd_register_uri_handler(s_server, &uri_css);
    httpd_register_uri_handler(s_server, &uri_js);
    httpd_register_uri_handler(s_server, &uri_alpine);
    httpd_register_uri_handler(s_server, &uri_favicon);

    // API 등록 (30줄)
    httpd_register_uri_handler(s_server, &uri_api_status);
    httpd_register_uri_handler(s_server, &uri_api_reboot);
    // ... 28개 더

    // OPTIONS 등록 (23줄)
    httpd_register_uri_handler(s_server, &uri_options_api_status);
    // ... 22개 더
}
```

**변경 후**:

```cpp
#include "web_server_routes.h"

esp_err_t web_server_start(void) {
    // ... 서버 시작 코드 ...

    // 자동 등록 (3줄)
    for (size_t i = 0; i < ROUTE_COUNT; i++) {
        httpd_register_uri_handler(s_server, &g_routes[i]);
    }

    T_LOGI(TAG, "Registered %zu routes", ROUTE_COUNT);
    return ESP_OK;
}
```

### 4. 핸들러 간소화 예시

**변경 전** (`api_status.cpp`):

```cpp
esp_err_t api_status_handler(httpd_req_t* req) {
    // CORS 헤더 (3줄)
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    // JSON 생성 (12줄)
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", "1.0.0");
    // ... 데이터 추가 ...

    char* json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}
```

**변경 후**:

```cpp
#include "web_server_helpers.h"

esp_err_t api_status_handler(httpd_req_t* req) {
    set_cors_headers(req);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", "1.0.0");
    // ... 데이터 추가 ...

    return send_json_response(req, root);
}
```

---

## Implementation Plan

### Phase 1: 헬퍼 함수 생성

- [ ] `web_server_helpers.h` 생성
- [ ] CORS, JSON 헬퍼 함수 구현
- [ ] 단위 테스트

### Phase 2: URI 라우팅 테이블

- [ ] `web_server_routes.h` 생성
- [ ] 모든 URI를 ROUTE_LIST로 정의
- [ ] 구조체 배열 자동 생성 확인

### Phase 3: web_server.cpp 리팩토링

- [ ] 수동 URI 구조체 정의 제거
- [ ] 수동 등록 코드를 루프로 대체
- [ ] OPTIONS 핸들러 통합

### Phase 4: 핸들러 파일 리팩토링

- [ ] `api_status.cpp` - 헬퍼 적용
- [ ] `api_config.cpp` - 헬퍼 적용
- [ ] `api_devices.cpp` - 헬퍼 적용
- [ ] `api_lora.cpp` - 헬퍼 적용
- [ ] `api_led.cpp` - 헬퍼 적용
- [ ] `api_test.cpp` - 헬퍼 적용
- [ ] `api_license.cpp` - 헬퍼 적용
- [ ] `api_notices.cpp` - 헬퍼 적용

### Phase 5: 검증

- [ ] 빌드 통과
- [ ] 모든 API 동작 테스트
- [ ] CORS Preflight 테스트
- [ ] 메모리 누수 확인

---

## Expected Benefits

| 항목 | 기존 | 리팩토링 후 | 절감 |
|------|------|-------------|------|
| URI 구조체 정의 | 200줄 | 60줄 (ROUTE_LIST) | -140줄 |
| URI 등록 코드 | 40줄 | 3줄 (for 루프) | -37줄 |
| OPTIONS 핸들러 | 23개 × 5줄 | ROUTE_LIST에 통합 | -115줄 |
| 핸들러 내 CORS | 11개 × 3줄 | 1줄 함수 호출 | -22줄 |
| 핸들러 내 JSON | 11개 × 15줄 | 1줄 함수 호출 | -154줄 |
| **총계** | ~672줄 | ~204줄 | **-468줄 (70%)** |

### 유지보수성 향상

- **신규 라우트 추가**: 3곳 수정 → 1줄 추가
- **CORS 정책 변경**: 11개 파일 수정 → 1개 함수 수정
- **실수 가능성**: 높음 → 낮음

---

## References

- ESP-IDF HTTP Server: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html
- X-Macro Pattern: https://en.wikipedia.org/wiki/X_Macro
