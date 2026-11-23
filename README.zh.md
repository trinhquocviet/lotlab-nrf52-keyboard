# nrf52-keyboard

[English](README.md) | [Tiếng Việt](README.vi.md) | **中文**

![Build Status](https://api.travis-ci.org/Lotlab/nrf52-keyboard.svg?branch=master)

## 概述

这是一个主要面向 Lotlab Lot60-BLE 的定制版本——基于 TMK 的 Nordic nRF52 系列蓝牙键盘固件（同时支持 **nRF52810** 与 **nRF52832**），使用 nRF5 SDK 15.3 作为底层硬件驱动，并使用 TMK 键盘库作为上层键盘功能实现。

原版及官方固件请参见 [Lotlab/nrf52-keyboard](https://github.com/Lotlab/nrf52-keyboard)。

## 功能亮点

- **蓝牙 / USB 双模切换**，支持 USB 全键无冲 (NKRO)、鼠标键、多媒体键（含 macOS F3–F6 功能键）与按键宏
- **多设备 BLE 切换** — 支持最多 3 台已配对 BLE 设备自由切换
- **自定义 UTF-8 蓝牙名称与分通道配置** — 带 `_<number>` 后缀的自定义名称
- **实时电量同步** — 实时 SAADC 采样、自动 BLE BAS 电量通知（同步 macOS 电池组件）及 HID 电量查询（`0x16`）
- **可自定义 ESC RGB 状态指示灯** — 支持分通道及连接/充电状态颜色指示
- **动态配列与宏配置**，全新刷写后自动加载出厂默认配列与 FDS 超时参数
- **低功耗与 nRF52810 深度优化** — 关闭灯光待机低至 ~200 uA（Lot60-BLE），启用 `DYNAMIC_TX_POWER`、`NRF_P0->IN` 单寄存器矩阵扫描并回收 2 KB RAM
- **存储与串口加固** — 增强型 FDS 写入队列（支持 RAM 降级模式）及 `NRF_UARTE0` EasyDMA + PPI 串口传输
- **免跳线烧录与构建工具链** — 5 层固件兼容性校验脚本、GCC 12+ 编译支持以及软件切换 DFU/ISP 模式
- **高度可扩展的驱动与事件系统** — 支持旋钮、WS2812、OLED 及模块化钩子（详见 `application/main/src/driver/`）

> 前往 **[Releases](https://github.com/trinhquocviet/lotlab-nrf52-keyboard/releases)** 页面下载此定制固件。

## 硬件支持

当前支持 **nRF52810** 和 **nRF52832** 两种主控芯片，支持的键盘列表见 `keyboard/` 目录。

## 目录结构

```text
.
├── application              # nRF52 的 Bootloader 与主程序固件
│   ├── main                 # nRF52 主程序
│   │   ├── src              # 源码
│   │   │   ├── ble          # 蓝牙低功耗 (BLE) 协议栈与服务
│   │   │   ├── config       # SDK 与硬件配置
│   │   │   ├── driver       # 外置设备驱动
│   │   │   ├── keyboard     # 键盘矩阵、电源管理、存储与事件逻辑
│   │   │   ├── protocol     # UART / HID 通信协议
│   │   │   └── tmk          # TMK 桥接层
│   │   └── project          # 编译工程与链接脚本
│   └── bootloader           # nRF52 安全 DFU Bootloader
├── docs                     # 项目文档与指南 (EN / VI / ZH)
├── keyboard                 # 各键盘硬件配置与默认配列（如 lot60-ble）
├── scripts                  # 编译、烧录及软件切换模式辅助脚本
├── SDK                      # Nordic nRF5 SDK 15.3 目录
├── template                 # nRF SDK Makefile 模板
├── tmk                      # TMK 核心源码
├── usb                      # CH554 USB 桥接与 CMSIS-DAP 固件代码
├── Dockerfile
├── README.md
└── ...
```

## 文档列表

- [开发指南](docs/zh/development.md) — 分支说明、工具链配置、编译与烧录
- [Lot60-BLE 硬件、模式切换与烧录指南](docs/zh/lot60-ble-guide.md)
- [通信协议文档](docs/zh/communicate.md)
- [外置驱动与模块化配置说明](docs/zh/driver.md)
- [键盘钩子 (Hooks) 说明](docs/zh/hook.md)
- [快速硬件移植指南](docs/zh/porting.md)
