# TALLY-NODE

基于 ESP32-S3 的 Wi-Fi 和以太网 Tally 系统，用于与视频切换台进行实时通信。

**语言：** [English](README.md) | [한국어](README-ko.md) | [日本語](README-ja.md) | 简体中文 | [Español](README-es.md) | [Français](README-fr.md)

## 概述

TALLY-NODE 是一个基于 DIY 的 TallyLight 系统，在保持专业级可靠性的同时显著降低了制作成本。专为与视频切换台进行实时通信而设计，目前支持 Blackmagic ATEM 和 vMix，更多切换台即将推出。

**链接：**
- 官网: https://tally-node.com
- 购买: https://tally-node.com/purchase
- TX UI 演示: https://demo.tally-node.com

## 功能特性

### LoRa 无线通信
- **长距离**：在城市环境中测试距离可达 300 米（因环境而异）
- **低功耗**：功耗低于标准 WiFi，延长 RX 电池续航
- **频段支持**：支持 433MHz 和 868MHz（根据国家法规）
- **信号稳定**：使用线性调频扩频技术实现可靠通信
- **实时传输**：零延迟的 tally 状态传输

### 双模式支持
- 可同时连接多达 2 个切换台（ATEM + vMix、vMix + vMix 等）
- 同时使用 WiFi 和以太网实现灵活的网络配置
- 支持 1-20 范围内的通道映射

### 基于 Web 的控制
- 直观的 Web 界面用于所有 TX 设置
- 网络配置（WiFi AP、以太网 DHCP/静态 IP）
- 切换台连接设置（IP、端口、协议）
- RX 设备管理（亮度、颜色、摄像机编号）
- 通过 Web UI 进行固件更新
- 系统日志和诊断

### RX 设备管理
- 实时电池电量和信号质量监控
- LED 亮度控制（0-100 级）
- 远程重启功能
- 所有 RX 设备的批量配置

## 硬件

### TX（发射器）
- 通过 IP 连接切换台（WiFi/以太网）
- USB-C 供电和 18650 电池支持
- 433MHz / 868MHz LoRa 广播
- Web UI 控制界面
- 支持多达 20 个 RX 设备

### RX（接收器）
- 安装在摄像机上
- 从 TX 接收无线 tally 信号
- RGB LED 显示 Program（红色）、Preview（绿色）、关闭状态
- USB-C 充电和 18650 电池
- 6-8 小时电池续航（实测）

## 规格参数

| 项目 | TX | RX |
|------|----|----|
| 通信方式 | LoRa 无线 | LoRa 无线 |
| 测试距离 | 城市环境可达 300 米 | 城市环境可达 300 米 |
| 支持的切换台 | ATEM、vMix | - |
| 支持的摄像机 | 最多 20 台 | - |
| 供电 | 18650 电池、USB-C | 18650 电池、USB-C |
| 电池续航 | 最多 8 小时 | 最多 8 小时 |
| 网络 | 以太网/WiFi/AP | - |
| 配置方式 | Web UI | 按键控制 |
| 安装方式 | 1/4 英寸螺口 | 1/4 英寸螺口 |

## 兼容的切换台

| 切换台 | 状态 |
|----------|--------|
| Blackmagic ATEM | 支持 |
| vMix | 支持 |
| OBS Studio | 计划中 |
| OSEE | 计划中 |

### 已测试的 ATEM 型号
- ATEM Television Studio 系列
- ATEM Mini 系列
- ATEM Constellation 系列

## 快速开始

### TX 设置
1. 通过 USB-C 连接电源或安装 18650 电池
2. 访问 Web UI：`192.168.4.1`（AP 模式）或分配的以太网 IP
3. 配置网络设置（WiFi/以太网）
4. 设置切换台连接（IP、端口、模式）
5. 配置广播频率和 SYNCWORD
6. 激活许可证密钥

### RX 设置
1. 安装 18650 电池或连接 USB-C
2. 长按正面按钮设置摄像机 ID（1-20）
3. 确保频率和 SYNCWORD 与 TX 匹配

## 许可证

TX 设备激活需要许可证代码。许可证决定了可连接的 RX 设备的最大数量。许可证密钥没有有效期。

## 演示

尝试 TX Web UI 演示：[https://demo.tally-node.com](https://demo.tally-node.com)

---

采用 ESP32-S3 制造
