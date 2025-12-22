/**
 * ATEM 클라이언트 구현
 *
 * ATEM 스위처 제어를 위한 메인 클라이언트 API 구현
 */

#include "atem_client.h"
#include "atem_parser.h"
#include "atem_buffer.h"
#include "atem_debug.h"
#include "sw_platform.h"
#include "log.h"
#include "log_tags.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 내부 함수 선언
 * ============================================================================ */

static void create_hello_packet(uint8_t* buf);
static void create_ack_packet(uint8_t* buf, uint16_t session_id, uint16_t packet_id);
static void create_keepalive_packet(uint8_t* buf, atem_state_t* state);
static int send_packet(atem_client_t* client, const uint8_t* data, uint16_t length);
static int send_command(atem_client_t* client, const char* cmd, const uint8_t* data, uint16_t length);
static int process_packet(atem_client_t* client, const uint8_t* data, uint16_t length);
static void command_callback(const char* cmd_name, const uint8_t* cmd_data,
                             uint16_t cmd_length, void* user_data);

/* ============================================================================
 * 생성/소멸
 * ============================================================================ */

int atem_client_init(atem_client_t* client, const char* ip, uint16_t port)
{
    if (!client || !ip) {
        return -1;
    }

    /* 플랫폼 초기화 */
    if (sw_platform_init() < 0) {
        return -1;
    }

    memset(client, 0, sizeof(atem_client_t));

    /* IP 주소 복사 */
    strncpy(client->ip, ip, sizeof(client->ip) - 1);
    client->port = (port > 0) ? port : ATEM_DEFAULT_PORT;

    /* 상태 초기화 */
    atem_state_init(&client->state);

    /* 소켓은 connect() 시 생성 */
    client->socket = SW_INVALID_SOCKET;

    return 0;
}

void atem_client_cleanup(atem_client_t* client)
{
    if (!client) {
        return;
    }

    atem_client_disconnect(client);
    sw_platform_cleanup();
}

/* ============================================================================
 * 연결 관리
 * ============================================================================ */

int atem_client_connect(atem_client_t* client, uint32_t timeout_ms)
{
    if (!client) {
        return -1;
    }

    /* 이미 연결되어 있으면 종료 */
    if (client->state.connected) {
        atem_client_disconnect(client);
    }

    /* 소켓 생성 */
    client->socket = sw_socket_udp_create();
    if (client->socket == SW_INVALID_SOCKET) {
        return -1;
    }

    /* 로컬 포트 바인드 (자동 할당) */
    if (sw_socket_bind(client->socket, 0) < 0) {
        sw_socket_close(client->socket);
        client->socket = SW_INVALID_SOCKET;
        return -1;
    }

    /* 상태 초기화 */
    atem_state_init(&client->state);

    /* Hello 패킷 전송 (20바이트) - Arduino와 동일 */
    uint8_t hello[20];
    create_hello_packet(hello);

    LOG_0(TAG_ATEM, "Hello 패킷 전송");
    ATEM_DUMP_TX(hello, 20);

    if (send_packet(client, hello, 20) < 0) {
        sw_socket_close(client->socket);
        client->socket = SW_INVALID_SOCKET;
        return -1;
    }

    /* Hello 응답 대기 */
    uint32_t start = sw_platform_millis();
    while (sw_platform_millis() - start < timeout_ms) {
        int received = sw_socket_recvfrom(client->socket, client->rx_buffer,
                                            sizeof(client->rx_buffer), 100);

        if (received > 0) {
            ATEM_DUMP_RX(client->rx_buffer, received);
            ATEM_DUMP_HEADER(client->rx_buffer);

            /*
             * 패킷 파싱: Python과 동일
             * header_word = (flags << 11) | (length & 0x07FF)
             * flags = (header_word >> 11) & 0x1F
             */
            uint16_t header_word = atem_get_u16(client->rx_buffer, 0);
            uint8_t flags = (header_word >> 11) & 0x1F;

            if (flags & ATEM_FLAG_HELLO) {
                /*
                 * Hello 응답 파싱 (Python과 동일):
                 * - session_id: byte 2-3
                 * - packet_id: byte 10-11
                 */
                uint16_t session_id = atem_get_u16(client->rx_buffer, 2);
                uint16_t packet_id = atem_get_u16(client->rx_buffer, 10);

                LOG_0(TAG_ATEM, "Hello 응답: session=0x%04X, pkt=%d", session_id, packet_id);

                /* ACK 전송 (Python과 동일) */
                uint8_t ack[12];
                create_ack_packet(ack, session_id, packet_id);

                ATEM_DUMP_TX(ack, 12);

                send_packet(client, ack, 12);

                /* Session ID 저장 (Python과 동일: Hello 응답에서 바로 사용) */
                client->state.session_id = session_id;
                client->state.connected = true;
                client->state.last_contact_ms = sw_platform_millis();

                LOG_0(TAG_ATEM, "연결 성공");

                /* 연결 콜백 */
                if (client->on_connected) {
                    client->on_connected(client->user_data);
                }

                return 0;
            }
        }
    }

    LOG_0(TAG_ATEM, "연결 타임아웃");
    sw_socket_close(client->socket);
    client->socket = SW_INVALID_SOCKET;
    return -1;
}

int atem_client_connect_start(atem_client_t* client)
{
    if (!client) {
        return -1;
    }

    /* 기존 소켓 정리 (연결 여부와 관계없이) */
    if (client->socket != SW_INVALID_SOCKET) {
        sw_socket_close(client->socket);
        client->socket = SW_INVALID_SOCKET;
    }

    /* 소켓 생성 */
    client->socket = sw_socket_udp_create();
    if (client->socket == SW_INVALID_SOCKET) {
        return -1;
    }

    /* 로컬 포트 바인드 (자동 할당) */
    if (sw_socket_bind(client->socket, 0) < 0) {
        sw_socket_close(client->socket);
        client->socket = SW_INVALID_SOCKET;
        return -1;
    }

    /* 상태 초기화 */
    atem_state_init(&client->state);

    /* Hello 패킷 전송 (20바이트) */
    uint8_t hello[20];
    create_hello_packet(hello);

    LOG_0(TAG_ATEM, "Hello 패킷 전송 (논블로킹)");
    ATEM_DUMP_TX(hello, 20);

    if (send_packet(client, hello, 20) < 0) {
        sw_socket_close(client->socket);
        client->socket = SW_INVALID_SOCKET;
        return -1;
    }


    /* Hello 응답 대기는 check에서 수행 */
    return 1;  /* 진행 중 */
}

int atem_client_connect_check(atem_client_t* client)
{
    if (!client || client->socket == SW_INVALID_SOCKET) {
        return -1;
    }

    /* 이미 연결됨 */
    if (client->state.connected) {
        return 0;
    }

    /* Hello 응답 수신 시도 (논블로킹) */
    int received = sw_socket_recvfrom(client->socket, client->rx_buffer,
                                       sizeof(client->rx_buffer), 0);

    if (received > 0) {
        ATEM_DUMP_RX(client->rx_buffer, received);
        ATEM_DUMP_HEADER(client->rx_buffer);

        /* 패킷 파싱 */
        uint16_t header_word = ((uint16_t)client->rx_buffer[0] << 8) | client->rx_buffer[1];
        uint8_t flags = (header_word >> 11) & 0x1F;

        uint16_t session_id = ((uint16_t)client->rx_buffer[2] << 8) | client->rx_buffer[3];
        uint16_t packet_id = ((uint16_t)client->rx_buffer[10] << 8) | client->rx_buffer[11];

        /* Hello 응답 확인 (flags & 0x02 != 0) */
        if (flags & 0x02) {
            /* ACK 전송 */
            uint8_t ack[12];
            create_ack_packet(ack, session_id, packet_id);

            ATEM_DUMP_TX(ack, 12);

            send_packet(client, ack, 12);

            /* Session ID 저장 */
            client->state.session_id = session_id;
            client->state.connected = true;
            client->state.last_contact_ms = sw_platform_millis();

            LOG_0(TAG_ATEM, "연결 성공");

            /* 연결 콜백 */
            if (client->on_connected) {
                client->on_connected(client->user_data);
            }

            return 0;  /* 연결 완료 */
        }
    }

    /* 아직 응답 대기 중 */
    return 1;
}

void atem_client_disconnect(atem_client_t* client)
{
    if (!client) {
        return;
    }

    if (client->socket != SW_INVALID_SOCKET) {
        sw_socket_close(client->socket);
        client->socket = SW_INVALID_SOCKET;
    }

    bool was_connected = client->state.connected;
    client->state.connected = false;
    client->state.initialized = false;

    if (was_connected && client->on_disconnected) {
        client->on_disconnected(client->user_data);
    }

    LOG_0(TAG_ATEM, "연결 종료");
}

int atem_client_wait_init(atem_client_t* client, uint32_t timeout_ms)
{
    if (!client || !client->state.connected) {
        return -1;
    }

    uint32_t start = sw_platform_millis();
    while (sw_platform_millis() - start < timeout_ms) {
        atem_client_loop(client);

        if (client->state.initialized) {
            return 0;
        }

        if (!client->state.connected) {
            return -1;
        }

        /* 최소 delay - 다른 태스크에 양보만 */
        sw_platform_delay(1);
    }

    LOG_0(TAG_ATEM, "초기화 타임아웃");
    return -1;
}

bool atem_client_is_connected(const atem_client_t* client)
{
    return client && client->state.connected;
}

bool atem_client_is_initialized(const atem_client_t* client)
{
    return client && client->state.initialized;
}

/* ============================================================================
 * 메인 루프
 * ============================================================================ */

int atem_client_loop(atem_client_t* client)
{
    if (!client || !client->state.connected) {
        return -1;
    }

    int processed = 0;
    uint32_t now = sw_platform_millis();

    /* 패킷 수신 - UDP 버퍼가 빌 때까지 모두 처리 */
    while (1) {
        int received = sw_socket_recvfrom(client->socket, client->rx_buffer,
                                           sizeof(client->rx_buffer), 0);

        if (received > 0) {
            /* PyATEMMax 방식: 패킷 검증 성공 시에만 last_contact_ms 업데이트 */
            int result = process_packet(client, client->rx_buffer, (uint16_t)received);
            if (result == 0) {
                /* 유효한 패킷 처리 성공 - last_contact 업데이트 */
                client->state.last_contact_ms = sw_platform_millis();
                processed++;
            } else {
                /* result < 0: Session ID 불일치, 패킷 길이 부족 등 - last_contact 업데이트 안 함 */
                LOG_0(TAG_ATEM, "패킷 검증 실패 (size=%d) - last_contact 업데이트 안 함", received);
            }
        } else {
            /* 0: 더 이상 패킷 없음, <0: 에러 (무시) */
            break;
        }
    }

    /* 타임아웃 체크 */
    now = sw_platform_millis();
    if (now - client->state.last_contact_ms > ATEM_MAX_SILENCE_TIME_MS) {
        LOG_0(TAG_ATEM, "타임아웃 (무응답 %dms)", (int)(now - client->state.last_contact_ms));
        atem_client_disconnect(client);
        return -1;
    }

    /* Keepalive 전송 */
    if (client->state.initialized &&
        now - client->state.last_keepalive_ms > ATEM_KEEPALIVE_INTERVAL_MS) {
        uint8_t keepalive[12];
        create_keepalive_packet(keepalive, &client->state);
        send_packet(client, keepalive, sizeof(keepalive));
        client->state.last_keepalive_ms = now;
    }

    return processed;
}

/* ============================================================================
 * 기기 정보 조회
 * ============================================================================ */

bool atem_client_get_version(const atem_client_t* client, uint8_t* major, uint8_t* minor)
{
    if (!client) return false;
    if (major) *major = client->state.protocol_major;
    if (minor) *minor = client->state.protocol_minor;
    return client->state.protocol_major > 0;
}

const char* atem_client_get_product_name(const atem_client_t* client)
{
    return client ? client->state.product_name : NULL;
}

uint8_t atem_client_get_num_sources(const atem_client_t* client)
{
    return client ? client->state.num_sources : 0;
}

uint8_t atem_client_get_num_mes(const atem_client_t* client)
{
    return client ? client->state.num_mes : 0;
}

uint8_t atem_client_get_num_dsks(const atem_client_t* client)
{
    return client ? client->state.num_dsks : 0;
}

uint8_t atem_client_get_num_cameras(const atem_client_t* client)
{
    return client ? client->state.num_cameras : 0;
}

uint8_t atem_client_get_num_supersources(const atem_client_t* client)
{
    return client ? client->state.num_supersources : 0;
}

/* ============================================================================
 * Program/Preview 조회
 * ============================================================================ */

uint16_t atem_client_get_program_input(const atem_client_t* client, uint8_t me)
{
    if (!client || me >= ATEM_MAX_MES) return 0;
    return client->state.program_input[me];
}

uint16_t atem_client_get_preview_input(const atem_client_t* client, uint8_t me)
{
    if (!client || me >= ATEM_MAX_MES) return 0;
    return client->state.preview_input[me];
}

bool atem_client_is_program(const atem_client_t* client, uint16_t source_id, uint8_t me)
{
    return atem_client_get_program_input(client, me) == source_id;
}

bool atem_client_is_preview(const atem_client_t* client, uint16_t source_id, uint8_t me)
{
    return atem_client_get_preview_input(client, me) == source_id;
}

/* ============================================================================
 * Tally 조회
 * ============================================================================ */

uint8_t atem_client_get_tally_by_index(const atem_client_t* client, uint8_t index)
{
    if (!client || index >= ATEM_MAX_CHANNELS) return 0;
    return atem_tally_get(client->state.tally_packed, index);
}

uint64_t atem_client_get_tally_packed(const atem_client_t* client)
{
    if (!client) return 0;
    return client->state.tally_packed;
}

/* ============================================================================
 * Transition 조회
 * ============================================================================ */

uint8_t atem_client_get_transition_style(const atem_client_t* client, uint8_t me)
{
    if (!client || me >= ATEM_MAX_MES) return 0;
    return client->state.transition[me].style;
}

uint16_t atem_client_get_transition_position(const atem_client_t* client, uint8_t me)
{
    if (!client || me >= ATEM_MAX_MES) return 0;
    return client->state.transition[me].position;
}

bool atem_client_is_in_transition(const atem_client_t* client, uint8_t me)
{
    if (!client || me >= ATEM_MAX_MES) return false;
    return client->state.transition[me].in_transition;
}

bool atem_client_is_transition_preview_enabled(const atem_client_t* client, uint8_t me)
{
    if (!client || me >= ATEM_MAX_MES) return false;
    return client->state.transition[me].preview_enabled;
}

/* ============================================================================
 * Keyer 조회
 * ============================================================================ */

uint8_t atem_client_get_num_keyers(const atem_client_t* client, uint8_t me)
{
    if (!client || me >= ATEM_MAX_MES) return 0;
    return client->state.num_keyers[me];
}

bool atem_client_is_keyer_on_air(const atem_client_t* client, uint8_t me, uint8_t keyer_index)
{
    if (!client || me >= ATEM_MAX_MES || keyer_index >= ATEM_MAX_KEYERS) return false;
    uint8_t index = me * ATEM_MAX_KEYERS + keyer_index;
    return client->state.keyers[index].on_air;
}

bool atem_client_is_dsk_on_air(const atem_client_t* client, uint8_t dsk_index)
{
    if (!client || dsk_index >= ATEM_MAX_DSKS) return false;
    return client->state.dsks[dsk_index].on_air;
}

bool atem_client_is_dsk_in_transition(const atem_client_t* client, uint8_t dsk_index)
{
    if (!client || dsk_index >= ATEM_MAX_DSKS) return false;
    return client->state.dsks[dsk_index].in_transition;
}

uint16_t atem_client_get_supersource_fill(const atem_client_t* client)
{
    return client ? client->state.supersource_fill : 0;
}

uint16_t atem_client_get_supersource_key(const atem_client_t* client)
{
    return client ? client->state.supersource_key : 0;
}

/* ============================================================================
 * 제어 명령
 * ============================================================================ */

int atem_client_cut(atem_client_t* client, uint8_t me)
{
    if (!client || !client->state.initialized) return -1;

    uint8_t data[4] = { me, 0, 0, 0 };
    return send_command(client, ATEM_CMD_CUT, data, sizeof(data));
}

int atem_client_auto(atem_client_t* client, uint8_t me)
{
    if (!client || !client->state.initialized) return -1;

    uint8_t data[4] = { me, 0, 0, 0 };
    return send_command(client, ATEM_CMD_AUTO, data, sizeof(data));
}

int atem_client_set_program_input(atem_client_t* client, uint16_t source_id, uint8_t me)
{
    if (!client || !client->state.initialized) return -1;

    uint8_t data[4];
    data[0] = me;
    data[1] = 0;
    atem_set_u16(data, 2, source_id);

    return send_command(client, ATEM_CMD_CHANGE_PROGRAM, data, sizeof(data));
}

int atem_client_set_preview_input(atem_client_t* client, uint16_t source_id, uint8_t me)
{
    if (!client || !client->state.initialized) return -1;

    uint8_t data[4];
    data[0] = me;
    data[1] = 0;
    atem_set_u16(data, 2, source_id);

    return send_command(client, ATEM_CMD_CHANGE_PREVIEW, data, sizeof(data));
}

int atem_client_set_dsk_on_air(atem_client_t* client, uint8_t dsk_index, bool on_air)
{
    if (!client || !client->state.initialized) return -1;

    uint8_t data[4] = { dsk_index, on_air ? 1 : 0, 0, 0 };
    return send_command(client, ATEM_CMD_DSK_ON_AIR, data, sizeof(data));
}

int atem_client_dsk_auto(atem_client_t* client, uint8_t dsk_index)
{
    if (!client || !client->state.initialized) return -1;

    uint8_t data[4] = { dsk_index, 0, 0, 0 };
    return send_command(client, ATEM_CMD_DSK_AUTO, data, sizeof(data));
}

int atem_client_set_dsk_tie(atem_client_t* client, uint8_t dsk_index, bool tie)
{
    if (!client || !client->state.initialized) return -1;

    uint8_t data[4] = { dsk_index, tie ? 1 : 0, 0, 0 };
    return send_command(client, ATEM_CMD_DSK_TIE, data, sizeof(data));
}

int atem_client_set_keyer_on_air(atem_client_t* client, uint8_t me, uint8_t keyer_index, bool on_air)
{
    if (!client || !client->state.initialized) return -1;

    uint8_t data[4] = { me, keyer_index, on_air ? 1 : 0, 0 };
    return send_command(client, ATEM_CMD_USK_ON_AIR, data, sizeof(data));
}

bool atem_client_is_keyer_in_next(const atem_client_t* client, uint8_t me, uint8_t keyer_index)
{
    if (!client || me >= ATEM_MAX_MES || keyer_index >= ATEM_MAX_KEYERS) return false;
    return (client->state.transition[me].next_key & (1 << keyer_index)) != 0;
}

int atem_client_set_keyer_in_next(atem_client_t* client, uint8_t me, uint8_t keyer_index, bool in_next)
{
    if (!client || !client->state.initialized) return -1;
    if (me >= ATEM_MAX_MES || keyer_index >= ATEM_MAX_KEYERS) return -1;

    /*
     * CTTp 명령 구조 (4바이트):
     * - byte 0: 변경 마스크 (0x01 = style, 0x02 = next selection)
     * - byte 1: ME 인덱스
     * - byte 2: Transition style (변경 시)
     * - byte 3: Next selection 비트마스크
     *           bit 0 = Background
     *           bit 1 = Key 1
     *           bit 2 = Key 2
     *           bit 3 = Key 3
     *           bit 4 = Key 4
     */
    uint8_t current_next = client->state.transition[me].next_key;
    uint8_t current_bkgd = client->state.transition[me].next_background ? 1 : 0;
    uint8_t new_next;

    if (in_next) {
        new_next = current_next | (1 << keyer_index);
    } else {
        new_next = current_next & ~(1 << keyer_index);
    }

    /* Next selection: bit0=BKGD, bit1-4=Key1-4 */
    uint8_t next_selection = current_bkgd | (new_next << 1);

    uint8_t data[4] = { 0x02, me, 0, next_selection };
    return send_command(client, ATEM_CMD_TRANSITION_NEXT, data, sizeof(data));
}

bool atem_client_is_dsk_tie(const atem_client_t* client, uint8_t dsk_index)
{
    if (!client || dsk_index >= ATEM_MAX_DSKS) return false;
    return client->state.dsks[dsk_index].tie;
}

/* ============================================================================
 * 콜백 설정
 * ============================================================================ */

void atem_client_set_on_connected(atem_client_t* client, void (*callback)(void*), void* user_data)
{
    if (client) {
        client->on_connected = callback;
        client->user_data = user_data;
    }
}

void atem_client_set_on_disconnected(atem_client_t* client, void (*callback)(void*), void* user_data)
{
    if (client) {
        client->on_disconnected = callback;
        client->user_data = user_data;
    }
}

void atem_client_set_on_state_changed(atem_client_t* client, void (*callback)(const char*, void*), void* user_data)
{
    if (client) {
        client->on_state_changed = callback;
        client->user_data = user_data;
    }
}

/* ============================================================================
 * 디버그
 * ============================================================================ */

void atem_client_set_debug(atem_client_t* client, bool enable)
{
    if (client) {
        client->debug = enable;
        sw_set_debug(enable);
    }
}

/* ============================================================================
 * 내부 함수 구현
 * ============================================================================ */

static void create_hello_packet(uint8_t* buf)
{
    /*
     * ATEM Hello 패킷 (20바이트) - Python과 동일한 구조
     *
     * 헤더: (flags << 11) | (length & 0x07FF)
     * flags = 0x02 (HELLO_PACKET)
     * (0x02 << 11) | (20 & 0x07FF) = 0x1014
     */
    memset(buf, 0, 20);

    /* 헤더 워드 생성 (Python과 동일) */
    uint16_t header_word = (ATEM_FLAG_HELLO << 11) | (20 & 0x07FF);
    atem_set_u16(buf, 0, header_word);

    /* Session ID: 초기값 0 (byte 2-3) */
    atem_set_u16(buf, 2, 0x0000);

    /* ACK ID: 초기값 0 (byte 4-5) */
    atem_set_u16(buf, 4, 0x0000);

    /* 추가 플래그 (Python과 동일) */
    buf[9] = 0x3a;
    buf[12] = 0x01;
}

static void create_ack_packet(uint8_t* buf, uint16_t session_id, uint16_t packet_id)
{
    /*
     * ACK 패킷 (12바이트) - Python과 동일
     *
     * 헤더: (flags << 11) | (length & 0x07FF)
     * flags = 0x10 (ACK)
     * (0x10 << 11) | (12 & 0x07FF) = 0x800C
     */
    memset(buf, 0, 12);

    /* 헤더 워드 생성 (Python과 동일) */
    uint16_t header_word = (ATEM_FLAG_ACK << 11) | (12 & 0x07FF);
    atem_set_u16(buf, 0, header_word);

    /* Session ID (byte 2-3) */
    atem_set_u16(buf, 2, session_id);

    /* ACK ID (byte 4-5) - 확인할 패킷 ID */
    atem_set_u16(buf, 4, packet_id);
}

static void create_keepalive_packet(uint8_t* buf, atem_state_t* state)
{
    /*
     * Keepalive 패킷 (12바이트) - Python과 동일 (ACK 패킷 전송)
     *
     * Python에서는 keepalive로 ACK 패킷을 전송함
     */
    memset(buf, 0, 12);

    /* 헤더 워드 생성 (ACK 패킷과 동일) */
    uint16_t header_word = (ATEM_FLAG_ACK << 11) | (12 & 0x07FF);
    atem_set_u16(buf, 0, header_word);

    /* Session ID */
    atem_set_u16(buf, 2, state->session_id);

    /* ACK ID - 마지막 받은 패킷 ID */
    atem_set_u16(buf, 4, state->remote_packet_id);
}

static int send_packet(atem_client_t* client, const uint8_t* data, uint16_t length)
{
    return sw_socket_sendto(client->socket, client->ip, client->port, data, length);
}

static int send_command(atem_client_t* client, const char* cmd, const uint8_t* data, uint16_t length)
{
    /* 패킷 구성 */
    uint16_t cmd_length = ATEM_CMD_HEADER_LENGTH + length;
    uint16_t packet_length = ATEM_HEADER_LENGTH + cmd_length;

    if (packet_length > sizeof(client->tx_buffer)) {
        return -1;
    }

    memset(client->tx_buffer, 0, packet_length);

    /*
     * 헤더 구성: Python과 동일
     * header_word = (flags << 11) | (length & 0x07FF)
     * flags = ACK_REQUEST (0x01) - 응답 요청
     */
    uint16_t header_word = (ATEM_FLAG_ACK_REQUEST << 11) | (packet_length & 0x07FF);
    atem_set_u16(client->tx_buffer, 0, header_word);
    atem_set_u16(client->tx_buffer, 2, client->state.session_id);
    atem_set_u16(client->tx_buffer, 4, 0);  /* ACK ID */

    /* Packet ID 증가 */
    client->state.local_packet_id++;
    atem_set_u16(client->tx_buffer, 10, client->state.local_packet_id);

    /* 명령 헤더 */
    atem_set_u16(client->tx_buffer, 12, cmd_length);
    /* buf[14-15] = 0 */
    atem_set_command(client->tx_buffer, 16, cmd);

    /* 명령 데이터 */
    if (length > 0) {
        memcpy(client->tx_buffer + 20, data, length);
    }

    ATEM_DUMP_TX(client->tx_buffer, packet_length);

    return send_packet(client, client->tx_buffer, packet_length);
}

static int process_packet(atem_client_t* client, const uint8_t* data, uint16_t length)
{
    if (length < ATEM_HEADER_LENGTH) {
        return -1;  /* 패킷 길이 부족 */
    }

    /*
     * 헤더 파싱: Python과 동일
     *
     * 첫 2바이트: (flags << 11) | (length & 0x07FF)
     * flags = (header_word >> 11) & 0x1F
     * length = header_word & 0x07FF
     */
    uint16_t header_word = atem_get_u16(data, 0);
    uint8_t flags = (header_word >> 11) & 0x1F;
    uint16_t session_id = atem_get_u16(data, 2);
    uint16_t remote_packet_id = atem_get_u16(data, 10);
    (void)header_word;  /* packet_length는 현재 사용하지 않음 */

    /* Session ID 업데이트 (첫 번째 유효한 Session ID 저장) */
    if (client->state.session_id == 0 && session_id != 0) {
        client->state.session_id = session_id;
        LOG_0(TAG_ATEM, "Session ID 설정: 0x%04X", session_id);
    }

    /* Session ID 검증 (설정 후) */
    if (client->state.session_id != 0 && session_id != 0 &&
        session_id != client->state.session_id) {
        LOG_0(TAG_ATEM, "세션 ID 불일치: expected=0x%04X, got=0x%04X (패킷 거부)",
              client->state.session_id, session_id);
        return -1;  /* Session ID 불일치 - 패킷 거부 */
    }

    /*
     * 중복/재전송 패킷 체크
     * - 초기화 전: 모든 패킷 파싱 (InCm이 resend로 올 수 있음)
     * - 초기화 후: 중복/재전송은 ACK만 보내고 파싱 안 함
     */
    bool is_resend = (flags & ATEM_FLAG_RESEND) != 0;
    bool skip_parsing = false;

    if (client->state.initialized && remote_packet_id != 0) {
        if (remote_packet_id <= client->state.last_received_packet_id) {
            skip_parsing = true;
        } else {
            /* 새 패킷 - 즉시 ID 갱신 (다음 중복 체크를 위해) */
            client->state.last_received_packet_id = remote_packet_id;
            /* 초기화 후 resend는 파싱 안 함 */
            if (is_resend) {
                skip_parsing = true;
            }
        }
    }
    /* 초기화 전에는 resend도 파싱함 (InCm 때문) */

    /* ACK 필요 여부 확인 및 전송 */
    if ((flags & ATEM_FLAG_ACK_REQUEST) && client->state.session_id != 0) {
        uint8_t ack[12];
        create_ack_packet(ack, client->state.session_id, remote_packet_id);

        send_packet(client, ack, sizeof(ack));
    }

    /* 파싱 스킵 */
    if (skip_parsing) {
        return 0;  /* ACK는 보냈으므로 성공으로 처리 */
    }

    /* 패킷 ID 업데이트 (keepalive용) */
    if (remote_packet_id > client->state.remote_packet_id) {
        client->state.remote_packet_id = remote_packet_id;
    }

    /* 명령 추출 및 처리 */
    if (length > ATEM_HEADER_LENGTH) {
        atem_parse_commands(data, length, command_callback, client);
    }

    return 0;  /* 성공 */
}

static void command_callback(const char* cmd_name, const uint8_t* cmd_data,
                             uint16_t cmd_length, void* user_data)
{
    atem_client_t* client = (atem_client_t*)user_data;

    ATEM_DUMP_CMD(cmd_name, cmd_data, cmd_length);

    /* 상태 업데이트 (기본 정보 + InCm은 항상 처리) */
    atem_update_state(&client->state, cmd_name, cmd_data, cmd_length);

    /* 초기화 전에는 콜백 호출 안 함 (Tally 등 실시간 이벤트 무시) */
    if (!client->state.initialized) {
        return;
    }

    /* 상태 변경 콜백 */
    if (client->on_state_changed) {
        client->on_state_changed(cmd_name, client->user_data);
    }
}
