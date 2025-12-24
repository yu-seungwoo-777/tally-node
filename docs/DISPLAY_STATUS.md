# 디스플레이 구현 현황

**작성일**: 2025-12-25
**상태: 프레젠테이션 계층까지 구현 완료, 앱 연동 필요**

---

## 1. 구현 완료 항목

### 05_hal - 하드웨어 추상화 계층

| 컴포넌트 | 설명 | 상태 |
|---------|------|------|
| display_hal | I2C 핀 설정, HAL 초기화 | ✅ |
| u8g2_lib | U8g2 라이브러리 (3개 폰트) | ✅ |
| u8g2_hal | ESP-IDF용 U8g2 HAL 래퍼 | ✅ |

**파일 구조:**
```
components/05_hal/display_hal/
├── display_hal.c/h
├── u8g2_lib/csrc/*.c      # U8g2 라이브러리
└── u8g2_hal/
    ├── u8g2_esp32_hal.c/h
    └── include/u8g2_esp32_hal.h
```

---

### 04_driver - 드라이버 계층

| 컴포넌트 | 설명 | 상태 |
|---------|------|------|
| display_driver | C++ 래퍼, U8g2 초기화/제어 | ✅ |

**주요 API:**
```cpp
DisplayDriver_init();              // 초기화
DisplayDriver_clearBuffer();       // 버퍼 클리어
DisplayDriver_sendBuffer();        // 화면 전송
DisplayDriver_getU8g2();           // U8g2 인스턴스 가져오기
DisplayDriver_setPowerSave(0/1);   // 전원 제어
```

---

### 02_presentation - 프레젠테이션 계층

| 컴포넌트 | 설명 | 상태 |
|---------|------|------|
| DisplayManager | 페이지 전환, 렌더링 관리 | ✅ |
| BootPage | 부팅 화면, 진행률 표시 | ✅ |
| TallyPage | Tally 상태 표시 | ✅ |
| InfoPage | 시스템 정보 표시 | ✅ |

**파일 구조:**
```
components/02_presentation/display/
├── DisplayManager/
│   ├── DisplayManager.cpp/h
│   └── CMakeLists.txt
└── pages/
    ├── BootPage/
    │   ├── BootPage.cpp/h
    │   └── CMakeLists.txt
    ├── TallyPage/
    │   ├── TallyPage.cpp/h
    │   └── CMakeLists.txt
    └── InfoPage/
        ├── InfoPage.cpp/h
        └── CMakeLists.txt
```

**DisplayManager API:**
```cpp
// 초기화/시작
display_manager_init();
display_manager_start();

// 페이지 전환
display_manager_set_page(PAGE_BOOT);
display_manager_set_page(PAGE_TALLY);
display_manager_set_page(PAGE_INFO);

// 갱신 (주기적 호출 필요)
display_manager_update();

// 전원 제어
display_manager_set_power(true/false);
```

**BootPage API:**
```cpp
boot_page_init();                     // 페이지 등록
boot_page_set_message("메시지");     // 메시지 설정
boot_page_set_progress(50);           // 진행률 0-100%
```

**TallyPage API:**
```cpp
tally_page_init();                              // 페이지 등록
tally_page_set_state(1, TALLY_STATE_PGM);    // 채널, 상태
tally_page_set_program_name("NEWS");         // 프로그램명
tally_page_set_connection(true);              // 연결 상태
tally_page_set_channel(1);                    // 채널 번호
```

**InfoPage API:**
```cpp
info_page_init();                    // 페이지 등록
info_page_set_ip("192.168.1.100");  // IP 주소
info_page_set_battery(85);           // 배터리 %
info_page_set_rssi(-65);             // RSSI (dBm)
info_page_set_snr(8);                // SNR (dB)
info_page_set_connection(true);      // 연결 상태
info_page_set_uptime(3600);          // 업타임 (초)
```

---

## 2. 디스플레이 좌표계 참조

- **해상도**: 128 x 64 픽셀
- **원점**: (0, 0) = 왼쪽 상단
- **텍스트 y 좌표**: 베이스라인 기준 (글자 하단 기준선)
- **권장 x 좌표**: ≥ 4 (테두리와 겹침 방지)

**예시 (profont11_mf, 11px):**
```cpp
u8g2_SetFont(u8g2, u8g2_font_profont11_mf);
u8g2_DrawStr(u8g2, 4, 10, "첫번째 줄");   // y=10 베이스라인
u8g2_DrawStr(u8g2, 4, 21, "두번째 줄");  // y=21 베이스라인 (11px 간격)
u8g2_DrawStr(u8g2, 4, 32, "세번째 줄");  // y=32 베이스라인
u8g2_DrawFrame(u8g2, 0, 0, 128, 64);      // 전체 테두리
```

---

## 3. 남은 과정

### 3.1 타이머/갱신 루프 추가 (필수)

`display_manager_update()`를 주기적으로 호출해야 화면이 갱신됩니다.

**옵션 A: main.cpp 루프에서 호출**
```cpp
// main.cpp 메인 루프
while (1) {
    display_manager_update();  // 200ms마다 화면 갱신
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

**옵션 B: 전용 FreeRTOS 태스크**
```cpp
void display_task(void* arg) {
    while (1) {
        display_manager_update();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// 앱 초기화 시
xTaskCreate(display_task, "display", 4096, NULL, 5, NULL);
```

---

### 3.2 Tally 앱 연동 (필수)

**tally_rx_app**에서 Tally 수신 시 디스플레이 업데이트:

```cpp
// Tally 데이터 수신 콜백
void on_tally_received(uint8_t channel, tally_state_t state) {
    // 페이지 전환
    display_manager_set_page(PAGE_TALLY);

    // 상태 업데이트
    tally_page_set_state(channel, state);
    tally_page_set_connection(true);
}
```

---

### 3.3 웹 인터페이스 (선택)

```
02_presentation/web/
├── WebServer/
│   ├── WebServer.cpp/h
│   └── CMakeLists.txt
└── pages/
    ├── (HTML 템플릿)
    └── CMakeLists.txt
```

---

## 4. 테스트 코드

현재 `main.cpp`에서 `RUN_DISPLAY_TEST = 1`로 설정하여 테스트 가능:

```cpp
#define RUN_DISPLAY_TEST    1   // 디스플레이 테스트 (OLED)
```

**빌드 및 업로드:**
```bash
source venv/bin/activate
pio run -t upload
```

---

## 5. 빌드 결과

| 항목 | 값 |
|------|-----|
| Flash 사용 | 236 KB (7.5%) |
| RAM 사용 | 13 KB (4.1%) |
