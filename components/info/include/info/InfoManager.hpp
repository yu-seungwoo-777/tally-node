/**
 * @file InfoManager.hpp
 * @brief InfoManager C++ 인터페이스
 *
 * 시스템 정보를 중앙에서 관리하는 매니저 클래스
 */

#pragma once

#include "info/info_types.h"
#include "info/result.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <memory>

#ifdef __cplusplus

namespace info {

/**
 * @brief 시스템 정보 중앙 관리자
 *
 * 장치 ID, 시스템 상태 등을 중앙에서 관리하고
 * Observer 패턴을 통해 변경을 알림
 */
class InfoManager {
public:
    /**
     * @brief 초기화
     * @return ESP_OK 성공, 그 외 실패
     * @note app_main()에서 다른 컴포넌트보다 먼저 호출해야 함
     */
    static esp_err_t init();

    /**
     * @brief 해제
     */
    static void deinit();

    /**
     * @brief 인스턴스 가져오기
     * @return 초기화된 인스턴스 포인터, 미초기화 시 nullptr
     */
    static InfoManager* get();

    // 복사/이동 금지
    InfoManager(const InfoManager&) = delete;
    InfoManager& operator=(const InfoManager&) = delete;
    InfoManager(InfoManager&&) = delete;
    InfoManager& operator=(InfoManager&&) = delete;

    /**
     * @brief 장치 ID 조회 (스레드 안전)
     * @return Result에 장치 ID가 담겨 있음
     */
    Result<std::string> getDeviceId() const;

    /**
     * @brief 장치 ID 설정 (스레드 안전)
     * @param device_id 새 장치 ID (최대 15자)
     * @return 결과
     */
    VoidResult setDeviceId(const std::string& device_id);

    /**
     * @brief MAC 주소 기반 장치 ID 자동 생성
     * @return 결과, 성공 시 생성된 ID는 getDeviceId()로 조회
     */
    VoidResult generateDeviceId();

    /**
     * @brief 시스템 정보 조회 (스레드 안전)
     * @return Result에 시스템 정보가 담겨 있음
     */
    Result<info_system_info_t> getSystemInfo() const;

    /**
     * @brief 시스템 정보 캐시 갱신 (스레드 안전)
     * @return 결과
     */
    VoidResult updateSystemInfo();

    /**
     * @brief Observer 등록 (스레드 안전)
     * @param callback 콜백 함수
     * @param ctx 사용자 컨텍스트
     * @return Result에 Observer 핸들이 담겨 있음
     */
    Result<info_observer_handle_t> addObserver(info_observer_fn_t callback, void* ctx);

    /**
     * @brief Observer 제거 (스레드 안전)
     * @param handle 등록 시 받은 핸들
     * @return 결과
     */
    VoidResult removeObserver(info_observer_handle_t handle);

    /**
     * @brief 패킷 송신 카운트 증가 (스레드 안전)
     * @return 결과
     */
    VoidResult incrementPacketTx();

    /**
     * @brief 패킷 수신 카운트 증가 (스레드 안전)
     * @return 결과
     */
    VoidResult incrementPacketRx();

    /**
     * @brief LoRa RSSI 설정 (스레드 안전)
     * @param rssi RSSI 값 (단위: 0.1dBm, 음수는 보정 필요)
     * @return 결과
     */
    VoidResult setLoraRssi(uint32_t rssi);

    /**
     * @brief LoRa SNR 설정 (스레드 안전)
     * @param snr SNR 값 (단위: 0.1dB)
     * @return 결과
     */
    VoidResult setLoraSnr(uint32_t snr);

    /**
     * @brief 에러 카운트 증가 (스레드 안전)
     * @return 결과
     */
    VoidResult incrementErrorCount();

    /**
     * @brief 모든 옵저버에게 변경 알림 (내부 사용)
     */
    void notifyObservers();

private:
    InfoManager();
    ~InfoManager();

    // 초기화 상태
    bool initialized_ = false;

    // NVS 관련
    esp_err_t loadFromNvs();
    esp_err_t saveToNvs();

    // MAC 주소에서 장치 ID 생성
    std::string generateDeviceIdFromMac();

    // 멤버 변수
    mutable std::mutex mutex_;
    std::string device_id_;
    info_system_info_t cached_info_{};
    bool dirty_ = false;  // NVS 저장 필요 플래그

    // Observer 저장소
    struct ObserverEntry {
        info_observer_handle_t handle;
        info_observer_fn_t callback;
        void* ctx;
        bool active;

        ObserverEntry() : handle(nullptr), callback(nullptr), ctx(nullptr), active(false) {}
    };
    std::vector<ObserverEntry> observers_;
    uint32_t next_observer_id_ = 1;  // 0은 유효하지 않은 핸들

    // 싱글톤 인스턴스 (init/deinit으로 관리)
    static InfoManager* instance_;
    static std::mutex init_mutex_;
};

} // namespace info

#endif // __cplusplus