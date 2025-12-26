# web_server 컴포넌트

Tally Node용 웹 서버 프레젠테이션 컴포넌트입니다.

## 구조

```
web_server/
├── CMakeLists.txt
├── include/
│   └── web_server.h          # extern "C" 인터페이스
├── src/
│   └── web_server.cpp        # HTTP/WebSocket 서버
└── static_embed/             # 임베디드 정적 파일 (.gitignore)
    ├── index_html.h
    ├── styles_css.h
    ├── app_js.h
    └── ...
```

## 사용법

```c
#include "web_server.h"

// 웹 서버 시작
web_server_init();

// Tally 상태 브로드캐스트
uint8_t channels[16] = { /* ... */ };
web_server_broadcast_tally(channels, 16);

// LoRa 상태 브로드캐스트
web_server_broadcast_lora(-45, 12, 123, 456);
```

## 개발

정적 파일 수정 시:
```bash
cd ../web
npm run deploy    # 빌드 + 임베드 + 복사
cd ../..
pio run          # ESP32 빌드
```

## 의존성

- `esp_http_server` - HTTP 서버
- `esp_event` - 이벤트 루프
- `network_service` - 네트워크 상태
- `event_bus` - 상태 구독
