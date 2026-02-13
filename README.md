# TALLY-NODE

ESP32-S3 based Wi-Fi & Ethernet Tally System for real-time communication with video switchers.

**Languages:** English | [한국어](README-ko.md) | [日本語](README-ja.md) | [简体中文](README-zh-cn.md) | [Español](README-es.md) | [Français](README-fr.md)

## Overview

TALLY-NODE is a DIY-based TallyLight system that significantly reduces production costs while maintaining professional-grade reliability. Designed for real-time communication with video switchers, it currently supports Blackmagic ATEM and vMix, with more switchers coming soon.

**Links:**
- Website: https://tally-node.com
- Purchase: https://tally-node.com/purchase
- TX UI Demo: https://demo.tally-node.com

## Features

### LoRa Wireless Communication
- **Long Range**: Tested up to 300m in urban environments (may vary by environment)
- **Low Power**: Consumes less power than standard WiFi, extending RX battery life
- **Frequency Bands**: 433MHz and 868MHz support (based on country regulations)
- **Stable Signal**: Chirp Spread Spectrum technology for reliable communication
- **Real-time**: Instant tally state transmission with zero delay

### Dual Mode Support
- Connect up to 2 switchers simultaneously (ATEM + vMix, vMix + vMix, etc.)
- Use WiFi and Ethernet simultaneously for flexible network configuration
- Channel mapping within 1-20 range

### Web-based Control
- Intuitive web interface for all TX settings
- Network configuration (WiFi AP, Ethernet DHCP/Static)
- Switcher connection settings (IP, Port, Protocol)
- RX device management (brightness, colors, camera numbers)
- Firmware update via web UI
- System logs and diagnostics

### RX Device Management
- Real-time battery level and signal quality monitoring
- LED brightness control (0-100 levels)
- Remote reboot capability
- Batch configuration for all RX devices

## Hardware

### TX (Transmitter)
- Connects to switchers via IP (WiFi/Ethernet)
- USB-C power supply & 18650 battery support
- 433MHz / 868MHz LoRa broadcast
- Web UI control interface
- Supports up to 20 RX devices

### RX (Receiver)
- Mounts on camera
- Receives wireless tally signals from TX
- RGB LED for Program (Red), Preview (Green), Off states
- USB-C charging & 18650 battery
- 6-8 hours battery life (tested)

## Specifications

| Item | TX | RX |
|------|----|----|
| Communication | LoRa Wireless | LoRa Wireless |
| Tested Range | Up to 300m urban | Up to 300m urban |
| Supported Switchers | ATEM, vMix | - |
| Supported Cameras | Up to 20 units | - |
| Power | 18650 Battery, USB-C | 18650 Battery, USB-C |
| Battery Life | Up to 8 hours | Up to 8 hours |
| Network | Ethernet/WiFi/AP | - |
| Configuration | Web UI | Button Control |
| Mounting | 1/4 inch screw | 1/4 inch screw |

## Compatible Switchers

| Switcher | Status |
|----------|--------|
| Blackmagic ATEM | Supported |
| vMix | Supported |
| OBS Studio | Planned |
| OSEE | Planned |

### Tested ATEM Models
- ATEM Television Studio Series
- ATEM Mini Series
- ATEM Constellation Series

## Quick Start

### TX Setup
1. Connect power via USB-C or install 18650 battery
2. Access Web UI: `192.168.4.1` (AP mode) or assigned Ethernet IP
3. Configure network settings (WiFi/Ethernet)
4. Set up switcher connection (IP, Port, Mode)
5. Configure broadcast frequency and SYNCWORD
6. Activate license key

### RX Setup
1. Install 18650 battery or connect USB-C
2. Long press front button to set camera ID (1-20)
3. Ensure frequency and SYNCWORD match TX

## License

A license code is required to activate TX devices. The license determines the maximum number of connectable RX devices. License keys have no expiration date.

## Demo

Try the TX Web UI demo: [https://demo.tally-node.com](https://demo.tally-node.com)

---

Made with ESP32-S3
