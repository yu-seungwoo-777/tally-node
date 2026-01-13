#!/usr/bin/env python3
"""
ESP32 Serial Monitor with Auto-Reset

Usage:
    python3 tools/serial_monitor.py [port]

Default port: /dev/ttyACM0
"""

import sys
import time
import serial
import serial.tools.list_ports
import subprocess

# Colors for output
class Colors:
    RESET = '\033[0m'
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    GRAY = '\033[90m'

def release_port(port):
    """포트 사용 중인 프로세스 종료"""
    try:
        subprocess.run(['fuser', '-k', port], check=False, capture_output=True)
        time.sleep(0.2)  # 포트가 해제될 때까지 대기
    except FileNotFoundError:
        pass  # fuser가 없으면 무시

def find_esp_port():
    """자동으로 ESP32 포트 찾기"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if 'ACM' in port.device or 'USB' in port.device:
            return port.device
    return None

def reset_esp(ser):
    """DTR 신호로 ESP32 리셋"""
    print(f"{Colors.YELLOW}--- Resetting ESP32 (DTR toggle) ---{Colors.RESET}")
    ser.dtr = False
    time.sleep(0.1)
    ser.dtr = True
    time.sleep(0.1)
    ser.dtr = False
    time.sleep(0.5)

def colorize_line(line):
    """로그 태그에 따라 색상 지정"""
    if '[E]' in line or 'fail:' in line or 'error' in line.lower():
        return Colors.RED + line + Colors.RESET
    elif '[W]' in line:
        return Colors.YELLOW + line + Colors.RESET
    elif '[I]' in line or 'ok' in line:
        return Colors.GREEN + line + Colors.RESET
    elif '[D]' in line:
        return Colors.GRAY + line + Colors.RESET
    elif 'RF' in line:
        return Colors.MAGENTA + line + Colors.RESET
    elif 'Tally' in line:
        return Colors.CYAN + line + Colors.RESET
    return line

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else find_esp_port()
    if not port:
        port = '/dev/ttyACM0'

    print(f"{Colors.BLUE}=== ESP32 Serial Monitor ==={Colors.RESET}")
    print(f"{Colors.BLUE}Port: {port}{Colors.RESET}")
    print(f"{Colors.BLUE}Baud: 115200{Colors.RESET}")
    print(f"{Colors.CYAN}Press Ctrl+C to exit{Colors.RESET}\n")

    # 포트 사용 중인 프로세스 종료
    release_port(port)

    try:
        ser = serial.Serial(
            port=port,
            baudrate=115200,
            timeout=0.1,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE
        )

        # 리셋 수행
        reset_esp(ser)
        print(f"{Colors.GREEN}--- Monitoring started ---{Colors.RESET}\n")

        # 시리얼 읽기
        buffer = ""
        while True:
            try:
                data = ser.read(100)
                if data:
                    text = data.decode('utf-8', errors='ignore')
                    buffer += text

                    # 라인 단위로 출력
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        if line.strip():
                            colored = colorize_line(line)
                            print(colored)

            except KeyboardInterrupt:
                print(f"\n{Colors.YELLOW}--- Exit ---{Colors.RESET}")
                break

    except serial.SerialException as e:
        print(f"{Colors.RED}Serial error: {e}{Colors.RESET}")
        print(f"Hint: Check if the port exists: ls -l {port}")
        sys.exit(1)

if __name__ == '__main__':
    main()
