/**
 * @file PageManager.c
 * @brief TALLY-NODE Page Manager Implementation
 *
 * 여러 디스플레이 페이지를 관리하고 전환하는 기능 구현
 */

#include "core/PageManager.h"
#include "core/DisplayManager.h"
#include "core/DisplayHelper.h"
#include "pages/BootScreen.h"
#include "pages/RxPage.h"
#include "pages/TxPage.h"
#include "pages/SettingsPage.h"
#include "log.h"
#include "log_tags.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "PAGE";

// 현재 페이지 상태
static PageType_t s_current_page = PAGE_TYPE_BOOT;
static bool s_rx_initialized = false;
static bool s_tx_initialized = false;
static bool s_settings_initialized = false;

// RX 전환 상태 (단일 버튼으로 RX1/RX2 번갈아 전환)
static bool s_rx1_active = true;  // 초기 상태: RX1 활성화



/**
 * @brief 현재 페이지 숨기기
 */
static void hideCurrentPage(void)
{
    switch (s_current_page) {
        case PAGE_TYPE_BOOT:
            // 부트 페이지는 숨기지 않음 (bootComplete에서 자동 전환)
            break;

        case PAGE_TYPE_RX:
            RxPage_hidePage();
            break;

  #ifdef DEVICE_MODE_TX
        case PAGE_TYPE_TX:
            TxPage_hidePage();
            break;
#endif

        case PAGE_TYPE_SETTINGS:
            SettingsPage_hidePage();
            break;

        default:
            break;
    }
}

/**
 * @brief 새로운 페이지 표시
 */
static esp_err_t showNewPage(PageType_t page_type)
{
    esp_err_t ret = ESP_OK;

    switch (page_type) {
        case PAGE_TYPE_BOOT:
            // 부트 페이지는 main에서 직접 관리
            break;

        case PAGE_TYPE_RX:
            if (!s_rx_initialized) {
                ret = RxPage_init();
                if (ret != ESP_OK) {
                    LOG_0(TAG, "RxPage init failed");
                    return ret;
                }
                s_rx_initialized = true;
            }
            RxPage_showPage();
            break;

        #ifdef DEVICE_MODE_TX
        case PAGE_TYPE_TX:
            if (!s_tx_initialized) {
                ret = TxPage_init();
                if (ret != ESP_OK) {
                    LOG_0(TAG, "TxPage init failed");
                    return ret;
                }
                s_tx_initialized = true;
            }
            TxPage_showPage();
            break;
#endif

        case PAGE_TYPE_SETTINGS:
            if (!s_settings_initialized) {
                ret = SettingsPage_init();
                if (ret != ESP_OK) {
                    LOG_0(TAG, "SettingsPage init failed");
                    return ret;
                }
                s_settings_initialized = true;
            }
            SettingsPage_showPage();
            break;

        default:
            ret = ESP_ERR_INVALID_ARG;
            break;
    }

    return ret;
}

// PageManager 함수들
esp_err_t PageManager_init(void)
{
    // DisplayManager가 초기화되었는지 확인
    u8g2_t* u8g2 = DisplayManager_getU8g2();
    if (!u8g2) {
        return ESP_FAIL;
    }

    s_current_page = PAGE_TYPE_BOOT;
    s_rx_initialized = false;
    s_tx_initialized = false;

    return ESP_OK;
}

esp_err_t PageManager_switchPage(PageType_t page_type)
{
    if (page_type == s_current_page) {
        return ESP_OK;
    }

    // 현재 페이지 숨기기
    hideCurrentPage();

    // 새로운 페이지 표시
    esp_err_t ret = showNewPage(page_type);
    if (ret == ESP_OK) {
        s_current_page = page_type;
    }

    return ret;
}

PageType_t PageManager_getCurrentPage(void)
{
    return s_current_page;
}

void PageManager_handleButton(int button_id)
{

    // 현재 페이지에 따른 버튼 동작
    switch (s_current_page) {
        case PAGE_TYPE_RX:
            // RX 페이지: 단일 버튼으로 RX1/RX2 번갈아 전환
            s_rx1_active = !s_rx1_active;  // 상태 반전

            if (s_rx1_active) {
                // RX1 활성화
                PageManager_setRx1(true);
                PageManager_setRx2(false);
            } else {
                // RX2 활성화
                PageManager_setRx1(false);
                PageManager_setRx2(true);
            }

            // 즉시 전송하여 전환 과정이 보이지 않도록 함
            DisplayHelper_sendBuffer();
            break;

        #ifdef DEVICE_MODE_TX
        case PAGE_TYPE_TX:
            // TX 페이지: 버튼으로 페이지 순환 (1→2→3→1)
            // 버튼은 button_actions.c에서 직접 처리하므로 여기서는 아무것도 안 함
            break;
#endif

        case PAGE_TYPE_SETTINGS:
            // 설정 페이지: 버튼 입력을 SettingsPage로 전달
            SettingsPage_handleButton(button_id);
            break;

        default:
            // 부트 페이지에서는 동작 없음
            break;
    }
}

void PageManager_update(void)
{
    // 주기적 업데이트 필요시 구현
    // 예: 애니메이션, 상태 표시 등

    // DisplayManager에서 display_changed 플래그 확인
    DisplaySystemInfo_t sys_info = DisplayManager_getSystemInfo();

    if (sys_info.display_changed) {
        // 플래그 초기화
        extern void DisplayManager_clearDisplayChangedFlag(void);
        DisplayManager_clearDisplayChangedFlag();

        LOG_1(TAG, "디스플레이 변경 감지 - 페이지 업데이트");
    }

    // 현재 페이지에 따른 업데이트
    switch (s_current_page) {
        case PAGE_TYPE_RX:
            // RX 페이지 업데이트 (최신 정보 표시)
            RxPage_showPage();
            s_rx_initialized = true;
            break;

        #ifdef DEVICE_MODE_TX
        case PAGE_TYPE_TX:
            // TX 페이지 업데이트 (최신 정보 표시)
            TxPage_showPage();
            s_tx_initialized = true;
            break;
#endif

        case PAGE_TYPE_SETTINGS:
            // 설정 페이지: 타이머가 독립적으로 동작하므로 주기적 업데이트 불필요
            break;

        default:
            // 다른 페이지의 업데이트가 필요하면 여기에 추가
            break;
    }
}

void PageManager_updateImmediate(void)
{
    // 즉시 업데이트 (플래그 없이 즉시 실행)
    // 페이지에 즉각 업데이트 알림
    switch (s_current_page) {
        case PAGE_TYPE_SETTINGS:
            // SettingsPage에 즉각 업데이트 필요 알림
            // SettingsPage 내부에서 처리하도록 플래그 설정
            break;

        case PAGE_TYPE_RX:
            // RX 페이지 즉각 업데이트
            RxPage_showPage();
            break;

        #ifdef DEVICE_MODE_TX
        case PAGE_TYPE_TX:
            // TX 페이지 즉각 업데이트
            TxPage_showPage();
            break;
#endif

        default:
            break;
    }
}

// 페이지별 제어 함수들
void PageManager_setRx1(bool active)
{
    RxPage_setRx1(active);
    // 즉시 버퍼 전송 (중복 호출 방지를 위해 RxPage에서 처리)
    DisplayHelper_sendBuffer();
}

void PageManager_setRx2(bool active)
{
    RxPage_setRx2(active);
    // 즉시 버퍼 전송 (중복 호출 방지를 위해 RxPage에서 처리)
    DisplayHelper_sendBuffer();
}

void PageManager_enterSettings(void)
{
    // 설정 페이지로 전환
    PageManager_switchPage(PAGE_TYPE_SETTINGS);

    LOG_1(TAG, "진입: 설정 페이지");
}

void PageManager_exitSettings(void)
{
#ifdef DEVICE_MODE_TX
    // TX 모드: TX 페이지 1로 복귀
    PageManager_switchPage(PAGE_TYPE_TX);
    // TxPage에 즉시 업데이트 알림
    extern void TxPage_forceUpdate(void);
    TxPage_forceUpdate();
#else
    // RX 모드: RX 페이지로 복귀
    PageManager_switchPage(PAGE_TYPE_RX);
#endif

    LOG_1(TAG, "퇴출: 설정 페이지 -> 메인 페이지");
}

// 롱프레스 처리 함수들
void PageManager_handleLongPress(int button_id)
{
    // 현재 페이지에 따른 롱프레스 동작
    switch (s_current_page) {
        case PAGE_TYPE_SETTINGS:
            // 설정 페이지: 롱프레스 입력을 SettingsPage로 전달
            SettingsPage_handleLongPress(button_id);
            break;

        default:
            // 다른 페이지에서는 롱프레스 처리하지 않음
            break;
    }
}

void PageManager_handleLongPressRelease(int button_id)
{
    // 현재 페이지에 따른 롱프레스 해제 동작
    switch (s_current_page) {
        case PAGE_TYPE_SETTINGS:
            // 설정 페이지: 롱프레스 해제를 SettingsPage로 전달
            SettingsPage_handleLongPressRelease(button_id);
            break;

        default:
            // 다른 페이지에서는 롱프레스 해제 처리하지 않음
            break;
    }
}

#ifdef DEVICE_MODE_TX
// SwitcherManager 및 ConfigCore 관련 함수 구현 (TX 전용)

// SwitcherManager 및 ConfigCore include
#include "SwitcherManager.h"
#include "ConfigCore.h"
#include "switcher_types.h"

// 스위처 타입을 문자열로 변환하는 헬퍼 함수
static const char* switcherTypeToString(switcher_type_t type)
{
    switch (type) {
        case SWITCHER_TYPE_ATEM:
            return "ATEM";
        case SWITCHER_TYPE_OBS:
            return "OBS";
        case SWITCHER_TYPE_VMIX:
            return "VMIX";
        case SWITCHER_TYPE_OSEE:
            return "OSEE";
        default:
            return "NONE";
    }
}

extern "C" {
bool PageManager_isSwitcherConnected(int index)
{
    if (index == PAGE_SWITCHER_PRIMARY) {
        return SwitcherManager::isConnected(SWITCHER_INDEX_PRIMARY);
    } else if (index == PAGE_SWITCHER_SECONDARY) {
        return SwitcherManager::isConnected(SWITCHER_INDEX_SECONDARY);
    }
    return false;
}

bool PageManager_getDualMode(void)
{
    return ConfigCore::getDualMode();
}

const char* PageManager_getSwitcherType(int index)
{
    static char type_buf[2][16];  // 버퍼 2개 (Primary, Secondary)
    if (index < 0 || index > 1) index = 0;

    ConfigSwitcher switcher = ConfigCore::getSwitcher((switcher_index_t)index);
    const char* result = switcherTypeToString(switcher.type);
    strncpy(type_buf[index], result, sizeof(type_buf[index]) - 1);
    type_buf[index][sizeof(type_buf[index]) - 1] = '\0';

    LOG_1(TAG, "PageManager_getSwitcherType(%d) = %s", index, type_buf[index]);
    return type_buf[index];
}

const char* PageManager_getSwitcherIp(int index)
{
    static char ip_buf[2][32];  // 버퍼 2개 (Primary, Secondary)
    if (index < 0 || index > 1) index = 0;

    ConfigSwitcher switcher = ConfigCore::getSwitcher((switcher_index_t)index);
    strncpy(ip_buf[index], switcher.ip, sizeof(ip_buf[index]) - 1);
    ip_buf[index][sizeof(ip_buf[index]) - 1] = '\0';

    LOG_1(TAG, "PageManager_getSwitcherIp(%d) = '%s' (len=%zu)", index, ip_buf[index], strlen(ip_buf[index]));
    return ip_buf[index];
}

uint16_t PageManager_getSwitcherPort(int index)
{
    ConfigSwitcher switcher = ConfigCore::getSwitcher((switcher_index_t)index);
    return switcher.port;
}

}
#endif