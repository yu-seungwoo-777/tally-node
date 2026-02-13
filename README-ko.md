# TALLY-NODE

ESP32-S3 기반 Wi-Fi & Ethernet 탈리 시스템 - 비디오 스위처와 실시간 통신

**언어:** [English](README.md) | 한국어 | [日本語](README-ja.md) | [简体中文](README-zh-cn.md) | [Español](README-es.md) | [Français](README-fr.md)

## 개요

TALLY-NODE는 제작 비용을 대폭 낮추면서도 전문급 안정성을 유지하는 DIY 기반 TallyLight 시스템입니다. 비디오 스위처와 실시간 통신을 위해 설계되었으며, 현재 Blackmagic ATEM과 vMix를 지원하며 더 많은 스위처 지원이 예정되어 있습니다.

**링크:**
- [웹사이트](https://tally-node.com)
- [제품 구매](https://tally-node.com/purchase)
- [TX UI 데모](https://demo.tally-node.com)

## 주요 기능

### LoRa 무선 통신
- **장거리 통신**: 도심 환경에서 300m까지 테스트 완료 (사용 환경에 따라 차이 있음)
- **저전력 소모**: 일반 WiFi보다 전력 소모가 적어 RX 배터리 수명 연장
- **주파수 대역**: 433MHz, 868MHz 지원 (국가별 규정에 따라 선택)
- **안정적인 신호**: Chirp Spread Spectrum 기술로 노이즈 환경에서도 안정적
- **실시간 전송**: 지연 없는 즉각적인 탈리 상태 전송

### 듀얼모드 지원
- 최대 2대의 스위처 동시 연결 (ATEM + vMix, vMix + vMix 등)
- WiFi와 Ethernet 동시 사용으로 유연한 네트워크 구성
- 1~20 범위 내 채널 매핑

### 웹 기반 컨트롤
- 직관적인 웹 인터페이스로 모든 TX 설정 관리
- 네트워크 구성 (WiFi AP, Ethernet DHCP/Static)
- 스위처 연결 설정 (IP, Port, 프로토콜)
- RX 기기 관리 (밝기, 색상, 카메라 번호)
- 웹 UI를 통한 펌웨어 업데이트
- 시스템 로그 및 진단

### RX 기기 관리
- 실시간 배터리 잔량 및 신호 품질 모니터링
- LED 밝기 조절 (0-100 단계)
- 원격 재부팅
- 모든 RX 기기 일괄 설정

## 하드웨어

### TX (송신기)
- IP 기반 스위처 연결 (WiFi/Ethernet)
- USB-C 전원 공급 & 18650 배터리 지원
- 433MHz / 868MHz LoRa 브로드캐스트
- 웹 UI 컨트롤 인터페이스
- 최대 20대 RX 기기 지원

### RX (수신기)
- 카메라에 장착
- TX로부터 무선 탈리 신호 수신
- RGB LED로 Program (빨강), Preview (녹색), Off 상태 표시
- USB-C 충전 & 18650 배터리
- 6~8시간 배터리 수명 (테스트 완료)

## 제원

| 항목 | TX | RX |
|------|----|----|
| 통신 방식 | LoRa 무선 | LoRa 무선 |
| 실측 거리 | 도심지 최대 300m | 도심지 최대 300m |
| 지원 스위처 | ATEM, vMix | - |
| 지원 카메라 수 | 최대 20대 | - |
| 전원 | 18650 배터리, USB-C | 18650 배터리, USB-C |
| 배터리 시간 | 최대 8시간 | 최대 8시간 |
| 네트워크 | 이더넷/WiFi/AP | - |
| 설정 방식 | 웹 UI | 버튼 조작 |
| 장착 방식 | 1/4 인치 나사 | 1/4 인치 나사 |

## 연결 가능한 스위처

| 스위처 | 상태 |
|--------|------|
| Blackmagic ATEM | 지원 |
| vMix | 지원 |
| OBS Studio | 예정 |
| OSEE | 예정 |

### 테스트 완료 ATEM 모델
- ATEM Television Studio Series
- ATEM Mini Series
- ATEM Constellation Series

## 빠른 시작

### TX 설정
1. USB-C로 전원 연결 또는 18650 배터리 장착
2. 웹 UI 접속: `192.168.4.1` (AP 모드) 또는 할당된 Ethernet IP
3. 네트워크 설정 구성 (WiFi/Ethernet)
4. 스위처 연결 설정 (IP, Port, 모드)
5. 브로드캐스트 주파수 및 SYNCWORD 설정
6. 라이센스 키 활성화

### RX 설정
1. 18650 배터리 장착 또는 USB-C 연결
2. 전면 버튼 길게 눌러 카메라 ID 설정 (1-20)
3. 주파수와 SYNCWORD가 TX와 일치하는지 확인

## 라이센스

TX 기기를 활성화하려면 라이센스 코드가 필요합니다. 라이센스에 따라 연결 가능한 RX 기기 수가 결정됩니다. 라이센스 키는 사용 기한이 없습니다.

## 데모

TX 웹 UI 데모 체험: [https://demo.tally-node.com](https://demo.tally-node.com)

---

ESP32-S3으로 제작됨
