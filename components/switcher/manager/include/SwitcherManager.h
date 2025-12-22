/**
 * @file SwitcherManager.h
 * @brief 스위처 통합 관리 Manager
 *
 * Manager 역할:
 * - 최대 2대 스위처 관리 (Primary, Secondary)
 * - ConfigCore에서 설정 로드
 * - 각 스위처의 tally_packed 제공
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "ConfigCore.h"

// Switcher 라이브러리 (C)
extern "C" {
#include "switcher.h"
#include "switcher_config.h"
}

/**
 * @brief 스위처 통합 관리 Manager
 *
 * 설계 원칙:
 * - ConfigCore에서 설정 자동 로드
 * - 각 스위처 독립 관리
 * - tally_packed 제공 (CommunicationManager가 사용)
 */
class SwitcherManager {
public:
    /**
     * @brief 초기화
     *
     * ConfigCore에서 스위처 설정을 읽어 핸들만 생성합니다 (연결은 하지 않음).
     */
    static esp_err_t init();

    /**
     * @brief 스위처 연결 시작
     *
     * 비차단 방식으로 모든 스위처 연결을 시작합니다.
     * loop()에서 비동기로 연결이 진행됩니다.
     */
    static void startConnect();

    /**
     * @brief 주기적 업데이트 (Hot Path)
     *
     * 모든 활성화된 스위처의 loop를 실행합니다.
     */
    static void loop();

    /**
     * @brief 특정 스위처 연결 여부
     *
     * @param index 스위처 인덱스 (PRIMARY or SECONDARY)
     * @return 연결되어 있으면 true
     */
    static bool isConnected(switcher_index_t index);

    /**
     * @brief 특정 스위처 상태 가져오기
     *
     * @param index 스위처 인덱스
     * @param state 상태를 저장할 구조체 포인터
     * @return 성공 시 ESP_OK
     */
    static esp_err_t getState(switcher_index_t index, switcher_state_t* state);

    /**
     * @brief 특정 스위처 Tally Packed 가져오기
     *
     * @param index 스위처 인덱스
     * @return 패킹된 Tally 상태 (64비트)
     */
    static uint64_t getTallyPacked(switcher_index_t index);

    /**
     * @brief 특정 스위처 핸들 가져오기
     *
     * @param index 스위처 인덱스
     * @return 스위처 핸들 (NULL이면 없음)
     */
    static switcher_t* getHandle(switcher_index_t index);

    /**
     * @brief Cut 실행
     *
     * @param index 스위처 인덱스
     * @return 성공 시 ESP_OK
     */
    static esp_err_t cut(switcher_index_t index);

    /**
     * @brief Auto (트랜지션) 실행
     *
     * @param index 스위처 인덱스
     * @return 성공 시 ESP_OK
     */
    static esp_err_t autoTransition(switcher_index_t index);

    /**
     * @brief Program 입력 변경
     *
     * @param index 스위처 인덱스
     * @param input 소스 ID
     * @return 성공 시 ESP_OK
     */
    static esp_err_t setProgram(switcher_index_t index, uint16_t input);

    /**
     * @brief Preview 입력 변경
     *
     * @param index 스위처 인덱스
     * @param input 소스 ID
     * @return 성공 시 ESP_OK
     */
    static esp_err_t setPreview(switcher_index_t index, uint16_t input);

    /**
     * @brief 초기화 여부 확인
     */
    static bool isInitialized();

    /**
     * @brief 듀얼 모드 여부 확인
     *
     * @return Secondary 스위처가 활성화되어 있으면 true (듀얼 모드)
     */
    static bool isDualMode();

    /**
     * @brief 활성화된 스위처 개수
     *
     * @return 활성화된 스위처 개수 (0~2)
     */
    static uint8_t getActiveSwitcherCount();

    /**
     * @brief 모든 스위처 재시작
     *
     * 설정을 다시 로드하고 모든 스위처를 재시작합니다.
     * 기존 연결을 종료하고 새 설정으로 다시 연결합니다.
     *
     * @return 성공 시 ESP_OK
     */
    static esp_err_t restartAll();

    /**
     * @brief 스위처 연결 완료 콜백 타입
     *
     * 스위처가 연결되고 초기화가 완료되었을 때 호출됩니다.
     *
     * @param index 연결된 스위처 인덱스
     */
    typedef void (*SwitcherConnectedCallback)(switcher_index_t index);

    /**
     * @brief 스위처 연결 완료 콜백 설정
     *
     * @param callback 콜백 함수 (NULL로 설정하면 콜백 해제)
     */
    static void setConnectedCallback(SwitcherConnectedCallback callback);

    /**
     * @brief Tally Packed 변경 감지 및 재시작 체크
     *
     * 1시간 동안 tally packed 값이 변하지 않으면 스위처 연결이 끊어진 것으로 간주하고
     * 모든 스위처를 재시작합니다.
     */
    static void checkTallyPackedChangeAndRestart();

private:
    // 싱글톤 패턴
    SwitcherManager() = delete;
    ~SwitcherManager() = delete;
    SwitcherManager(const SwitcherManager&) = delete;
    SwitcherManager& operator=(const SwitcherManager&) = delete;

    // 연결 상태
    enum ConnectionState {
        STATE_DISCONNECTED,    // 연결 안됨
        STATE_CONNECTING,      // 연결 시도 중
        STATE_CONNECTED        // 연결됨
    };

    // 스위처 컨텍스트
    struct SwitcherContext {
        switcher_t* handle;        // Switcher 핸들
        ConfigSwitcher config;     // 설정
        bool initialized;          // 초기화 완료 여부
        ConnectionState conn_state;  // 연결 상태
        uint32_t connect_start_ms;   // 연결 시작 시간
        uint32_t last_reconnect_attempt_ms;  // 마지막 재연결 시도 시간
        bool was_connected;        // 이전 연결 상태
        bool topology_printed;     // 토폴로지 정보 출력 완료 여부

        // Tally Packed 모니터링
        uint64_t last_tally_packed;     // 마지막 tally packed 값
        uint32_t last_tally_update_ms;  // 마지막 tally 업데이트 시간
        bool tally_monitored;           // Tally 모니터링 시작 여부
    };

    static SwitcherContext s_switchers[SWITCHER_INDEX_MAX];
    static bool s_initialized;
    static SwitcherConnectedCallback s_connected_callback;  // 연결 완료 콜백

    // 재연결 관련 상수 (switcher_config.h에서 공통 관리)
    static constexpr uint32_t RECONNECT_INTERVAL_MS = SWITCHER_RECONNECT_INTERVAL_MS;
    static constexpr uint32_t RECONNECT_TIMEOUT_MS = 500;    // 재연결 타임아웃
};
