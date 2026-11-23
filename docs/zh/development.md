[English](../development.md) | [Tiếng Việt](../vi/development.md) | **中文**

# 开发指南

本指南介绍编译工具链配置、固件与 Bootloader 编译、硬件烧录方式以及进阶开发文档。

---

## 分支说明

- `master`：主开发分支，可能包含正在进行的改动。
- `develop`：实验性分支，可能包含未完成的功能或编译问题。

若需日常稳定使用，建议前往 **[Releases](https://github.com/trinhquocviet/lotlab-nrf52-keyboard/releases)** 页面下载预编译的稳定版本。

---

## 工具链配置

1. **nRF5 SDK 15.3**
   下载 [nRF5 SDK 15.3](https://www.nordicsemi.com/Software-and-Tools/Software/nRF5-SDK/Download#infotabs)，解压并放入 `SDK/` 文件夹（确保存在 `SDK/components`）。

2. **ARM GCC 工具链**
   安装 `gcc-arm-none-eabi`（`7-2018-q2-update` 或 GCC 12+，已通过 `application/main/project/app.mk` 中的 `-Wno-array-bounds` 兼容 GCC 15.x）。
   将 `template/Makefile.posix.template` 或 `template/Makefile.windows.template` 复制为 `Makefile.posix` 或 `Makefile.windows`，并修改其中的工具链路径。

3. **SDCC（用于编译 CH554 USB 固件）**
   安装 [SDCC](http://sdcc.sourceforge.net/)（`>= 4.0`）。旧版本 SDCC（如 Ubuntu 18.04 / Debian 10 自带版本）无法通过编译。

4. **uECC（仅编译 Bootloader 时需要）**
   如需编译安全 DFU Bootloader，请先参考 [Nordic Secure DFU Bootloader 入门指南](https://devzone.nordicsemi.com/nordic/short-range-guides/b/software-development-kit/posts/getting-started-with-nordics-secure-dfu-bootloader) 编译 `uECC` 库。

---

## 编译

### 本地编译

```bash
cd keyboard/lot60-ble
make            # 编译 nRF52 主程序与 CH554 USB 固件
make bootloader # 编译 nRF52 安全 DFU Bootloader
```

### 辅助脚本与 Docker

也可使用辅助脚本或预配置 Docker 镜像（已包含工具链与 SDK）：

```bash
# 仅构建指定版本（例如 Lot60-BLE Rev.G/GX）
./scripts/build_lot60ble.sh --revision rev_g

# 构建全部版本
./scripts/build_lot60ble.sh

# 使用 Docker 构建
./scripts/build_lot60ble.sh --docker
# 或直接运行：
docker run lotlab/nrf52-keyboard
```

---

## 烧录

- **nRF52（DAP-Link / SWD）：** 推荐使用。需安装 [pyOCD](https://github.com/mbedmicro/pyOCD)（或通过 `uvx pyocd` 运行）。
- **nRF52（蓝牙 DFU）：** 需安装 [nrfutil](https://github.com/NordicSemiconductor/pc-nrfutil/) 生成 `.zip` DFU 升级包，或使用手机端 nRF Connect 应用无线刷写。
- **CH554（USB ISP）：** Windows 可使用官方 [WCH 烧写工具](http://www.wch.cn/downloads/WCHISPTool_Setup_exe.html)，Linux 可使用第三方 [librech551 (`usbisp`)](https://github.com/rgwan/librech551)。

在任意键盘目录下运行 `make help` 可查看所有烧录和打包指令。

如需详细的分步烧录、免拆壳软件切换模式（DFU / ISP）及固件兼容性检查说明，请参阅 [Lot60-BLE 硬件、模式切换与烧录指南](lot60-ble-guide.md)。

---

## 进阶开发文档

- [快速硬件移植指南](porting.md) — 适配新的键盘 PCB
- [外置驱动与模块化配置说明](driver.md) — 编写外设驱动、自定义 Fn、ADC 通道及 HID 配置项
- [键盘钩子 (Hooks) 说明](hook.md) — `hook_bootmagic()` 与 `action_function()`
- [通信协议文档](communicate.md) — UART 与 HID 配置通信协议规范
