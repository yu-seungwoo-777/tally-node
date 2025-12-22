/**
 * @file obs_client.c
 * @brief OBS WebSocket 클라이언트 구현 (순수 C)
 *
 * 외부 라이브러리 의존성 없이 순수 C로 구현
 */

#include "obs_client.h"
#include "obs_websocket.h"
#include "obs_json.h"
#include "obs_sha256.h"
#include "obs_base64.h"
#include "sw_platform.h"
#include "log.h"
#include "log_tags.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * 디버그 매크로 (로거 사용)
 *===========================================================================*/

#define OBS_LOGE(fmt, ...) LOG_0(TAG_OBS, fmt, ##__VA_ARGS__)
#define OBS_LOGW(fmt, ...) LOG_0(TAG_OBS, fmt, ##__VA_ARGS__)
#define OBS_LOGI(fmt, ...) LOG_0(TAG_OBS, fmt, ##__VA_ARGS__)
#define OBS_LOGV(fmt, ...) LOG_1(TAG_OBS, fmt, ##__VA_ARGS__)

/*===========================================================================
 * 내부 구조체 접근
 *===========================================================================*/

/* obs_client_t의 _ws_reserved 영역을 ws_client_t로 사용 */
#define GET_WS(client) ((ws_client_t*)((client)->_ws_reserved))

/*===========================================================================
 * 내부 함수 선언
 *===========================================================================*/

static void on_ws_connected(void* user_data);
static void on_ws_disconnected(void* user_data);
static void on_ws_message(const char* data, size_t len, void* user_data);
static void handle_message(obs_client_t* client, const char* data, size_t len);
static void handle_hello(obs_client_t* client, json_value_t* d);
static void handle_identified(obs_client_t* client, json_value_t* d);
static void handle_event(obs_client_t* client, json_value_t* d);
static void handle_request_response(obs_client_t* client, json_value_t* d);
static int send_identify(obs_client_t* client);
static int send_request(obs_client_t* client, const char* request_type, const char* scene_name);
static char* generate_auth_string(const char* password, const char* salt, const char* challenge);

/*===========================================================================
 * 초기화 및 정리
 *===========================================================================*/

int obs_client_init(obs_client_t* client, const char* host, uint16_t port, const char* password)
{
    if (!client || !host) return -1;

    memset(client, 0, sizeof(obs_client_t));

    strncpy(client->host, host, sizeof(client->host) - 1);
    client->port = port ? port : OBS_DEFAULT_PORT;

    if (password && password[0]) {
        strncpy(client->password, password, sizeof(client->password) - 1);
    }

    client->socket_fd = -1;
    obs_state_init(&client->state);
    client->debug_level = OBS_DEBUG_NONE;  /* 기본값: 디버그 꺼짐 */

    return 0;
}

void obs_client_cleanup(obs_client_t* client)
{
    if (!client) return;
    obs_client_disconnect(client);
    memset(client, 0, sizeof(obs_client_t));
    client->socket_fd = -1;
}

/*===========================================================================
 * 연결 관리
 *===========================================================================*/

int obs_client_connect(obs_client_t* client, uint32_t timeout_ms)
{
    if (!client) return -1;

    ws_client_t* ws = GET_WS(client);

    /* WebSocket 클라이언트 초기화 */
    if (ws_client_init(ws, client->host, client->port, "/") < 0) {
        OBS_LOGE("WebSocket init failed\n");
        return -1;
    }

    /* 콜백 설정 */
    ws_client_set_callbacks(ws,
                            on_ws_connected,
                            on_ws_disconnected,
                            on_ws_message,
                            client);

    LOG_0(TAG_OBS, "Connecting to ws://%s:%d", client->host, client->port);

    /* WebSocket 연결 */
    if (ws_client_connect(ws, timeout_ms) < 0) {
        LOG_0(TAG_OBS, "WebSocket connect failed");
        ws_client_cleanup(ws);
        return -1;
    }

    /* 인증 완료 대기 */
    uint32_t start = sw_platform_millis();
    while (!client->state.authenticated) {
        ws_client_loop(ws);

        if (sw_platform_millis() - start > timeout_ms) {
            LOG_0(TAG_OBS, "Authentication timeout");
            obs_client_disconnect(client);
            return -1;
        }

        if (!ws_client_is_connected(ws)) {
            LOG_0(TAG_OBS, "Connection lost during auth");
            obs_client_disconnect(client);
            return -1;
        }

        sw_platform_delay(10);
    }

    return 0;
}

int obs_client_connect_start(obs_client_t* client)
{
    if (!client) return -1;

    ws_client_t* ws = GET_WS(client);

    /* WebSocket 클라이언트 초기화 */
    if (ws_client_init(ws, client->host, client->port, "/") < 0) {
        OBS_LOGE("WebSocket init failed\n");
        return -1;
    }

    /* 콜백 설정 */
    ws_client_set_callbacks(ws,
                            on_ws_connected,
                            on_ws_disconnected,
                            on_ws_message,
                            client);

    LOG_0(TAG_OBS, "Connecting to ws://%s:%d", client->host, client->port);

    /* WebSocket 연결 시작 */
    int ret = ws_client_connect_start(ws);
    if (ret < 0) {
        LOG_0(TAG_OBS, "WebSocket connect start failed");
        ws_client_cleanup(ws);
        return -1;
    }

    return ret;  /* 0: 성공, 1: 진행중 */
}

int obs_client_connect_check(obs_client_t* client)
{
    if (!client) return -1;

    ws_client_t* ws = GET_WS(client);

    /* WebSocket 연결 체크 */
    int ret = ws_client_connect_check(ws);
    if (ret < 0) {
        LOG_0(TAG_OBS, "WebSocket connect failed");
        obs_client_disconnect(client);
        return -1;
    }

    if (ret == 1) {
        /* WebSocket 연결 진행 중 */
        return 1;
    }

    /* WebSocket 연결 완료 - 인증 확인 */
    if (!client->state.authenticated) {
        /* 인증 진행 중 - loop 호출 필요 */
        ws_client_loop(ws);

        if (!ws_client_is_connected(ws)) {
            LOG_0(TAG_OBS, "Connection lost during auth");
            obs_client_disconnect(client);
            return -1;
        }

        return 1;  /* 인증 진행 중 */
    }

    /* 인증 완료 */
    return 0;
}

void obs_client_disconnect(obs_client_t* client)
{
    if (!client) return;

    ws_client_t* ws = GET_WS(client);

    ws_client_disconnect(ws);
    ws_client_cleanup(ws);

    client->state.connected = false;
    client->state.authenticated = false;
}

bool obs_client_is_connected(const obs_client_t* client)
{
    if (!client) return false;
    return client->state.connected && client->state.authenticated;
}

bool obs_client_is_initialized(const obs_client_t* client)
{
    if (!client) return false;
    return client->state.initialized;
}

int obs_client_wait_init(obs_client_t* client, uint32_t timeout_ms)
{
    if (!client || !obs_client_is_connected(client)) {
        return -1;
    }

    uint32_t start = sw_platform_millis();
    while (sw_platform_millis() - start < timeout_ms) {
        obs_client_loop(client);

        if (client->state.initialized) {
            return 0;
        }

        if (!obs_client_is_connected(client)) {
            return -1;
        }

        /* 최소 delay - 다른 태스크에 양보 */
        sw_platform_delay(1);
    }

    return -1;  /* 타임아웃 */
}

int obs_client_loop(obs_client_t* client)
{
    if (!client) return -1;

    ws_client_t* ws = GET_WS(client);
    int result = ws_client_loop(ws);

    if (result < 0) {
        return result;
    }

    /* 인증 완료 후에만 keepalive/타임아웃 처리 */
    if (!client->state.authenticated) {
        return result;
    }

    uint32_t now = sw_platform_millis();

    /* Keepalive (WebSocket Ping) 전송 */
    if (now - client->state.last_keepalive_ms > OBS_KEEPALIVE_INTERVAL_MS) {
        ws_client_send_ping(ws);
        client->state.last_keepalive_ms = now;
        OBS_LOGV("Keepalive ping sent\n");
    }

    /* 타임아웃 체크 */
    if (client->state.last_contact_ms > 0 &&
        now - client->state.last_contact_ms > OBS_MAX_SILENCE_TIME_MS) {
        OBS_LOGW("Connection timeout (no response for %dms)\n",
                      (int)(now - client->state.last_contact_ms));
        obs_client_disconnect(client);
        return -1;
    }

    return result;
}

/*===========================================================================
 * 상태 조회
 *===========================================================================*/

int16_t obs_client_get_program_scene(const obs_client_t* client)
{
    if (!client) return -1;
    return client->state.program_scene_index;
}

int16_t obs_client_get_preview_scene(const obs_client_t* client)
{
    if (!client) return -1;
    return client->state.preview_scene_index;
}

uint8_t obs_client_get_scene_count(const obs_client_t* client)
{
    if (!client) return 0;
    return client->state.num_cameras;
}

const char* obs_client_get_scene_name(const obs_client_t* client, uint8_t index)
{
    if (!client || index >= client->state.num_cameras) return NULL;
    return client->state.scenes[index].name;
}

uint8_t obs_client_get_tally_by_index(const obs_client_t* client, uint8_t index)
{
    if (!client) return OBS_TALLY_OFF;
    return obs_tally_get(client->state.tally_packed, index);
}

uint64_t obs_client_get_tally_packed(const obs_client_t* client)
{
    if (!client) return 0;
    return client->state.tally_packed;
}

bool obs_client_is_studio_mode(const obs_client_t* client)
{
    if (!client) return false;
    return client->state.studio_mode;
}

/*===========================================================================
 * 제어
 *===========================================================================*/

int obs_client_set_program_scene(obs_client_t* client, uint8_t index)
{
    if (!client || index >= client->state.num_cameras) return -1;
    return obs_client_set_program_scene_by_name(client, client->state.scenes[index].name);
}

int obs_client_set_program_scene_by_name(obs_client_t* client, const char* name)
{
    if (!client || !name || !obs_client_is_connected(client)) return -1;
    return send_request(client, OBS_REQUEST_SET_CURRENT_PROGRAM, name);
}

int obs_client_set_preview_scene(obs_client_t* client, uint8_t index)
{
    if (!client || index >= client->state.num_cameras) return -1;
    return obs_client_set_preview_scene_by_name(client, client->state.scenes[index].name);
}

int obs_client_set_preview_scene_by_name(obs_client_t* client, const char* name)
{
    if (!client || !name || !obs_client_is_connected(client)) return -1;
    return send_request(client, OBS_REQUEST_SET_CURRENT_PREVIEW, name);
}

int obs_client_refresh_scenes(obs_client_t* client)
{
    if (!client || !obs_client_is_connected(client)) return -1;
    return send_request(client, OBS_REQUEST_GET_SCENE_LIST, NULL);
}

int obs_client_cut(obs_client_t* client)
{
    if (!client || !obs_client_is_connected(client)) return -1;

    /* Studio Mode인 경우 Preview를 Program으로 즉시 전환 */
    if (client->state.studio_mode && client->state.preview_scene_index >= 0) {
        return obs_client_set_program_scene(client, (uint8_t)client->state.preview_scene_index);
    }

    return 0;  /* Studio Mode 아니면 무시 */
}

int obs_client_auto(obs_client_t* client)
{
    if (!client || !obs_client_is_connected(client)) return -1;

    /* Studio Mode에서만 동작 */
    if (!client->state.studio_mode) {
        OBS_LOGI("Auto requires Studio Mode");
        return -1;
    }

    return send_request(client, OBS_REQUEST_TRIGGER_TRANSITION, NULL);
}

int obs_client_set_studio_mode(obs_client_t* client, bool enabled)
{
    if (!client || !obs_client_is_connected(client)) return -1;

    ws_client_t* ws = GET_WS(client);

    json_builder_t b;
    json_builder_init(&b);

    json_builder_object_start(&b);
    json_builder_key(&b, "op");
    json_builder_int(&b, OBS_OP_REQUEST);

    json_builder_key(&b, "d");
    json_builder_object_start(&b);

    json_builder_key(&b, "requestType");
    json_builder_string(&b, OBS_REQUEST_SET_STUDIO_MODE);

    char req_id[32];
    snprintf(req_id, sizeof(req_id), "%lu", (unsigned long)client->state.next_request_id++);
    json_builder_key(&b, "requestId");
    json_builder_string(&b, req_id);

    json_builder_key(&b, "requestData");
    json_builder_object_start(&b);
    json_builder_key(&b, "studioModeEnabled");
    json_builder_bool(&b, enabled);
    json_builder_object_end(&b);

    json_builder_object_end(&b);  /* d */
    json_builder_object_end(&b);  /* root */

    OBS_LOGV("Sending: %s\n", json_builder_get(&b));

    return ws_client_send_text(ws, json_builder_get(&b), json_builder_len(&b));
}

/*===========================================================================
 * 콜백 설정
 *===========================================================================*/

void obs_client_set_on_connected(obs_client_t* client, obs_callback_t callback, void* user_data)
{
    if (!client) return;
    client->on_connected = callback;
    client->on_connected_data = user_data;
}

void obs_client_set_on_disconnected(obs_client_t* client, obs_callback_t callback, void* user_data)
{
    if (!client) return;
    client->on_disconnected = callback;
    client->on_disconnected_data = user_data;
}

void obs_client_set_on_scene_changed(obs_client_t* client, obs_callback_t callback, void* user_data)
{
    if (!client) return;
    client->on_scene_changed = callback;
    client->on_scene_changed_data = user_data;
}

void obs_client_set_on_authenticated(obs_client_t* client, obs_callback_t callback, void* user_data)
{
    if (!client) return;
    client->on_authenticated = callback;
    client->on_authenticated_data = user_data;
}

/*===========================================================================
 * 디버그
 *===========================================================================*/

void obs_client_set_debug(obs_client_t* client, obs_debug_level_t level)
{
    if (!client) return;
    client->debug_level = level;

    /* OBS 레벨 → ESP 로그 레벨 매핑 */
    static const esp_log_level_t level_map[] = {
        ESP_LOG_NONE,    /* OBS_DEBUG_NONE(0) */
        ESP_LOG_ERROR,   /* OBS_DEBUG_ERROR(1) */
        ESP_LOG_INFO,    /* OBS_DEBUG_INFO(2) */
        ESP_LOG_DEBUG    /* OBS_DEBUG_VERBOSE(3) */
    };

    if (level <= OBS_DEBUG_VERBOSE) {
        esp_log_level_set(TAG_OBS, level_map[level]);
    }
}

/*===========================================================================
 * WebSocket 콜백
 *===========================================================================*/

static void on_ws_connected(void* user_data)
{
    obs_client_t* client = (obs_client_t*)user_data;
    OBS_LOGI("WebSocket connected");
    client->state.connected = true;
}

static void on_ws_disconnected(void* user_data)
{
    obs_client_t* client = (obs_client_t*)user_data;
    OBS_LOGI("WebSocket disconnected");

    bool was_connected = client->state.connected;
    client->state.connected = false;
    client->state.authenticated = false;

    if (was_connected && client->on_disconnected) {
        client->on_disconnected(client->on_disconnected_data);
    }
}

static void on_ws_message(const char* data, size_t len, void* user_data)
{
    obs_client_t* client = (obs_client_t*)user_data;
    /* 메시지 수신 시 last_contact 업데이트 */
    client->state.last_contact_ms = sw_platform_millis();
    handle_message(client, data, len);
}

/*===========================================================================
 * 메시지 처리
 *===========================================================================*/

static void handle_message(obs_client_t* client, const char* data, size_t len)
{
    /* 빈 메시지는 Pong 응답 (last_contact_ms 업데이트용) */
    if (len == 0) {
        OBS_LOGV("Pong received\n");
        return;
    }

    OBS_LOGV("Received: %.*s\n", (int)len, data);

    json_value_t* json = json_parse(data, len);
    if (!json) {
        OBS_LOGE("JSON parse error\n");
        return;
    }

    json_value_t* op_val = json_object_get(json, "op");
    json_value_t* d_val = json_object_get(json, "d");

    int op = json_get_int(op_val, -1);

    switch (op) {
        case OBS_OP_HELLO:
            handle_hello(client, d_val);
            break;

        case OBS_OP_IDENTIFIED:
            handle_identified(client, d_val);
            break;

        case OBS_OP_EVENT:
            handle_event(client, d_val);
            break;

        case OBS_OP_REQUEST_RESPONSE:
            handle_request_response(client, d_val);
            break;

        default:
            OBS_LOGV("Unknown opcode: %d\n", op);
            break;
    }

    json_free(json);
}

static void handle_hello(obs_client_t* client, json_value_t* d)
{
    OBS_LOGI("Received Hello");

    if (!d) return;

    /* 인증 정보 확인 */
    json_value_t* auth = json_object_get(d, "authentication");
    if (auth) {
        json_value_t* challenge = json_object_get(auth, "challenge");
        json_value_t* salt = json_object_get(auth, "salt");

        const char* challenge_str = json_get_string(challenge);
        const char* salt_str = json_get_string(salt);

        if (challenge_str && salt_str) {
            strncpy(client->state.challenge, challenge_str, OBS_AUTH_STRING_MAX - 1);
            strncpy(client->state.salt, salt_str, OBS_AUTH_STRING_MAX - 1);
            client->state.auth_required = true;
            LOG_0(TAG_OBS, "Authentication required");
        }
    } else {
        client->state.auth_required = false;
        LOG_0(TAG_OBS, "No authentication required");
    }

    /* Identify 전송 */
    send_identify(client);
}

static void handle_identified(obs_client_t* client, json_value_t* d)
{
    (void)d;

    LOG_0(TAG_OBS, "Authenticated successfully");

    client->state.authenticated = true;

    if (client->on_authenticated) {
        client->on_authenticated(client->on_authenticated_data);
    }

    if (client->on_connected) {
        client->on_connected(client->on_connected_data);
    }

    /* Scene 목록 요청 */
    send_request(client, OBS_REQUEST_GET_SCENE_LIST, NULL);

    /* Studio Mode 상태 요청 */
    send_request(client, OBS_REQUEST_GET_STUDIO_MODE, NULL);
}

static void handle_event(obs_client_t* client, json_value_t* d)
{
    if (!d) return;

    json_value_t* event_type = json_object_get(d, "eventType");
    json_value_t* event_data = json_object_get(d, "eventData");

    const char* type = json_get_string(event_type);
    if (!type) return;

    bool scene_changed = false;

    if (strcmp(type, OBS_EVENT_CURRENT_PROGRAM_CHANGED) == 0) {
        if (event_data) {
            json_value_t* name = json_object_get(event_data, "sceneName");
            const char* name_str = json_get_string(name);
            if (name_str) {
                strncpy(client->state.program_scene_name, name_str, OBS_SCENE_NAME_MAX - 1);
                client->state.program_scene_index = obs_state_find_scene_index(&client->state, name_str);
                OBS_LOGI("Program scene: %s (idx=%d)", name_str, client->state.program_scene_index);
                scene_changed = true;
            }
        }
    }
    else if (strcmp(type, OBS_EVENT_CURRENT_PREVIEW_CHANGED) == 0) {
        if (event_data) {
            json_value_t* name = json_object_get(event_data, "sceneName");
            const char* name_str = json_get_string(name);
            if (name_str) {
                strncpy(client->state.preview_scene_name, name_str, OBS_SCENE_NAME_MAX - 1);
                client->state.preview_scene_index = obs_state_find_scene_index(&client->state, name_str);
                OBS_LOGI("Preview scene: %s (idx=%d)", name_str, client->state.preview_scene_index);
                scene_changed = true;
            }
        }
    }
    else if (strcmp(type, OBS_EVENT_STUDIO_MODE_CHANGED) == 0) {
        if (event_data) {
            json_value_t* enabled = json_object_get(event_data, "studioModeEnabled");
            client->state.studio_mode = json_get_bool(enabled, false);
            OBS_LOGI("Studio mode: %s", client->state.studio_mode ? "enabled" : "disabled");
            if (!client->state.studio_mode) {
                client->state.preview_scene_index = -1;
                client->state.preview_scene_name[0] = '\0';
            }
            scene_changed = true;
        }
    }
    else if (strcmp(type, OBS_EVENT_SCENE_LIST_CHANGED) == 0) {
        /* Scene 목록 변경 시 다시 요청 */
        send_request(client, OBS_REQUEST_GET_SCENE_LIST, NULL);
    }

    if (scene_changed) {
        LOG_1(TAG_OBS, "Scene changed - PGM: %d, PVW: %d",
              client->state.program_scene_index, client->state.preview_scene_index);
        obs_state_update_tally(&client->state);
        LOG_1(TAG_OBS, "Tally updated: 0x%016llX", (unsigned long long)client->state.tally_packed);
        if (client->on_scene_changed) {
            client->on_scene_changed(client->on_scene_changed_data);
        }
    }
}

static void handle_request_response(obs_client_t* client, json_value_t* d)
{
    if (!d) return;

    json_value_t* request_type = json_object_get(d, "requestType");
    json_value_t* response_data = json_object_get(d, "responseData");

    const char* type = json_get_string(request_type);
    if (!type) {
        LOG_0(TAG_OBS, "RequestSuccess: no requestType");
        return;
    }

    // GetStudioModeEnabled는 로그 출력하지 않음 (토폴로지 출력 후 늦게 도착하는 것 방지)
    if (strcmp(type, OBS_REQUEST_GET_STUDIO_MODE) != 0) {
        LOG_0(TAG_OBS, "RequestSuccess: %s", type);
    }

    if (strcmp(type, OBS_REQUEST_GET_SCENE_LIST) == 0 && response_data) {
        LOG_0(TAG_OBS, "Parsing scene list...");
        /* Scene 목록 파싱 */
        json_value_t* scenes = json_object_get(response_data, "scenes");
        if (scenes) {
            client->state.num_cameras = 0;

            /* OBS는 역순으로 반환 (마지막 scene이 index 0) */
            size_t count = json_array_size(scenes);
            LOG_1(TAG_OBS, "Total scenes in response: %d", (int)count);
            for (int i = (int)count - 1; i >= 0 && client->state.num_cameras < OBS_MAX_SCENES; i--) {
                json_value_t* scene = json_array_get(scenes, (size_t)i);
                json_value_t* name = json_object_get(scene, "sceneName");
                const char* name_str = json_get_string(name);
                if (name_str) {
                    strncpy(client->state.scenes[client->state.num_cameras].name,
                            name_str, OBS_SCENE_NAME_MAX - 1);
                    client->state.scenes[client->state.num_cameras].index = client->state.num_cameras;
                    LOG_1(TAG_OBS, "  Scene[%d]: %s", client->state.num_cameras, name_str);
                    client->state.num_cameras++;
                }
            }

            LOG_1(TAG_OBS, "Parsed %d scenes", client->state.num_cameras);

            /* Scene 개수 업데이트 후 effective 카메라 제한 재계산 */
            obs_state_update_camera_limit(&client->state);
        }

        /* 현재 Program Scene */
        json_value_t* pgm_name = json_object_get(response_data, "currentProgramSceneName");
        const char* pgm_str = json_get_string(pgm_name);
        if (pgm_str) {
            strncpy(client->state.program_scene_name, pgm_str, OBS_SCENE_NAME_MAX - 1);
            client->state.program_scene_index = obs_state_find_scene_index(&client->state, pgm_str);
            OBS_LOGI("Program: %s (idx=%d)", pgm_str, client->state.program_scene_index);
        }

        /* 현재 Preview Scene */
        json_value_t* pvw_name = json_object_get(response_data, "currentPreviewSceneName");
        const char* pvw_str = json_get_string(pvw_name);
        if (pvw_str) {
            strncpy(client->state.preview_scene_name, pvw_str, OBS_SCENE_NAME_MAX - 1);
            client->state.preview_scene_index = obs_state_find_scene_index(&client->state, pvw_str);
            OBS_LOGI("Preview: %s (idx=%d)", pvw_str, client->state.preview_scene_index);
        }

        obs_state_update_tally(&client->state);

        /* Scene List 파싱 완료 → 초기화 완료 */
        if (!client->state.initialized) {
            client->state.initialized = true;
            OBS_LOGI("초기화 완료 (Scene List 파싱됨)");
        }

        if (client->on_scene_changed) {
            client->on_scene_changed(client->on_scene_changed_data);
        }
    }
    else if (strcmp(type, OBS_REQUEST_GET_STUDIO_MODE) == 0 && response_data) {
        json_value_t* enabled = json_object_get(response_data, "studioModeEnabled");
        client->state.studio_mode = json_get_bool(enabled, false);
        // OBS_LOGI("Studio mode: %s\n", client->state.studio_mode ? "enabled" : "disabled");
    }
    else if (strcmp(type, OBS_REQUEST_SET_STUDIO_MODE) == 0) {
        /* SetStudioModeEnabled 성공 시 상태 새로고침 */
        send_request(client, OBS_REQUEST_GET_STUDIO_MODE, NULL);
    }
    /* Scene 변경 (SetProgram/SetPreview) 성공 시:
     * - Scene List는 변경되지 않으므로 재요청 불필요
     * - Event (CurrentProgramSceneChanged/CurrentPreviewSceneChanged)로 자동 업데이트됨
     */
}

/*===========================================================================
 * 메시지 전송
 *===========================================================================*/

static int send_identify(obs_client_t* client)
{
    ws_client_t* ws = GET_WS(client);

    json_builder_t b;
    json_builder_init(&b);

    json_builder_object_start(&b);
    json_builder_key(&b, "op");
    json_builder_int(&b, OBS_OP_IDENTIFY);

    json_builder_key(&b, "d");
    json_builder_object_start(&b);

    json_builder_key(&b, "rpcVersion");
    json_builder_int(&b, OBS_RPC_VERSION);

    /* 인증이 필요한 경우 */
    if (client->state.auth_required && client->password[0]) {
        char* auth_string = generate_auth_string(
            client->password,
            client->state.salt,
            client->state.challenge
        );
        if (auth_string) {
            json_builder_key(&b, "authentication");
            json_builder_string(&b, auth_string);
            free(auth_string);
        }
    }

    /* 이벤트 구독 */
    json_builder_key(&b, "eventSubscriptions");
    json_builder_int(&b, OBS_EVENT_TALLY);

    json_builder_object_end(&b);  /* d */
    json_builder_object_end(&b);  /* root */

    OBS_LOGV("Sending: %s\n", json_builder_get(&b));

    return ws_client_send_text(ws, json_builder_get(&b), json_builder_len(&b));
}

static int send_request(obs_client_t* client, const char* request_type, const char* scene_name)
{
    ws_client_t* ws = GET_WS(client);

    json_builder_t b;
    json_builder_init(&b);

    json_builder_object_start(&b);
    json_builder_key(&b, "op");
    json_builder_int(&b, OBS_OP_REQUEST);

    json_builder_key(&b, "d");
    json_builder_object_start(&b);

    json_builder_key(&b, "requestType");
    json_builder_string(&b, request_type);

    char req_id[32];
    snprintf(req_id, sizeof(req_id), "%lu", (unsigned long)client->state.next_request_id++);
    json_builder_key(&b, "requestId");
    json_builder_string(&b, req_id);

    if (scene_name) {
        json_builder_key(&b, "requestData");
        json_builder_object_start(&b);
        json_builder_key(&b, "sceneName");
        json_builder_string(&b, scene_name);
        json_builder_object_end(&b);
    }

    json_builder_object_end(&b);  /* d */
    json_builder_object_end(&b);  /* root */

    OBS_LOGV("Sending: %s\n", json_builder_get(&b));

    return ws_client_send_text(ws, json_builder_get(&b), json_builder_len(&b));
}

static char* generate_auth_string(const char* password, const char* salt, const char* challenge)
{
    if (!password || !salt || !challenge) return NULL;

    /* Step 1: SHA256(password + salt) */
    size_t secret_len = strlen(password) + strlen(salt);
    char* secret = (char*)malloc(secret_len + 1);
    if (!secret) return NULL;

    strcpy(secret, password);
    strcat(secret, salt);

    uint8_t hash1[SHA256_DIGEST_SIZE];
    sha256((const uint8_t*)secret, secret_len, hash1);
    free(secret);

    /* Step 2: Base64(hash1) */
    char base64_secret[64];
    base64_encode(hash1, SHA256_DIGEST_SIZE, base64_secret);

    /* Step 3: SHA256(base64_secret + challenge) */
    size_t auth_len = strlen(base64_secret) + strlen(challenge);
    char* auth_string = (char*)malloc(auth_len + 1);
    if (!auth_string) return NULL;

    strcpy(auth_string, base64_secret);
    strcat(auth_string, challenge);

    uint8_t hash2[SHA256_DIGEST_SIZE];
    sha256((const uint8_t*)auth_string, auth_len, hash2);
    free(auth_string);

    /* Step 4: Base64(hash2) */
    char* result = (char*)malloc(64);
    if (!result) return NULL;

    base64_encode(hash2, SHA256_DIGEST_SIZE, result);

    return result;
}
