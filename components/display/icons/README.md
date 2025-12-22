# 상태 아이콘 (Status Icons)

RX/TX 페이지에서 사용하는 상태 아이콘 모음입니다.

## 파일 구조

```
icons/
├── status_icons.h          # 아이콘 데이터 정의
├── status_icons.c          # 아이콘 데이터 구현 (SSD1306용)
├── icon_renderer.h         # U8g2 아이콘 렌더러 헤더
├── icon_renderer.c         # U8g2 아이콘 렌더러 구현
├── CMakeLists.txt          # 빌드 설정
└── README.md               # 이 파일
```

## 아이콘 종류

### 배터리 아이콘 (Battery Icons)
- `BATTERY_100` (100%)
- `BATTERY_75` (75%)
- `BATTERY_50` (50%)
- `BATTERY_25` (25%)
- `BATTERY_LOW` (저전력 - 10% 미만, 반전 표시)

### 안테나 신호 아이콘 (Signal Icons)
- `SIGNAL_STRONG` (강함: > -50dBm)
- `SIGNAL_MEDIUM` (중간: -50 ~ -70dBm)
- `SIGNAL_WEAK` (약함: -70 ~ -85dBm)
- `SIGNAL_NONE` (없음: < -85dBm)

## 사용법

### 1. 헤더 포함
```c
#include "icon_renderer.h"
```

### 2. 배터리 아이콘 그리기
```c
// U8g2 인스턴스에서 배터리 아이콘 그리기
u8g2_t* u8g2 = DisplayHelper_getU8g2();
icon_draw_battery_u8g2(u8g2, 85, 5, 6);  // 85%, x=5, y=6
```

### 3. 신호 강도 아이콘 그리기
```c
// RSSI 값으로 신호 아이콘 그리기
icon_draw_signal_u8g2(u8g2, -60, 107, 6);  // -60dBm, x=107, y=6
```

### 4. 직접 아이콘 그리기
```c
// 특정 아이콘 타입으로 직접 그리기
icon_draw_u8g2(u8g2, BATTERY_LOW, 5, 6);
icon_draw_u8g2(u8g2, SIGNAL_STRONG, 107, 6);
```

## 페이지별 아이콘 위치

### RX 페이지
- 배터리 아이콘: (5, 6)
- 신호 아이콘: (107, 6)

### TX 페이지
- 배터리 아이콘: (5, 6)
- 신호 아이콘: (107, 6) - 네트워크 상태에 따라 동적 변경

## 아이콘 제작
- 크기: 16x12 픽셀
- 형식: XBM (X BitMap)
- 제작 도구: 이미지에서 XBM으로 변환하는 온라인 도구 또는 GIMP

## 참고
- 모든 아이콘은 U8g2 라이브러리와 호환
- 저전력 배터리 아이콘은 자동으로 반전되어 표시
- 신호 아이콘은 RSSI 값에 따라 자동으로 적절한 아이콘 선택