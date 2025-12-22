/**
 * Switcher 통합 핸들러 구현
 */

#include "switcher.h"
#include "switcher_dispatch.h"
#include "sw_platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ATEM 라이브러리 포함 */
#include "atem_client.h"

/* vMix 라이브러리 포함 */
#include "vmix_client.h"

/* OBS 라이브러리 포함 */
#include "obs_client.h"

/* ============================================================================
 * 내부 구조체
 * ============================================================================ */

struct switcher_handle {
    switcher_type_t type;
    char ip[SWITCHER_IP_LEN];
    uint16_t port;
    char password[SWITCHER_PASSWORD_LEN];  /* OBS 비밀번호 */
    bool debug;

    /* 콜백 */
    switcher_callbacks_t callbacks;

    /* 이전 Tally (변경 감지용) */
    uint64_t prev_tally;

    /* 네이티브 클라이언트 포인터 (테스트용 접근) */
    void* native_client;

    /* 백엔드별 클라이언트 */
    union {
        atem_client_t atem;
        vmix_client_t vmix;
        obs_client_t obs;
        /* 추후 추가: osee_client_t osee; */
        uint8_t placeholder[2048];  /* 최대 클라이언트 크기 예약 */
    } backend;
};

/* ============================================================================
 * 타입 이름
 * ============================================================================ */

static const char* g_type_names[] = {
    "Unknown",
    "ATEM",
    "vMix",
    "OBS",
    "OSEE"
};

/* ============================================================================
 * 기본 포트
 * ============================================================================ */

static uint16_t get_default_port(switcher_type_t type)
{
    switch (type) {
        case SWITCHER_TYPE_ATEM:  return 9910;
        case SWITCHER_TYPE_VMIX:  return 8099;   /* vMix TCP API */
        case SWITCHER_TYPE_OBS:   return 4455;   /* OBS WebSocket */
        case SWITCHER_TYPE_OSEE:  return 9910;   /* 확인 필요 */
        default:                  return 0;
    }
}

/* ============================================================================
 * ATEM 콜백 래퍼
 * ============================================================================ */

static void atem_on_connected_wrapper(void* user_data)
{
    switcher_t* sw = (switcher_t*)user_data;
    if (sw->callbacks.on_connected) {
        sw->callbacks.on_connected(sw->callbacks.user_data);
    }
}

static void atem_on_disconnected_wrapper(void* user_data)
{
    switcher_t* sw = (switcher_t*)user_data;
    if (sw->callbacks.on_disconnected) {
        sw->callbacks.on_disconnected(sw->callbacks.user_data);
    }
}

static void atem_on_state_changed_wrapper(const char* cmd_name, void* user_data)
{
    switcher_t* sw = (switcher_t*)user_data;

    /* TlIn 명령일 때만 Tally 변경 감지 */
    if (cmd_name[0] == 'T' && cmd_name[1] == 'l' && cmd_name[2] == 'I' && cmd_name[3] == 'n') {
        uint64_t current_tally = atem_client_get_tally_packed(&sw->backend.atem);
        if (current_tally != sw->prev_tally) {
            sw->prev_tally = current_tally;
            if (sw->callbacks.on_tally_changed) {
                sw->callbacks.on_tally_changed(current_tally, sw->callbacks.user_data);
            }
        }
    }

    if (sw->callbacks.on_state_changed) {
        sw->callbacks.on_state_changed(cmd_name, sw->callbacks.user_data);
    }
}

/* ============================================================================
 * vMix 콜백 래퍼
 * ============================================================================ */

static void vmix_on_connected_wrapper(void* user_data)
{
    switcher_t* sw = (switcher_t*)user_data;
    if (sw->callbacks.on_connected) {
        sw->callbacks.on_connected(sw->callbacks.user_data);
    }
}

static void vmix_on_disconnected_wrapper(void* user_data)
{
    switcher_t* sw = (switcher_t*)user_data;
    if (sw->callbacks.on_disconnected) {
        sw->callbacks.on_disconnected(sw->callbacks.user_data);
    }
}

static void vmix_on_tally_changed_wrapper(void* user_data)
{
    switcher_t* sw = (switcher_t*)user_data;

    uint64_t current_tally = vmix_client_get_tally_packed(&sw->backend.vmix);
    if (current_tally != sw->prev_tally) {
        sw->prev_tally = current_tally;
        if (sw->callbacks.on_tally_changed) {
            sw->callbacks.on_tally_changed(current_tally, sw->callbacks.user_data);
        }
    }

    if (sw->callbacks.on_state_changed) {
        sw->callbacks.on_state_changed("TALLY", sw->callbacks.user_data);
    }
}

/* ============================================================================
 * OBS 콜백 래퍼
 * ============================================================================ */

static void obs_on_authenticated_wrapper(void* user_data)
{
    /* Protocol Layer (obs_client)에서 이미 Scene List 요청 */
    /* Handler Layer에서는 중복 요청하지 않음 */
    (void)user_data;
}

static void obs_on_connected_wrapper(void* user_data)
{
    switcher_t* sw = (switcher_t*)user_data;
    if (sw->callbacks.on_connected) {
        sw->callbacks.on_connected(sw->callbacks.user_data);
    }
}

static void obs_on_disconnected_wrapper(void* user_data)
{
    switcher_t* sw = (switcher_t*)user_data;
    if (sw->callbacks.on_disconnected) {
        sw->callbacks.on_disconnected(sw->callbacks.user_data);
    }
}

static void obs_on_scene_changed_wrapper(void* user_data)
{
    switcher_t* sw = (switcher_t*)user_data;

    uint64_t current_tally = obs_client_get_tally_packed(&sw->backend.obs);
    if (current_tally != sw->prev_tally) {
        sw->prev_tally = current_tally;
        if (sw->callbacks.on_tally_changed) {
            sw->callbacks.on_tally_changed(current_tally, sw->callbacks.user_data);
        }
    }

    if (sw->callbacks.on_state_changed) {
        sw->callbacks.on_state_changed("SCENE", sw->callbacks.user_data);
    }
}

/* ============================================================================
 * 생성/소멸
 * ============================================================================ */

switcher_t* switcher_create_with_password(switcher_type_t type, const char* ip, uint16_t port, const char* password)
{
    if (type <= SWITCHER_TYPE_UNKNOWN || type >= SWITCHER_TYPE_MAX) {
        return NULL;
    }

    if (!ip || strlen(ip) == 0) {
        return NULL;
    }

    switcher_t* sw = (switcher_t*)calloc(1, sizeof(switcher_t));
    if (!sw) {
        return NULL;
    }

    sw->type = type;
    strncpy(sw->ip, ip, SWITCHER_IP_LEN - 1);
    sw->port = (port > 0) ? port : get_default_port(type);
    if (password && strlen(password) > 0) {
        strncpy(sw->password, password, SWITCHER_PASSWORD_LEN - 1);
    }

    /* 백엔드 초기화 */
    switch (type) {
        case SWITCHER_TYPE_ATEM:
            if (atem_client_init(&sw->backend.atem, sw->ip, sw->port) < 0) {
                free(sw);
                return NULL;
            }
            sw->native_client = &sw->backend.atem;
            /* 콜백 연결 */
            atem_client_set_on_connected(&sw->backend.atem, atem_on_connected_wrapper, sw);
            atem_client_set_on_disconnected(&sw->backend.atem, atem_on_disconnected_wrapper, sw);
            atem_client_set_on_state_changed(&sw->backend.atem, atem_on_state_changed_wrapper, sw);
            break;
        case SWITCHER_TYPE_VMIX:
            if (vmix_client_init(&sw->backend.vmix, sw->ip, sw->port) < 0) {
                free(sw);
                return NULL;
            }
            sw->native_client = &sw->backend.vmix;
            vmix_client_set_on_connected(&sw->backend.vmix, vmix_on_connected_wrapper, sw);
            vmix_client_set_on_disconnected(&sw->backend.vmix, vmix_on_disconnected_wrapper, sw);
            vmix_client_set_on_tally_changed(&sw->backend.vmix, vmix_on_tally_changed_wrapper, sw);
            break;
        case SWITCHER_TYPE_OBS:
            if (obs_client_init(&sw->backend.obs, sw->ip, sw->port, sw->password[0] ? sw->password : NULL) < 0) {
                free(sw);
                return NULL;
            }
            sw->native_client = &sw->backend.obs;
            obs_client_set_on_authenticated(&sw->backend.obs, obs_on_authenticated_wrapper, sw);
            obs_client_set_on_connected(&sw->backend.obs, obs_on_connected_wrapper, sw);
            obs_client_set_on_disconnected(&sw->backend.obs, obs_on_disconnected_wrapper, sw);
            obs_client_set_on_scene_changed(&sw->backend.obs, obs_on_scene_changed_wrapper, sw);
            break;
        case SWITCHER_TYPE_OSEE:
            /* TODO: 구현 예정 */
            free(sw);
            return NULL;

        default:
            free(sw);
            return NULL;
    }

    return sw;
}

switcher_t* switcher_create(switcher_type_t type, const char* ip, uint16_t port)
{
    return switcher_create_with_password(type, ip, port, NULL);
}

void switcher_destroy(switcher_t* sw)
{
    if (!sw) return;

    switcher_disconnect(sw);

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            atem_client_cleanup(&sw->backend.atem);
            break;
        case SWITCHER_TYPE_VMIX:
            vmix_client_cleanup(&sw->backend.vmix);
            break;
        case SWITCHER_TYPE_OBS:
            obs_client_cleanup(&sw->backend.obs);
            break;
        default:
            break;
    }

    free(sw);
}

/* ============================================================================
 * 연결 관리
 * ============================================================================ */

int switcher_connect(switcher_t* sw, uint32_t timeout_ms)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return (atem_client_connect(&sw->backend.atem, timeout_ms) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        case SWITCHER_TYPE_VMIX:
            return (vmix_client_connect(&sw->backend.vmix, timeout_ms) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        case SWITCHER_TYPE_OBS:
            return (obs_client_connect(&sw->backend.obs, timeout_ms) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

int switcher_connect_start(switcher_t* sw)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            {
                int ret = atem_client_connect_start(&sw->backend.atem);
                if (ret < 0) return SWITCHER_ERROR;
                if (ret == 0) return SWITCHER_OK;
                return 1;  /* 진행중 */
            }
        case SWITCHER_TYPE_VMIX:
            {
                int ret = vmix_client_connect_start(&sw->backend.vmix);
                if (ret < 0) return SWITCHER_ERROR;
                if (ret == 0) return SWITCHER_OK;
                return 1;  /* 진행중 */
            }
        case SWITCHER_TYPE_OBS:
            {
                int ret = obs_client_connect_start(&sw->backend.obs);
                if (ret < 0) return SWITCHER_ERROR;
                if (ret == 0) return SWITCHER_OK;
                return 1;  /* 진행중 */
            }
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

int switcher_connect_check(switcher_t* sw)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            {
                int ret = atem_client_connect_check(&sw->backend.atem);
                if (ret < 0) return SWITCHER_ERROR;
                if (ret == 0) return SWITCHER_OK;
                return 1;  /* 진행중 */
            }
        case SWITCHER_TYPE_VMIX:
            {
                int ret = vmix_client_connect_check(&sw->backend.vmix);
                if (ret < 0) return SWITCHER_ERROR;
                if (ret == 0) return SWITCHER_OK;
                return 1;  /* 진행중 */
            }
        case SWITCHER_TYPE_OBS:
            {
                int ret = obs_client_connect_check(&sw->backend.obs);
                if (ret < 0) return SWITCHER_ERROR;
                if (ret == 0) return SWITCHER_OK;
                return 1;  /* 진행중 */
            }
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

void switcher_disconnect(switcher_t* sw)
{
    if (!sw) return;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            atem_client_disconnect(&sw->backend.atem);
            break;
        case SWITCHER_TYPE_VMIX:
            vmix_client_disconnect(&sw->backend.vmix);
            break;
        case SWITCHER_TYPE_OBS:
            obs_client_disconnect(&sw->backend.obs);
            break;
        default:
            break;
    }
}

bool switcher_is_connected(const switcher_t* sw)
{
    if (!sw) return false;
    SWITCHER_DISPATCH_BOOL(sw, is_connected);
}

bool switcher_is_initialized(const switcher_t* sw)
{
    if (!sw) return false;
    SWITCHER_DISPATCH_BOOL(sw, is_initialized);
}

int switcher_wait_init(switcher_t* sw, uint32_t timeout_ms)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return (atem_client_wait_init(&sw->backend.atem, timeout_ms) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR_TIMEOUT;
        case SWITCHER_TYPE_VMIX:
            /* vMix는 별도 초기화 대기 없음 (SUBSCRIBE TALLY 전송됨) */
            return SWITCHER_OK;
        case SWITCHER_TYPE_OBS:
            /* OBS는 연결 시 Scene 목록 자동 요청, 응답 대기 필요 */
            return (obs_client_wait_init(&sw->backend.obs, timeout_ms) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR_TIMEOUT;
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

/* ============================================================================
 * 메인 루프
 * ============================================================================ */

int switcher_loop(switcher_t* sw)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;
    SWITCHER_DISPATCH_INT(sw, loop);
}

/* ============================================================================
 * 정보 조회
 * ============================================================================ */

switcher_type_t switcher_get_type(const switcher_t* sw)
{
    return sw ? sw->type : SWITCHER_TYPE_UNKNOWN;
}

const char* switcher_type_name(switcher_type_t type)
{
    if (type >= SWITCHER_TYPE_MAX) return "Unknown";
    return g_type_names[type];
}

int switcher_get_info(const switcher_t* sw, switcher_info_t* info)
{
    if (!sw || !info) return SWITCHER_ERROR_INVALID_PARAM;

    memset(info, 0, sizeof(switcher_info_t));

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM: {
            const char* name = atem_client_get_product_name(&sw->backend.atem);
            if (name) {
                strncpy(info->product_name, name, SWITCHER_MAX_NAME_LEN - 1);
            }
            info->num_cameras = atem_client_get_num_cameras(&sw->backend.atem);
            info->num_mes = atem_client_get_num_mes(&sw->backend.atem);
            return SWITCHER_OK;
        }
        case SWITCHER_TYPE_VMIX:
            strncpy(info->product_name, "vMix", SWITCHER_MAX_NAME_LEN - 1);
            info->num_cameras = vmix_client_get_tally_count(&sw->backend.vmix);
            return SWITCHER_OK;
        case SWITCHER_TYPE_OBS:
            strncpy(info->product_name, "OBS Studio", SWITCHER_MAX_NAME_LEN - 1);
            info->num_cameras = obs_client_get_scene_count(&sw->backend.obs);
            return SWITCHER_OK;
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

int switcher_get_state(const switcher_t* sw, switcher_state_t* state)
{
    if (!sw || !state) return SWITCHER_ERROR_INVALID_PARAM;

    memset(state, 0, sizeof(switcher_state_t));

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            state->connected = atem_client_is_connected(&sw->backend.atem);
            state->initialized = atem_client_is_initialized(&sw->backend.atem);
            state->program_input = atem_client_get_program_input(&sw->backend.atem, 0);
            state->preview_input = atem_client_get_preview_input(&sw->backend.atem, 0);
            state->tally_packed = atem_client_get_tally_packed(&sw->backend.atem);
            state->in_transition = atem_client_is_in_transition(&sw->backend.atem, 0);
            state->transition_position = atem_client_get_transition_position(&sw->backend.atem, 0);
            return SWITCHER_OK;
        case SWITCHER_TYPE_VMIX:
            state->connected = vmix_client_is_connected(&sw->backend.vmix);
            state->initialized = state->connected;
            state->program_input = vmix_client_get_program_input(&sw->backend.vmix);
            state->preview_input = vmix_client_get_preview_input(&sw->backend.vmix);
            state->tally_packed = vmix_client_get_tally_packed(&sw->backend.vmix);
            state->in_transition = false;
            state->transition_position = 0;
            return SWITCHER_OK;
        case SWITCHER_TYPE_OBS: {
            state->connected = obs_client_is_connected(&sw->backend.obs);
            state->initialized = obs_client_is_initialized(&sw->backend.obs);
            int16_t pgm = obs_client_get_program_scene(&sw->backend.obs);
            int16_t pvw = obs_client_get_preview_scene(&sw->backend.obs);
            state->program_input = (pgm >= 0) ? (uint16_t)(pgm + 1) : 0;  /* 1-based */
            state->preview_input = (pvw >= 0) ? (uint16_t)(pvw + 1) : 0;
            state->tally_packed = obs_client_get_tally_packed(&sw->backend.obs);
            state->in_transition = false;
            state->transition_position = 0;
            return SWITCHER_OK;
        }
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

/* ============================================================================
 * 정보 출력
 * ============================================================================ */

void switcher_print_topology(const switcher_t* sw)
{
    if (!sw) return;

    printf("\n==============================\n");

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM: {
            const atem_client_t* atem = &sw->backend.atem;
            uint8_t major, minor;
            atem_client_get_version(atem, &major, &minor);

            printf(" %s (v%d.%d)\n", atem_client_get_product_name(atem), major, minor);
            printf("==============================\n");
            printf("CAM:%d SRC:%d ME:%d DSK:%d USK:%d SS:%d\n",
                   atem_client_get_num_cameras(atem),
                   atem_client_get_num_sources(atem),
                   atem_client_get_num_mes(atem),
                   atem_client_get_num_dsks(atem),
                   atem_client_get_num_keyers(atem, 0),
                   atem_client_get_num_supersources(atem));
            break;
        }
        case SWITCHER_TYPE_VMIX: {
            const vmix_client_t* vmix = &sw->backend.vmix;
            printf(" vMix\n");
            printf("==============================\n");
            printf("입력:%d\n", vmix_client_get_tally_count(vmix));
            break;
        }
        case SWITCHER_TYPE_OBS: {
            const obs_client_t* obs = &sw->backend.obs;
            printf(" OBS Studio\n");
            printf("==============================\n");
            printf("씬:%d\n", obs_client_get_scene_count(obs));
            break;
        }
        default:
            printf(" Unknown Switcher\n");
            printf("==============================\n");
            break;
    }

    /* 현재 상태 출력 */
    switcher_print_status(sw);

    printf("==============================\n\n");
}

void switcher_print_status(const switcher_t* sw)
{
    if (!sw) return;

    switcher_info_t info;
    uint8_t num_cameras = 4;  /* 기본값 */

    if (switcher_get_info(sw, &info) == SWITCHER_OK) {
        num_cameras = info.num_cameras;
        if (num_cameras > 8) num_cameras = 8;  /* 최대 8개만 표시 */
    }

    printf("Tally: ");
    for (uint8_t i = 0; i < num_cameras; i++) {
        uint8_t t = switcher_get_tally(sw, i);
        const char* state = (t == SWITCHER_TALLY_OFF) ? "-" :
                           (t == SWITCHER_TALLY_PROGRAM) ? "P" :
                           (t == SWITCHER_TALLY_PREVIEW) ? "V" : "B";
        printf("%d:%s ", i + 1, state);
    }
    printf("\n");
}

/* ============================================================================
 * Program/Preview 조회
 * ============================================================================ */

uint16_t switcher_get_program(const switcher_t* sw)
{
    if (!sw) return 0;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return atem_client_get_program_input(&sw->backend.atem, 0);
        case SWITCHER_TYPE_VMIX:
            return vmix_client_get_program_input(&sw->backend.vmix);
        case SWITCHER_TYPE_OBS: {
            int16_t idx = obs_client_get_program_scene(&sw->backend.obs);
            return (idx >= 0) ? (uint16_t)(idx + 1) : 0;  /* 1-based */
        }
        default:
            return 0;
    }
}

uint16_t switcher_get_preview(const switcher_t* sw)
{
    if (!sw) return 0;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return atem_client_get_preview_input(&sw->backend.atem, 0);
        case SWITCHER_TYPE_VMIX:
            return vmix_client_get_preview_input(&sw->backend.vmix);
        case SWITCHER_TYPE_OBS: {
            int16_t idx = obs_client_get_preview_scene(&sw->backend.obs);
            return (idx >= 0) ? (uint16_t)(idx + 1) : 0;  /* 1-based */
        }
        default:
            return 0;
    }
}

/* ============================================================================
 * Tally 조회
 * ============================================================================ */

uint8_t switcher_get_tally(const switcher_t* sw, uint8_t index)
{
    if (!sw) return 0;
    SWITCHER_DISPATCH_UINT8(sw, get_tally_by_index, index);
}

uint64_t switcher_get_tally_packed(const switcher_t* sw)
{
    if (!sw) return 0;
    SWITCHER_DISPATCH_UINT64(sw, get_tally_packed);
}

void switcher_tally_unpack(const switcher_t* sw,
                           uint8_t* pgm, uint8_t* pgm_count,
                           uint8_t* pvw, uint8_t* pvw_count)
{
    if (pgm_count) *pgm_count = 0;
    if (pvw_count) *pvw_count = 0;
    if (!sw) return;

    /* 카메라 수 가져오기 */
    switcher_info_t info;
    uint8_t num_cameras = 8;  /* 기본값 */
    if (switcher_get_info(sw, &info) == SWITCHER_OK && info.num_cameras > 0) {
        num_cameras = info.num_cameras;
    }
    if (num_cameras > 20) num_cameras = 20;

    uint64_t packed = switcher_get_tally_packed(sw);
    uint8_t pi = 0, vi = 0;

    for (uint8_t i = 0; i < num_cameras; i++) {
        uint8_t tally = (packed >> (i * 2)) & 0x03;

        if (tally == SWITCHER_TALLY_PROGRAM || tally == SWITCHER_TALLY_BOTH) {
            if (pgm) pgm[pi] = i + 1;  /* 채널 번호만 저장 */
            pi++;
        }
        if (tally == SWITCHER_TALLY_PREVIEW || tally == SWITCHER_TALLY_BOTH) {
            if (pvw) pvw[vi] = i + 1;  /* 채널 번호만 저장 */
            vi++;
        }
    }

    if (pgm_count) *pgm_count = pi;
    if (pvw_count) *pvw_count = vi;
}

char* switcher_tally_format(const switcher_t* sw, char* buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return buf;

    uint8_t pgm[20], pvw[20];
    uint8_t pgm_count = 0, pvw_count = 0;
    switcher_tally_unpack(sw, pgm, &pgm_count, pvw, &pvw_count);

    char pgm_str[48] = "--";
    char pvw_str[48] = "--";

    if (pgm_count > 0) {
        char* p = pgm_str;
        for (uint8_t i = 0; i < pgm_count && i < 20; i++) {
            if (i > 0) *p++ = ',';
            p += snprintf(p, pgm_str + sizeof(pgm_str) - p, "%d", pgm[i]);
        }
    }

    if (pvw_count > 0) {
        char* p = pvw_str;
        for (uint8_t i = 0; i < pvw_count && i < 20; i++) {
            if (i > 0) *p++ = ',';
            p += snprintf(p, pvw_str + sizeof(pvw_str) - p, "%d", pvw[i]);
        }
    }

    snprintf(buf, buf_size, "PGM: %s / PVW: %s", pgm_str, pvw_str);
    return buf;
}

/* ============================================================================
 * 제어 명령
 * ============================================================================ */

int switcher_cut(switcher_t* sw)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return (atem_client_cut(&sw->backend.atem, 0) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        case SWITCHER_TYPE_VMIX:
            return (vmix_client_cut(&sw->backend.vmix) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        case SWITCHER_TYPE_OBS:
            /* OBS는 Cut 기능 없음 - Preview를 Program으로 전환 */
            if (obs_client_is_studio_mode(&sw->backend.obs)) {
                int16_t pvw = obs_client_get_preview_scene(&sw->backend.obs);
                if (pvw >= 0) {
                    return (obs_client_set_program_scene(&sw->backend.obs, (uint8_t)pvw) == 0)
                           ? SWITCHER_OK : SWITCHER_ERROR;
                }
            }
            return SWITCHER_ERROR_NOT_SUPPORTED;
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

int switcher_auto(switcher_t* sw)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return (atem_client_auto(&sw->backend.atem, 0) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        case SWITCHER_TYPE_VMIX:
            return (vmix_client_fade(&sw->backend.vmix) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        case SWITCHER_TYPE_OBS:
            /* OBS는 Auto/Fade 기능 없음 - Cut과 동일하게 처리 */
            return switcher_cut(sw);
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

int switcher_set_program(switcher_t* sw, uint16_t input)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return (atem_client_set_program_input(&sw->backend.atem, input, 0) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        case SWITCHER_TYPE_VMIX:
            return (vmix_client_set_program_input(&sw->backend.vmix, input) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        case SWITCHER_TYPE_OBS:
            /* OBS는 0-based index 사용, input은 1-based */
            if (input == 0) return SWITCHER_ERROR_INVALID_PARAM;
            return (obs_client_set_program_scene(&sw->backend.obs, (uint8_t)(input - 1)) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

int switcher_set_preview(switcher_t* sw, uint16_t input)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return (atem_client_set_preview_input(&sw->backend.atem, input, 0) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        case SWITCHER_TYPE_VMIX:
            return (vmix_client_set_preview_input(&sw->backend.vmix, input) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        case SWITCHER_TYPE_OBS:
            /* OBS는 0-based index 사용, input은 1-based */
            if (input == 0) return SWITCHER_ERROR_INVALID_PARAM;
            return (obs_client_set_preview_scene(&sw->backend.obs, (uint8_t)(input - 1)) == 0)
                   ? SWITCHER_OK : SWITCHER_ERROR;
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

/* ============================================================================
 * 콜백 설정
 * ============================================================================ */

void switcher_set_callbacks(switcher_t* sw, const switcher_callbacks_t* callbacks)
{
    if (!sw || !callbacks) return;

    sw->callbacks = *callbacks;
}

/* ============================================================================
 * 디버그
 * ============================================================================ */

void switcher_set_debug(switcher_t* sw, bool enable)
{
    if (!sw) return;

    sw->debug = enable;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            atem_client_set_debug(&sw->backend.atem, enable);
            break;
        case SWITCHER_TYPE_VMIX:
            vmix_client_set_debug(&sw->backend.vmix, enable);
            break;
        case SWITCHER_TYPE_OBS:
            obs_client_set_debug(&sw->backend.obs, enable ? OBS_DEBUG_INFO : OBS_DEBUG_NONE);
            break;
        default:
            break;
    }
}

/* ============================================================================
 * 카메라 매핑 설정
 * ============================================================================ */

int switcher_set_camera_limit(switcher_t* sw, uint8_t limit)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            sw->backend.atem.state.user_camera_limit = limit;
            atem_state_update_camera_limit(&sw->backend.atem.state);
            return SWITCHER_OK;
        case SWITCHER_TYPE_VMIX:
            sw->backend.vmix.state.user_camera_limit = limit;
            vmix_state_update_camera_limit(&sw->backend.vmix.state);
            return SWITCHER_OK;
        case SWITCHER_TYPE_OBS:
            sw->backend.obs.state.user_camera_limit = limit;
            obs_state_update_camera_limit(&sw->backend.obs.state);
            return SWITCHER_OK;
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

int switcher_set_camera_offset(switcher_t* sw, uint8_t offset)
{
    if (!sw) return SWITCHER_ERROR_INVALID_PARAM;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            sw->backend.atem.state.camera_offset = offset;
            return SWITCHER_OK;
        case SWITCHER_TYPE_VMIX:
            sw->backend.vmix.state.camera_offset = offset;
            return SWITCHER_OK;
        case SWITCHER_TYPE_OBS:
            sw->backend.obs.state.camera_offset = offset;
            return SWITCHER_OK;
        default:
            return SWITCHER_ERROR_NOT_SUPPORTED;
    }
}

uint8_t switcher_get_camera_limit(const switcher_t* sw)
{
    if (!sw) return 0;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return sw->backend.atem.state.user_camera_limit;
        case SWITCHER_TYPE_VMIX:
            return sw->backend.vmix.state.user_camera_limit;
        case SWITCHER_TYPE_OBS:
            return sw->backend.obs.state.user_camera_limit;
        default:
            return 0;
    }
}

uint8_t switcher_get_camera_offset(const switcher_t* sw)
{
    if (!sw) return 0;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return sw->backend.atem.state.camera_offset;
        case SWITCHER_TYPE_VMIX:
            return sw->backend.vmix.state.camera_offset;
        case SWITCHER_TYPE_OBS:
            return sw->backend.obs.state.camera_offset;
        default:
            return 0;
    }
}

uint8_t switcher_get_effective_camera_count(const switcher_t* sw)
{
    if (!sw) return 0;

    switch (sw->type) {
        case SWITCHER_TYPE_ATEM:
            return sw->backend.atem.state.effective_camera_limit;
        case SWITCHER_TYPE_VMIX:
            return sw->backend.vmix.state.effective_camera_limit;
        case SWITCHER_TYPE_OBS:
            return sw->backend.obs.state.effective_camera_limit;
        default:
            return 0;
    }
}