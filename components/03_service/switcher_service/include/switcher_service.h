/**
 * @file SwitcherService.h
 * @brief Switcher 서비스 (Service Layer)
 *
 * 역할: 스위처 생명주기 관리 및 듀얼모드 Tally 결합
 * - Primary, Secondary 스위처 관리 (이름 기반)
 * - 각 스위처의 상태 변경 감지
 * - 듀얼모드 및 오프셋 설정 관리
 * - 결합된 Packed Tally 데이터 제공
 */

#ifndef SWITCHER_SERVICE_H
#define SWITCHER_SERVICE_H

#include "TallyTypes.h"
#include <memory>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 스위처 역할 (이름 기반)
// ============================================================================

/**
 * @brief 스위처 역할을 문자열로 변환
 * @param role 스위처 역할
 * @return 문자열 ("Primary" 또는 "Secondary")
 */
const char* switcher_role_to_string(switcher_role_t role);

// ============================================================================
// C 인터페이스 (extern "C" 래퍼)
// ============================================================================

/**
 * @brief SwitcherService 핸들 (불투명 포인터)
 */
typedef void* switcher_service_handle_t;

/**
 * @brief 서비스 생성
 * @return 서비스 핸들
 */
switcher_service_handle_t switcher_service_create(void);

/**
 * @brief 서비스 소멸
 * @param handle 서비스 핸들
 */
void switcher_service_destroy(switcher_service_handle_t handle);

/**
 * @brief 서비스 초기화
 * @param handle 서비스 핸들
 * @return 성공 여부
 */
bool switcher_service_initialize(switcher_service_handle_t handle);

/**
 * @brief ATEM 스위처 설정
 * @param handle 서비스 핸들
 * @param role 스위처 역할 (PRIMARY/SECONDARY)
 * @param name 스위처 이름
 * @param ip IP 주소
 * @param port 포트 번호 (0 = 기본값 9910)
 * @param camera_limit 카메라 제한 (0 = 자동)
 * @param network_interface 네트워크 인터페이스 (1=WiFi, 2=Ethernet, 0=자동)
 * @return 성공 여부
 */
bool switcher_service_set_atem(switcher_service_handle_t handle,
                                switcher_role_t role,
                                const char* name,
                                const char* ip,
                                uint16_t port,
                                uint8_t camera_limit,
                                tally_network_if_t network_interface);

/**
 * @brief 스위처 제거
 * @param handle 서비스 핸들
 * @param role 스위처 역할
 */
void switcher_service_remove_switcher(switcher_service_handle_t handle, switcher_role_t role);

/**
 * @brief 루프 처리 (모든 스위처의 loop 호출)
 * @param handle 서비스 핸들
 * @note 태스크 모드에서는 자동 호출되므로 사용 불필요
 */
void switcher_service_loop(switcher_service_handle_t handle);

/**
 * @brief 태스크 시작 (내부 태스크 생성)
 * @param handle 서비스 핸들
 * @return 성공 여부
 */
bool switcher_service_start(switcher_service_handle_t handle);

/**
 * @brief 태스크 정지
 * @param handle 서비스 핸들
 */
void switcher_service_stop(switcher_service_handle_t handle);

/**
 * @brief 태스크 실행 중 여부
 * @param handle 서비스 핸들
 * @return 실행 중이면 true
 */
bool switcher_service_is_running(switcher_service_handle_t handle);

/**
 * @brief 결합된 Tally 데이터 조회
 * @param handle 서비스 핸들
 * @return 결합된 Packed 데이터
 */
packed_data_t switcher_service_get_combined_tally(switcher_service_handle_t handle);

/**
 * @brief Packed 데이터 메모리 해제
 * @param packed Packed 데이터 포인터
 */
void switcher_service_free_packed_data(packed_data_t* packed);

/**
 * @brief 스위터 상태 조회
 * @param handle 서비스 핸들
 * @param role 스위처 역할
 * @return 스위처 상태
 */
switcher_status_t switcher_service_get_switcher_status(switcher_service_handle_t handle, switcher_role_t role);

/**
 * @brief 듀얼모드 설정
 * @param handle 서비스 핸들
 * @param enabled 듀얼모드 활성화 여부
 */
void switcher_service_set_dual_mode(switcher_service_handle_t handle, bool enabled);

/**
 * @brief Secondary 스위처 오프셋 설정
 * @param handle 서비스 핸들
 * @param offset 오프셋 값 (0~19)
 */
void switcher_service_set_secondary_offset(switcher_service_handle_t handle, uint8_t offset);

/**
 * @brief 듀얼모드 활성화 여부
 * @param handle 서비스 핸들
 * @return 듀얼모드 활성화되었으면 true
 */
bool switcher_service_is_dual_mode_enabled(switcher_service_handle_t handle);

/**
 * @brief Secondary 오프셋 값
 * @param handle 서비스 핸들
 * @return 오프셋 값
 */
uint8_t switcher_service_get_secondary_offset(switcher_service_handle_t handle);

/**
 * @brief Tally 변경 콜백 설정
 * @param handle 서비스 핸들
 * @param callback 콜백 함수
 */
void switcher_service_set_tally_callback(switcher_service_handle_t handle, tally_callback_t callback);

/**
 * @brief 연결 상태 변경 콜백 설정
 * @param handle 서비스 핸들
 * @param callback 콜백 함수
 */
void switcher_service_set_connection_callback(switcher_service_handle_t handle, connection_callback_t callback);

/**
 * @brief 모든 스위처 재연결 시도
 * @param handle 서비스 핸들
 */
void switcher_service_reconnect_all(switcher_service_handle_t handle);

/**
 * @brief 스위처 변경 콜백 설정
 * @param handle 서비스 핸들
 * @param callback 콜백 함수 (role 파라미터)
 */
void switcher_service_set_switcher_change_callback(switcher_service_handle_t handle,
                                                     void (*callback)(switcher_role_t role));

// ============================================================================
// LoRa 패킷 해석 (수신 측)
// ============================================================================

/**
 * @brief LoRa 패킷에서 Tally 데이터 추출
 * @param handle 서비스 핸들
 * @param packet 수신 패킷 ([Header][Data...])
 * @param length 패킷 길이
 * @param tally 출력: Packed Tally 데이터
 * @return 성공 여부
 */
bool switcher_service_parse_lora_packet(switcher_service_handle_t handle,
                                        const uint8_t* packet, size_t length,
                                        packed_data_t* tally);

/**
 * @brief 채널 Tally 상태 조회
 * @param tally Packed Tally 데이터
 * @param channel 채널 번호 (1~20)
 * @return Tally 상태 (0=OFF, 1=PGM, 2=PVW, 3=BOTH)
 */
uint8_t switcher_service_get_tally_state(const packed_data_t* tally, uint8_t channel);

/**
 * @brief PGM 채널 목록 추출
 * @param tally Packed Tally 데이터
 * @param pgm 출력: PGM 채널 배열
 * @param pgm_count 출력: PGM 채널 수
 * @param max_count 최대 배열 크기
 */
void switcher_service_get_pgm_channels(const packed_data_t* tally,
                                       uint8_t* pgm, uint8_t* pgm_count,
                                       uint8_t max_count);

/**
 * @brief PVW 채널 목록 추출
 * @param tally Packed Tally 데이터
 * @param pvw 출력: PVW 채널 배열
 * @param pvw_count 출력: PVW 채널 수
 * @param max_count 최대 배열 크기
 */
void switcher_service_get_pvw_channels(const packed_data_t* tally,
                                       uint8_t* pvw, uint8_t* pvw_count,
                                       uint8_t max_count);

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ 인터페이스
// ============================================================================

#ifdef __cplusplus

/**
 * @brief SwitcherService 클래스
 */
class SwitcherService {
public:
    using SwitcherChangeCallback = std::function<void(switcher_role_t role)>;

    SwitcherService();
    ~SwitcherService();

    /**
     * @brief 초기화
     */
    bool initialize();

    /**
     * @brief ATEM 스위처 설정
     * @param role 스위처 역할 (PRIMARY/SECONDARY)
     * @param name 스위처 이름
     * @param ip IP 주소
     * @param port 포트 번호
     * @param camera_limit 카메라 제한
     * @param network_interface 네트워크 인터페이스 (1=WiFi, 2=Ethernet, 0=자동)
     * @return 성공 여부
     */
    bool setAtem(switcher_role_t role, const char* name, const char* ip, uint16_t port, uint8_t camera_limit, tally_network_if_t network_interface = static_cast<tally_network_if_t>(0));

    /**
     * @brief 스위처 제거
     * @param role 스위처 역할
     */
    void removeSwitcher(switcher_role_t role);

    /**
     * @brief 루프 처리
     */
    void loop();

    /**
     * @brief 태스크 시작 (내부 태스크 생성)
     * @return 성공 여부
     */
    bool start();

    /**
     * @brief 태스크 정지
     */
    void stop();

    /**
     * @brief 태스크 실행 중 여부
     */
    bool isRunning() const { return task_running_; }

    /**
     * @brief 결합된 Tally 데이터 조회
     */
    packed_data_t getCombinedTally() const;

    /**
     * @brief Primary 스위처 상태 조회
     */
    switcher_status_t getPrimaryStatus() const;

    /**
     * @brief Secondary 스위처 상태 조회
     */
    switcher_status_t getSecondaryStatus() const;

    /**
     * @brief 듀얼모드 설정
     */
    void setDualMode(bool enabled);

    /**
     * @brief Secondary 오프셋 설정
     */
    void setSecondaryOffset(uint8_t offset);

    /**
     * @brief 듀얼모드 활성화 여부
     */
    bool isDualModeEnabled() const { return dual_mode_enabled_; }

    /**
     * @brief Secondary 오프셋 값
     */
    uint8_t getSecondaryOffset() const { return secondary_offset_; }

    /**
     * @brief Tally 변경 콜백 설정
     */
    void setTallyCallback(tally_callback_t callback);

    /**
     * @brief 연결 상태 변경 콜백 설정
     */
    void setConnectionCallback(connection_callback_t callback);

    /**
     * @brief 모든 스위처 재연결
     */
    void reconnectAll();

    /**
     * @brief 스위처 변경 콜백 설정
     */
    void setSwitcherChangeCallback(SwitcherChangeCallback callback);

private:
    struct SwitcherInfo {
        std::unique_ptr<ISwitcherPort> adapter;
        packed_data_t last_packed;
        bool has_changed;
        bool change_notified;       // 콜백 중복 방지 플래그
        uint32_t last_reconnect_attempt;

        SwitcherInfo() : adapter(nullptr), last_packed{nullptr, 0, 0}, has_changed(false), change_notified(false), last_reconnect_attempt(0) {}

        void cleanup() {
            adapter.reset();
            if (last_packed.data) {
                packed_data_cleanup(&last_packed);
                last_packed.data = nullptr;
                last_packed.data_size = 0;
                last_packed.channel_count = 0;
            }
            has_changed = false;
            change_notified = false;
        }
    };

    SwitcherInfo primary_;
    SwitcherInfo secondary_;
    bool dual_mode_enabled_;
    uint8_t secondary_offset_;
    tally_callback_t tally_callback_;
    connection_callback_t connection_callback_;
    SwitcherChangeCallback change_callback_;

    // ============================================================================
    // 태스크 관련
    // ============================================================================

    TaskHandle_t task_handle_;          ///< FreeRTOS 태스크 핸들
    bool task_running_;                 ///< 태스크 실행 중 플래그

    // ============================================================================
    // 콜백 플래그 (task 내부에서 로그 출력용)
    // ============================================================================

    mutable volatile bool tally_changed_;
    mutable volatile bool switcher_changed_;
    mutable switcher_role_t last_switcher_role_;

    // 결합된 Packed 데이터 캐시
    mutable packed_data_t combined_packed_;

    StaticTask_t task_buffer_;          ///< 정적 태스크 메모리
    StackType_t task_stack_[4096];      ///< 태스크 스택 (4KB)

    /**
     * @brief 태스크 함수 (정적)
     * @param param 파라미터 (this 포인터)
     */
    static void switcher_task(void* param);

    /**
     * @brief 태스크 메인 루프
     */
    void taskLoop();

    /**
     * @brief 스위처 정보 조회 (역할 기반)
     */
    SwitcherInfo* getSwitcherInfo(switcher_role_t role);
    const SwitcherInfo* getSwitcherInfo(switcher_role_t role) const;

    /**
     * @brief 스위처 변경 감지
     */
    void checkSwitcherChange(switcher_role_t role);

    /**
     * @brief 스위처 Tally 변경 콜백 핸들러
     */
    void onSwitcherTallyChange(switcher_role_t role);

    /**
     * @brief 듀얼모드 Packed 데이터 결합
     */
    packed_data_t combineDualModeTally() const;

    /**
     * @brief 스위처 상태 조회 (역할 기반, private)
     */
    switcher_status_t getSwitcherStatus(switcher_role_t role) const;
};

#endif // __cplusplus

#endif // SWITCHER_SERVICE_H
