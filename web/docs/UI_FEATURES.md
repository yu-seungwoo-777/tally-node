# Tally Web UI 기능 명세

## 1. 대시보드

### 1.1 Tally 채널 그리드
- 16개 채널 버튼 (4x4 또는 8x2 레이아웃)
- 상태별 색상:
  - `btn-error animate-pulse`: Live (빨간색 + 깜빡임)
  - `btn-success`: Preview (초록색)
  - `btn-neutral`: Off (중립색)
- 버튼 클릭으로 채널 토글
- All Off 버튼: 전체 채널 끄기

### 1.2 LoRa 상태 카드
| 항목 | 표시 |
|------|------|
| RSSI | dBm 단위 (예: -45 dBm) |
| SNR | dB 단위 (예: 12) |
| TX | 송신 패킷 수 |
| RX | 수신 패킷 수 |

### 1.3 네트워크 상태 카드
| 항목 | 표시 |
|------|------|
| IP | WiFi 또는 Ethernet IP |
| WiFi | 연결 상태 (성공/실패) |
| Mode | TX / RX |
| Signal | RSSI 프로그레스 바 |

### 1.4 스위처 상태 카드
| 항목 | 표시 |
|------|------|
| Primary | 타입 + 연결 상태 |
| Secondary | 타입 + 연결 상태 |

### 1.5 시스템 상태 카드
| 항목 | 표시 |
|------|------|
| Uptime | 가동 시간 (h m s) |
| Free Heap | 여유 메모리 (B/KB/MB) |
| Version | 펌웨어 버전 |

---

## 2. 설정

### 2.1 네트워크 설정

#### WiFi 설정
- SSID 입력
- 비밀번호 입력
- 검색 버튼: 사용 가능한 네트워크 스캔
- 저장 및 적용 버튼

#### Ethernet 설정
- IP 주소
- 게이트웨이
- 서브넷 마스크
- 저장 및 적용 버튼

#### AP 설정
- AP SSID
- AP 비밀번호
- 저장 버튼

### 2.2 Switcher 설정

#### 기본 설정
| 항목 | 설명 |
|------|------|
| 타입 | ATEM / vMix / OBS |
| IP 주소 | 스위처 IP |
| 포트 | vMix: 8099 (자동), OBS: 포트 입력, ATEM: 미사용 |
| ME | ATEM ME1 / ME2 / ME1&ME2 |

#### 옵션
- PGM과 PVW 중복 채널 제거 (체크박스)
- ATEM 카메라 입력 자동 감지 (체크박스)
- 연결 네트워크: WiFi / Ethernet 선택

### 2.3 시스템 설정

| 항목 | 설명 | 범위 |
|------|------|------|
| 밝기 | LED 밝기 | 1 ~ 255 |
| Max Count | 최대 카메라 수 | 1 ~ 16 |
| Sync Code | 무선 통신 코드 | 0 ~ 200 |
| LED 타임아웃 | 리시버 LED 자동 끄기 | 0 ~ 6 (0분 ~ 60분) |

---

## 3. 추후 추가 (예정)

### 3.1 디바이스 관리
- 연결된 디바이스 목록 (최대 40개)
- 자동 업데이트 (10초 간격)
- 상태 체크 버튼

### 3.2 라이선스
- 라이선스 키 인증
- 구매자 정보 조회 (이름, 전화번호, 이메일)
- 라이선스 서버 연결 테스트

---

## 4. API 설계 (예정)

### 4.1 REST API
| 엔드포인트 | 메서드 | 설명 |
|------------|--------|------|
| `/api/status` | GET | 전체 상태 조회 |
| `/api/tally` | GET | Tally 상태 조회 |
| `/api/toggle` | POST | 채널 토글 |
| `/api/config` | GET/POST | 설정 조회/저장 |
| `/api/reboot` | POST | 시스템 재부팅 |

### 4.2 WebSocket
| 메시지 타입 | 방향 | 설명 |
|-------------|------|------|
| `tally_update` | S→C | Tally 상태 변경 |
| `lora_status` | S→C | LoRa 상태 |
| `network_status` | S→C | 네트워크 상태 |
| `switcher_status` | S→C | 스위처 상태 |
| `system_status` | S→C | 시스템 상태 |
| `toggle_channel` | C→S | 채널 토글 |
| `all_off` | C→S | 전체 끄기 |

---

## 5. 기술 스택

| 구성 | 선택 |
|------|------|
| CSS 프레임워크 | DaisyUI + Tailwind CSS |
| JS 프레임워크 | Alpine.js v3 |
| 실시간 통신 | WebSocket API |
| HTTP 통신 | Fetch API |
| 아이콘 | Heroicons (SVG) |
| 파일 임베드 | C 헤더 변환 |
