# Serial Monitor Guide

ESP32-S3 시리얼 모니터링 가이드

## Python 시리얼 모니터 (권장)

프로젝트에 포함된 Python 시리얼 모니터를 사용하면 **자동 리셋** 후 로그를 확인할 수 있습니다.

### 사용법

```bash
# 자동 포트 감지
python3 tools/serial_monitor.py

# 포트 지정
python3 tools/serial_monitor.py /dev/ttyACM0

# Windows
python tools/serial_monitor.py COM3
```

### 기능

- **자동 리셋**: DTR 신호로 ESP32 자동 재부팅
- **컬러 로그**: 로그 레벨별 색상 구분
  - 빨강: 에러 (E, fail, error)
  - 노랑: 경고 (W)
  - 초록: 정보 (I, ok)
  - 회색: 디버그 (D)
  - 마젠타: RF 관련
  - 시안: Tally 관련
- **실시간 출력**: 부팅부터 실행까지 모니터링

### 필수 패키지

```bash
pip3 install pyserial

# 또는
sudo apt install python3-serial
```

## 기본 사용법

### 1. 시리얼 포트 찾기

```bash
# Linux
ls /dev/ttyUSB* /dev/ttyACM*

# Windows (Device Manager에서 COM 포트 확인)

# macOS
ls /dev/tty.usb*
```

### 2. Claude Code에서 시리얼 모니터링

Claude Code는 별도의 시리얼 모니터 명령이 없으므로, Bash 툴을 사용합니다:

```bash
# 방법 1: cat (읽기 전용, 간단)
stty -F /dev/ttyACM0 115200 raw -echo
cat /dev/ttyACM0

# 방법 2: tail (실시간 출력)
tail -f /dev/ttyACM0
```

### 3. 권장 시리얼 모니터 도구

#### minicom (Linux/macOS)

```bash
# 설치
sudo apt install minicom

# 사용
minicom -D /dev/ttyACM0 -b 115200

# 종료: Ctrl+A, then Z, then Q
```

#### screen (Linux/macOS)

```bash
# 설치
sudo apt install screen

# 사용
screen /dev/ttyACM0 115200

# 종료: Ctrl+A, then K, then Y
```

#### picocom (간단함)

```bash
# 설치
sudo apt install picocom

# 사용
picocom -b 115200 /dev/ttyACM0

# 종료: Ctrl+A, then Ctrl+X
```

## ESP32-S3 특이사항

EoRa-S3 보드는 **Native USB CDC**를 사용하므로:
- 시리얼 포트: `/dev/ttyACM0` (Linux)
- 보드레이트: 상관없음 (USB이므로), 기본 921600로 설정됨
- 별도 USB-Serial 변환기 없이 직접 연결

## PlatformIO에서 시리얼 모니터

```bash
# device monitor 실행
pio device monitor -b 115200

# 또는
pio device monitor
```

## 유용한 팁

### 시리얼 로그 저장

```bash
# Python 모니터로 저장
python3 tools/serial_monitor.py > serial_log.txt

# 또는 cat으로
cat /dev/ttyACM0 > serial_log.txt

# 타임스탬프와 함께 저장
tail -f /dev/ttyACM0 | while read line; do echo "[$(date '+%H:%M:%S')] $line"; done > log.txt
```

### 필터링

```bash
# 특정 태그만 보기
tail -f /dev/ttyACM0 | grep --line-buffered "01_TxApp"

# 에러만 보기
tail -f /dev/ttyACM0 | grep --line-buffered -E "(E\]|fail|error)"
```

## 문제 해결

### 권한 거부

```bash
# dialout 그룹에 사용자 추가
sudo usermod -a -G dialout $USER
# 재로그인 필요
```

### 포트 사용 중

```bash
# 사용 중인 프로세스 확인
fuser -v /dev/ttyACM0

# 프로세스 종료
sudo kill -9 <PID>
```

### pyserial 설치 오류

```bash
# Ubuntu/Debian
sudo apt install python3-serial

# pip
pip3 install pyserial
```
