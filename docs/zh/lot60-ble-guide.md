[English](../lot60-ble-guide.md) | [Tiếng Việt](../vi/lot60-ble-guide.md) | **中文**

# Lot60-BLE 硬件、模式切换与烧录指南

> **参考 PCB：** Rev.G / Rev.GX — 单颗 RGB LED — nRF52810 — 板载 CMSIS-DAP
> **Fork：** <https://github.com/trinhquocviet/lotlab-nrf52-keyboard>
> **上游 Wiki：** <https://wiki.lotlab.org/ble/lot60-ble.html>

---

## 1. 硬件概述

Lot60-BLE 是一款 5×14（60%）蓝牙/USB 双模机械键盘，每块 PCB 包含**两颗微控制器**：

| 芯片 | 作用 | 烧录工具 | 固件格式 |
|------|------|----------|----------|
| **nRF52810**（或 **nRF52832** 变体） | 主控芯片 — BLE、按键矩阵、LED | `pyocd` / `nrfutil` DFU | `.hex` 或 `.zip` |
| **CH554** | USB 接口芯片 — HID、UART 桥接、CMSIS-DAP 调试器 | `usbisp` (Linux) / WCH ISP Tool (Windows) | `.bin` |

> **重要提示：** 升级固件时，请务必同步更新 nRF52 与 CH554 固件。

### 默认快捷键与电源行为

- **开机 / 深度休眠唤醒：** `Space + U`
- **清空配对设备：** `Space + E`（Fork `v1.2.1.0+` 支持 `Fn + R`）
- **手动休眠：** `Fn + CapsLock`
- **切换 USB / BLE：** `Fn + Tab`
- **多设备切换（`v1.2.0.0+`）：** `Space + U + Q/W/E`（Fork `v1.2.1.0+` 支持 `Fn + T/Y/U`，最多 3 台 BLE 设备）
- **空闲超时：** `15 秒` 后进入慢速扫描，`600 秒` 后自动休眠

### 状态 LED 颜色（单颗 RGB LED — `rev_g`）

| 颜色 | 色值 | 含义 |
|------|------|------|
| 白色 | `#FFFFFF` | 无连接（空闲） |
| 青色 | `#66FFFF` | BLE 已连接 |
| 天蓝色 | `#0099FF` | USB 已连接 |
| 橙色 | `#FF8000` | 充电中 |
| 绿色 | `#00FF00` | 充电完毕 |
| 黄色 | `#FFFF00` | 需要输入配对密码 |
| 紫红色 | `#FF0080` | 配对密码已发送 |
| 紫色 | `#FF00FF` | 休眠中 |

---

## 2. PCB 版本与固件兼容性

### 版本矩阵

构建系统会生成 **8 种版本变体**的固件，不同版本的 GPIO 引脚映射、主控芯片和 LED 配置各异。**刷入错误版本可能导致键盘变砖。**

| 目录 | 主板 | 芯片 | LED 配置 | CMSIS-DAP | DFU 按键 |
|------|------|------|----------|-----------|----------|
| `rev_c` | Rev.C | nRF52810 | 1 颗 RGB LED | ✅ 有 | 无（Buttonless DFU） |
| `rev_e` | Rev.E | nRF52810 | 1 颗 RGB LED | ❌ 无 | 无（Buttonless DFU） |
| `rev_e_3led` | Rev.E | nRF52810 | 3 颗状态 LED | ❌ 无 | 无（Buttonless DFU） |
| `rev_f` | Rev.F | nRF52810 | 1 颗 RGB LED | ❌ 无 | 无（Buttonless DFU） |
| `rev_f_3led` | Rev.F | nRF52810 | 3 颗状态 LED | ❌ 无 | 无（Buttonless DFU） |
| **`rev_g`** | **Rev.G/GX** | **nRF52810** | **1 颗 RGB LED** | **✅ 有** | **有（RESET 引脚）** |
| `rev_g_3led` | Rev.G/GX | nRF52810 | 3 颗状态 LED | ✅ 有 | 有（RESET 引脚） |
| `rev_g_832` | Rev.G/GX | **nRF52832** | 1 颗 RGB LED | ✅ 有 | 有（RESET 引脚） |

### 刷错固件的后果

| 不匹配项 | 后果 |
|----------|------|
| 主板型号错误（如在 Rev.GX 上刷 `rev_e`） | GPIO 引脚错位 → 按键失灵、LED 异常、USB 不可用 |
| 芯片型号错误（在 nRF52810 上刷 `rev_g_832`） | 镜像超过 192 KB Flash → **无法启动，可能变砖** |
| LED 配置错误（单灯板刷 `rev_g_3led`） | 键盘可用，但状态灯颜色异常 |

### LED 配置详情

定义于 `keyboard/lot60-ble/common/common.mk`：
- 未设置 `THREE_LED_STATUS` → 启用 `ONE_RGB_LED = yes` 并编译 `user_evt.c`（单颗 RGB LED 驱动）
- 设置 `THREE_LED_STATUS = yes` → 定义 `THREE_LED_STATUS` 并使用 `3led_status.c`（3 颗独立 LED 驱动）

### 芯片存储限制

| 芯片 | Flash 大小 | 最大地址 |
|------|------------|----------|
| nRF52810 | 192 KB | `0x30000` |
| nRF52832 | 512 KB | `0x80000` |
| CH554 | 14 KB 代码区 | `0x3800` |

---

## 3. 构建系统与固件命名规范

### 工具链要求

| 工具 | 版本 | 用途 |
|------|------|------|
| `arm-none-eabi-gcc` | 7-2018-q2-update 或 GCC 12+ | 编译 nRF52 固件 |
| `sdcc` | >= 4.0.0 | 编译 CH554 固件 |
| `nrfutil` | 最新版 | 生成 BLE DFU 升级包（`.zip`） |
| `mergehex` | Nordic CLI 工具 | 合并 SoftDevice 与主程序为 `.hex` |
| `make` | GNU Make | 构建调度 |
| nRF5 SDK | 15.3.0 | Nordic SDK（置于 `SDK/` 目录） |

### 构建命令

```bash
# 仅构建单个版本（例如 Rev.G/GX 单颗 RGB LED）
./scripts/build_lot60ble.sh --revision rev_g

# 构建全部 8 个版本
./scripts/build_lot60ble.sh

# 使用 Docker 构建（无需本地安装工具链）
./scripts/build_lot60ble.sh --docker
```

### 固件文件命名规范

由 `keyboard/lot60-ble/Makefile` 生成：

```text
lot60-ble-{revision}-{chip}-{date}-{git_version}.{ext}
```

`rev_g` 输出示例：

```text
lot60-ble-rev_g-nrf52-2026_06_11-v1.2.0.0.hex   # pyocd 烧录（已合并 SoftDevice）
lot60-ble-rev_g-nrf52-2026_06_11-v1.2.0.0.zip   # BLE DFU 升级包
lot60-ble-rev_g-ch554-2026_06_11-v1.2.0.0.bin   # CH554 USB 固件
```

### GitHub Release 工作流

- 推送 `v*.*.*.*` 格式的 tag 会触发 `.github/workflows/Release.yml`。
- 自动构建全部 8 个版本并上传最多 24 个固件文件（`*.hex`、`*.zip`、`*.bin`）。

### Rev.G 关键配置宏（`keyboard/lot60-ble/common/config.rev_g.h`）

```c
#define PRODUCT "Lot60.G"
#define DEVICE_VER 0x0003
#define LED_CAPS 19
#define LED_RGB_R 24
#define LED_RGB_G 23
#define LED_RGB_B 22
#define UART_RXD 17
#define UART_TXD 18
// 矩阵：5 行 × 14 列
// row_pin_array    = { 25, 28, 29, 30, 26 }
// column_pin_array = { 16, 15, 14, 13, 12, 11, 27, 3, 4, 5, 6, 7, 8, 9 }
```

---

## 4. 软件切换模式（免跳线 / 免拆壳）

当键盘已装入外壳、无法触及 PCB 背面按键或跳线时，可通过 `scripts/` 中的脚本完全经由 USB 切换至各种调试与烧录模式。

脚本采用内联 PEP 723 依赖声明，可直接通过 [`uv`](https://docs.astral.sh/uv/) 运行：

```bash
# 如未安装 uv，可执行以下命令安装
curl -LsSf https://astral.sh/uv/install.sh | sh
```

### 4.1 访问板载 CMSIS-DAP 调试器

- **机制：** CH554 运行标准固件接入 USB 时，会枚举为 USB 复合设备，同时提供 HID 键盘与 DAP-Link/CMSIS-DAP 接口。Rev.GX 无需切换模式或短接跳线。
- **验证连接：**
  ```bash
  uvx pyocd list
  ```
  输出列表中应显示 `CMSIS-DAP` 调试器。

### 4.2 进入 nRF52 BLE DFU 模式（`scripts/enter_dfu.py`）

- **机制：** nRF5 SDK Bootloader 启动时会检查 `GPREGRET` 寄存器（`0x4000051C`）。通过 CMSIS-DAP 写入 `0xB1`（`BOOTLOADER_DFU_START`）并触发软复位，即可直接进入 BLE DFU 模式（`DFUTarg`），无需按 `Space + B` 或物理复位键。
- **使用方法：**
  ```bash
  uv run scripts/enter_dfu.py
  ```
  在手机端 nRF Connect 中搜索 `DFUTarg`，或使用 `nrfutil` 刷写 `.zip` 升级包。

### 4.3 进入 CH554 USB ISP 模式（`scripts/enter_isp.py`）

- **机制：** 在 `usb/main.c` 中，CH554 固件在 Endpoint 3 上监听第 2 字节为 `0xF0`、第 3 字节为 `0x00` 的魔法数据包。收到后自动关闭看门狗、断开 USB 并跳转至 `0x3800`（`isp_enter()`，CH554 内置 Bootloader），无需按住背面 K1 按键。
- **使用方法：**
  ```bash
  uv run scripts/enter_isp.py
  ```
  键盘会断开并重新枚举为 CH554 ISP 设备，随后即可使用 `usbisp` 烧录 `.bin` 固件。烧录完成后会自动重启恢复正常模式。

### 4.4 备选方案：持久化虚拟环境（`.venv`）

如偏好传统的 Python 虚拟环境而非 `uv run`：

```bash
./scripts/setup_env.sh
source scripts/.venv/bin/activate
```

该脚本会从 `scripts/requirements.txt` 安装 `hidapi` 和 `pyocd`。

---

## 5. 烧录方式与兼容性检查

### 方式 1：`pyocd`（CMSIS-DAP / SWD）— Rev.G/GX 推荐

通过板载 CMSIS-DAP 调试器有线烧录。

```bash
# 标准烧录
./scripts/flash_lot60ble.sh -m pyocd -f lot60-ble-rev_g-nrf52-*.hex

# 全片擦除 + 烧录（用于降级或全新刷写）
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex --erase
```

**前置条件：**
- `pyocd`（`pip3 install pyocd` 或 `uvx pyocd`）
- macOS：`brew install libusb`
- Linux：配置 udev 规则（`sudo python3 -m pyocd.udev`）

### 方式 2：蓝牙 DFU（`nrfutil` / nRF Connect）

使用 `.zip` DFU 升级包通过 BLE 无线更新。

```bash
./scripts/flash_lot60ble.sh -m dfu -f lot60-ble-rev_g-nrf52-*.zip
```

**进入 DFU 模式**（任选其一）：
- 软件方式：`uv run scripts/enter_dfu.py`
- 键盘快捷键：`Fn + CapsLock`（休眠），再按 `Space + B`（DFU 唤醒）
- 硬件按键：按下 Rev.G/GX 上的 `RESET` 按钮

**手机端 DFU：** 打开 nRF Connect（iOS/Android）→ 扫描并连接 `DFUTarg` → 点击 DFU → 选择 `.zip` 文件。

### 方式 3：CH554 USB ISP（Linux / Windows）

使用 `.bin` 镜像更新 CH554 USB 桥接芯片。

```bash
./scripts/flash_lot60ble.sh -m usb -f lot60-ble-rev_g-ch554-*.bin
```

**进入 ISP 模式**（二选一）：
- 软件方式：`uv run scripts/enter_isp.py`
- 硬件方式：拔掉 USB → 按住背面 K1 按键 → 插入 USB → 识别后松开

**烧录工具：**
- Linux：[librech551](https://github.com/rgwan/librech551) 提供的 `usbisp`
- Windows：[WCH ISP Tool](http://www.wch.cn/downloads/WCHISPTool_Setup_exe.html)
- macOS：暂不原生支持（请使用 Linux 虚拟机）

### 5 层固件兼容性检查器

`scripts/flash_lot60ble.sh` 在烧录前会执行以下校验：
1. **文件后缀与方式匹配：** `.hex`（pyocd）、`.zip`（dfu）、`.bin`（usb）
2. **固件版本与 PCB 版本匹配：** 检查主板系列、芯片与 LED 配置是否一致
3. **芯片类型与烧录方式匹配：** 防止跨芯片误刷
4. **CMSIS-DAP 可用性检查：** 若目标 PCB 无板载调试器则发出警告
5. **二进制级校验：** 检查 Intel HEX 地址范围（阻止在 192 KB nRF52810 上刷入 512 KB nRF52832 固件）并校验 CH554 `.bin` 不超过 14 KB

```bash
# 默认 PCB 为 rev_g
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex

# 指定其他 PCB 版本
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex -p rev_g_3led

# 跳过所有检查（谨慎使用）
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex --skip-compat
```

### “免跳线”烧录速查流程

1. **nRF52 主固件（`.hex`）：** `./scripts/flash_lot60ble.sh -m pyocd -f <file>.hex`（CMSIS-DAP 始终在线）
2. **nRF52 BLE DFU（`.zip`）：** `uv run scripts/enter_dfu.py` → 通过 nRF Connect / `nrfutil` 刷写 `.zip`
3. **CH554 USB 固件（`.bin`）：** `uv run scripts/enter_isp.py` → 通过 `usbisp` 刷写 `.bin`（完成后自动重启）

---

## 6. 脚本参考与关键源码文件

### 脚本总览

| 脚本 | 用途 |
|------|------|
| `scripts/build_lot60ble.sh` | 从源码构建固件（单版本或全部版本） |
| `scripts/flash_lot60ble.sh` | 带 5 层兼容性校验的固件烧录脚本 |
| `scripts/enter_dfu.py` | 通过 CMSIS-DAP 使 nRF52 重启进入 BLE DFU 模式 |
| `scripts/enter_isp.py` | 通过 HID 魔法包使 CH554 进入 USB ISP 模式 |
| `scripts/setup_env.sh` | 创建包含 `hidapi` 与 `pyocd` 的持久化 `.venv` |

### 常用参数

**`build_lot60ble.sh`：** `-r, --revision <rev>` | `-D, --docker` | `-j, --jobs <N>` | `-o, --output <dir>`

**`flash_lot60ble.sh`：** `-m, --method <pyocd|dfu|usb>` | `-f, --file <path>` | `-p, --pcb <rev>`（默认：`rev_g`）| `-e, --erase` | `-d, --dfu-device <addr>` | `--skip-compat`

### 关键源码文件

| 文件 | 用途 |
|------|------|
| `keyboard/lot60-ble/Makefile` | 顶层构建调度与固件命名规则 |
| `keyboard/lot60-ble/common/common.mk` | 硬件版本、LED 与芯片选择逻辑 |
| `keyboard/lot60-ble/common/config.h` | 共享 USB ID、矩阵、BLE 与功耗配置 |
| `keyboard/lot60-ble/common/config.rev_g.h` | Rev.G/GX GPIO 引脚定义 |
| `keyboard/lot60-ble/common/user_evt.c` | 单颗 RGB LED 状态指示逻辑 |
| `keyboard/lot60-ble/rev_g/Makefile` | Rev.G 构建目标（`HARDWARE_VERSION = REV_G`） |
| `usb/main.c` / `usb/usb.mk` | CH554 USB 桥接、ISP 触发与构建配置 |
| `.github/workflows/Release.yml` | 多版本自动构建与 Release 发布流水线 |
