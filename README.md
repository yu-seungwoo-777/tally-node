# Tally-Node

ESP32-S3 (EORA S3) 기반 Tally 시스템 송수신 장치 펌웨어 프로젝트입니다.

## 개요

- **장치**: EORA S3 (ESP32-S3)
- **환경**: TX (송신), RX (수신)
- **빌드 시스템**: PlatformIO

## 빌드 방법

### 1. 의존성 설치

```bash
# 가상환경 생성
python3 -m venv venv

# 가상환경 활성화
source venv/bin/activate

# PlatformIO 설치
pip install -U platformio
```

### 2. 빌드

```bash
# 가상환경 활성화
source venv/bin/activate

# TX 환경 빌드
platformio run --environment eora_s3_tx

# RX 환경 빌드
platformio run --environment eora_s3_rx
```

### 3. 업로드

```bash
# TX 환경 빌드 및 업로드
platformio run --environment eora_s3_tx -t upload

# RX 환경 빌드 및 업로드
platformio run --environment eora_s3_rx -t upload
```

### 4. 디바이스 모니터링

**참고**: `pio device monitor`는 사용할 수 없습니다.

파이썬 스크립트로 포트를 종료하고 재부팅 후 모니터링을 진행해야 합니다.

## 프로젝트 구조

```
tally-node/
├── components/          # 컴포넌트 (00_common ~ 05_hal)
├── src/                # 메인 소스
├── include/            # 헤더 파일
├── docs/               # 문서 (ARCHITECTURE.md 등)
└── platformio.ini      # PlatformIO 설정
```

## 문서

- [ARCHITECTURE.md](docs/ARCHITECTURE.md) - 아키텍처 문서
- [CLAUDE.md](CLAUDE.md) - Claude Code 작업 지침

## 라이선스

저작권 (c) 2025
