[English](../lot60-ble-guide.md) | **Tiếng Việt** | [中文](../zh/lot60-ble-guide.md)

# Hướng dẫn Phần cứng, Chuyển chế độ & Nạp Firmware Lot60-BLE

> **PCB tham chiếu:** Rev.G / Rev.GX — 1 LED RGB — nRF52810 — tích hợp CMSIS-DAP
> **Fork:** <https://github.com/trinhquocviet/lotlab-nrf52-keyboard>
> **Wiki gốc:** <https://wiki.lotlab.org/ble/lot60-ble.html>

---

## 1. Tổng quan phần cứng

Lot60-BLE là bàn phím hai chế độ Bluetooth/USB layout 5×14 (60%) với **hai vi điều khiển** trên mỗi PCB:

| Chip | Vai trò | Công cụ nạp | Firmware |
|------|---------|-------------|----------|
| **nRF52810** (hoặc bản **nRF52832**) | Vi điều khiển chính — BLE, ma trận phím, LED | `pyocd` / `nrfutil` DFU | `.hex` hoặc `.zip` |
| **CH554** | Giao tiếp USB — HID, cầu nối UART, bộ nạp CMSIS-DAP | `usbisp` (Linux) / WCH ISP Tool (Windows) | `.bin` |

> **Lưu ý quan trọng:** Luôn cập nhật đồng bộ cả firmware nRF52 và CH554 khi nâng cấp.

### Phím tắt mặc định & Quản lý nguồn

- **Khởi động / Đánh thức từ ngủ sâu:** `Space + U`
- **Xóa thiết bị đã ghép nối:** `Space + E` (hoặc `Fn + R` trên fork `v1.2.1.0+`)
- **Ngủ thủ công:** `Fn + CapsLock`
- **Chuyển USB / BLE:** `Fn + Tab`
- **Chuyển nhiều thiết bị (`v1.2.0.0+`):** `Space + U + Q/W/E` (hoặc `Fn + T/Y/U` trên fork `v1.2.1.0+`, tối đa 3 thiết bị BLE)
- **Thời gian chờ nghỉ:** Quét chậm sau `15 giây`, tự động ngủ sau `600 giây`

### Màu LED trạng thái (1 LED RGB — `rev_g`)

| Màu | Mã Hex | Ý nghĩa |
|-----|--------|---------|
| Trắng | `#FFFFFF` | Chưa kết nối (chờ) |
| Xanh lơ | `#66FFFF` | Đã kết nối BLE |
| Xanh da trời | `#0099FF` | Đã kết nối USB |
| Cam | `#FF8000` | Đang sạc |
| Xanh lá | `#00FF00` | Sạc đầy |
| Vàng | `#FFFF00` | Yêu cầu nhập passkey |
| Hồng | `#FF0080` | Đã gửi passkey |
| Tím | `#FF00FF` | Đang ngủ |

---

## 2. Các phiên bản PCB & Tương thích Firmware

### Bảng phiên bản

Hệ thống build tạo firmware cho **8 biến thể phiên bản** với sơ đồ chân GPIO, chip và cấu hình LED khác nhau. **Nạp sai phiên bản có thể làm hỏng (brick) bàn phím.**

| Thư mục | Mạch | Chip | Cấu hình LED | CMSIS-DAP | Nút DFU |
|---------|------|------|--------------|-----------|---------|
| `rev_c` | Rev.C | nRF52810 | 1 RGB LED | ✅ Có | Không (Buttonless DFU) |
| `rev_e` | Rev.E | nRF52810 | 1 RGB LED | ❌ Không | Không (Buttonless DFU) |
| `rev_e_3led` | Rev.E | nRF52810 | 3 LED trạng thái | ❌ Không | Không (Buttonless DFU) |
| `rev_f` | Rev.F | nRF52810 | 1 RGB LED | ❌ Không | Không (Buttonless DFU) |
| `rev_f_3led` | Rev.F | nRF52810 | 3 LED trạng thái | ❌ Không | Không (Buttonless DFU) |
| **`rev_g`** | **Rev.G/GX** | **nRF52810** | **1 RGB LED** | **✅ Có** | **Có (chân RESET)** |
| `rev_g_3led` | Rev.G/GX | nRF52810 | 3 LED trạng thái | ✅ Có | Có (chân RESET) |
| `rev_g_832` | Rev.G/GX | **nRF52832** | 1 RGB LED | ✅ Có | Có (chân RESET) |

### Hậu quả khi nạp sai firmware

| Sai lệch | Hậu quả |
|----------|---------|
| Sai dòng mạch (vd. `rev_e` trên Rev.GX) | Sai chân GPIO → liệt phím, sai LED, hỏng USB |
| Sai chip (`rev_g_832` trên nRF52810) | Firmware vượt 192 KB flash → **không khởi động, nguy cơ brick** |
| Sai cấu hình LED (`rev_g_3led` trên mạch 1 LED) | Bàn phím hoạt động nhưng màu LED trạng thái sai |

### Chi tiết cấu hình LED

Định nghĩa trong `keyboard/lot60-ble/common/common.mk`:
- Không đặt `THREE_LED_STATUS` → build với `ONE_RGB_LED = yes` và `user_evt.c` (driver 1 LED RGB)
- Đặt `THREE_LED_STATUS = yes` → build với `THREE_LED_STATUS` và `3led_status.c` (driver 3 LED rời)

### Giới hạn bộ nhớ Chip

| Chip | Dung lượng Flash | Địa chỉ tối đa |
|------|------------------|----------------|
| nRF52810 | 192 KB | `0x30000` |
| nRF52832 | 512 KB | `0x80000` |
| CH554 | 14 KB code | `0x3800` |

---

## 3. Hệ thống Build & Quy ước đặt tên tệp

### Yêu cầu Toolchain

| Công cụ | Phiên bản | Mục đích |
|---------|-----------|----------|
| `arm-none-eabi-gcc` | 7-2018-q2-update hoặc GCC 12+ | Biên dịch firmware nRF52 |
| `sdcc` | >= 4.0.0 | Biên dịch firmware CH554 |
| `nrfutil` | Mới nhất | Tạo gói BLE DFU (`.zip`) |
| `mergehex` | Nordic CLI tools | Gộp SoftDevice + application thành `.hex` |
| `make` | GNU Make | Điều phối build |
| nRF5 SDK | 15.3.0 | Nordic SDK (đặt trong `SDK/`) |

### Lệnh Build

```bash
# Build một phiên bản (vd. Rev.G/GX 1 LED RGB)
./scripts/build_lot60ble.sh --revision rev_g

# Build tất cả 8 phiên bản
./scripts/build_lot60ble.sh

# Build bằng Docker (không cần cài toolchain trên máy)
./scripts/build_lot60ble.sh --docker
```

### Quy ước đặt tên tệp Firmware

Được tạo bởi `keyboard/lot60-ble/Makefile`:

```text
lot60-ble-{revision}-{chip}-{date}-{git_version}.{ext}
```

Ví dụ đầu ra cho `rev_g`:

```text
lot60-ble-rev_g-nrf52-2026_06_11-v1.2.0.0.hex   # nạp qua pyocd (đã gộp SoftDevice)
lot60-ble-rev_g-nrf52-2026_06_11-v1.2.0.0.zip   # gói BLE DFU
lot60-ble-rev_g-ch554-2026_06_11-v1.2.0.0.bin   # firmware USB CH554
```

### Quy trình GitHub Release

- Push tag dạng `v*.*.*.*` sẽ kích hoạt `.github/workflows/Release.yml`.
- Build cả 8 phiên bản và tải tối đa 24 tệp (`*.hex`, `*.zip`, `*.bin`) lên GitHub Release.

### Các Define chính cho Rev.G (`keyboard/lot60-ble/common/config.rev_g.h`)

```c
#define PRODUCT "Lot60.G"
#define DEVICE_VER 0x0003
#define LED_CAPS 19
#define LED_RGB_R 24
#define LED_RGB_G 23
#define LED_RGB_B 22
#define UART_RXD 17
#define UART_TXD 18
// Ma trận: 5 hàng × 14 cột
// row_pin_array    = { 25, 28, 29, 30, 26 }
// column_pin_array = { 16, 15, 14, 13, 12, 11, 27, 3, 4, 5, 6, 7, 8, 9 }
```

---

## 4. Chuyển chế độ bằng phần mềm (Không cần Jumper / Mở vỏ)

Khi bàn phím đã lắp kín trong vỏ (case) và không chạm tới nút hay jumper mặt sau PCB, bạn có thể chuyển vào mọi chế độ nạp/gỡ lỗi qua USB bằng các script trong `scripts/`.

Các script sử dụng metadata PEP 723 nội tuyến và chạy trực tiếp bằng [`uv`](https://docs.astral.sh/uv/):

```bash
# Cài uv nếu chưa có
curl -LsSf https://astral.sh/uv/install.sh | sh
```

### 4.1 Truy cập bộ nạp tích hợp CMSIS-DAP

- **Cơ chế:** Khi chạy firmware chuẩn qua USB, CH554 hoạt động như một thiết bị USB tổ hợp (composite), cung cấp đồng thời giao diện bàn phím HID và bộ nạp DAP-Link/CMSIS-DAP. Không cần chuyển chế độ hay nối jumper trên Rev.GX.
- **Kiểm tra kết nối:**
  ```bash
  uvx pyocd list
  ```
  Đầu dò `CMSIS-DAP` sẽ hiển thị trong danh sách.

### 4.2 Vào chế độ nRF52 BLE DFU (`scripts/enter_dfu.py`)

- **Cơ chế:** Bootloader của nRF5 SDK kiểm tra thanh ghi `GPREGRET` (`0x4000051C`) khi khởi động. Ghi giá trị `0xB1` (`BOOTLOADER_DFU_START`) qua CMSIS-DAP rồi soft-reset sẽ ép nRF52 vào chế độ BLE DFU (`DFUTarg`) mà không cần nhấn `Space + B` hay nút reset vật lý.
- **Cách dùng:**
  ```bash
  uv run scripts/enter_dfu.py
  ```
  Tìm `DFUTarg` trong nRF Connect (điện thoại) hoặc nạp gói `.zip` qua `nrfutil`.

### 4.3 Vào chế độ CH554 USB ISP (`scripts/enter_isp.py`)

- **Cơ chế:** Trong `usb/main.c`, firmware CH554 lắng nghe trên Endpoint 3 gói tin đặc biệt có byte 2 là `0xF0` và byte 3 là `0x00`. Khi nhận được, chip tắt watchdog, ngắt USB và nhảy tới `0x3800` (`isp_enter()`, bootloader tích hợp của CH554) — không cần giữ nút K1 mặt sau PCB.
- **Cách dùng:**
  ```bash
  uv run scripts/enter_isp.py
  ```
  Bàn phím sẽ ngắt kết nối và hiện lại dưới dạng thiết bị CH554 ISP sẵn sàng cho `usbisp`. Sau khi nạp `.bin`, thiết bị tự động khởi động lại về chế độ chuẩn.

### 4.4 Tùy chọn: Môi trường ảo cố định (`.venv`)

Nếu muốn dùng môi trường ảo Python truyền thống thay vì `uv run`:

```bash
./scripts/setup_env.sh
source scripts/.venv/bin/activate
```

Script sẽ cài `hidapi` và `pyocd` từ `scripts/requirements.txt`.

---

## 5. Các phương pháp nạp Firmware & Bộ kiểm tra tương thích

### Phương pháp 1: `pyocd` (CMSIS-DAP / SWD) — Khuyên dùng cho Rev.G/GX

Nạp có dây qua bộ nạp CMSIS-DAP tích hợp.

```bash
# Nạp tiêu chuẩn
./scripts/flash_lot60ble.sh -m pyocd -f lot60-ble-rev_g-nrf52-*.hex

# Xóa toàn bộ chip + nạp mới (khi hạ cấp hoặc cài đặt lại)
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex --erase
```

**Điều kiện:**
- `pyocd` (`pip3 install pyocd` hoặc `uvx pyocd`)
- macOS: `brew install libusb`
- Linux: udev rules (`sudo python3 -m pyocd.udev`)

### Phương pháp 2: Bluetooth DFU (`nrfutil` / nRF Connect)

Cập nhật không dây qua BLE bằng gói DFU `.zip`.

```bash
./scripts/flash_lot60ble.sh -m dfu -f lot60-ble-rev_g-nrf52-*.zip
```

**Vào chế độ DFU** (chọn một trong các cách):
- Phần mềm: `uv run scripts/enter_dfu.py`
- Phím tắt: `Fn + CapsLock` (ngủ), sau đó `Space + B` (đánh thức vào DFU)
- Phần cứng: nhấn nút `RESET` trên Rev.G/GX

**DFU qua điện thoại:** Mở nRF Connect (iOS/Android) → quét & kết nối `DFUTarg` → DFU → chọn tệp `.zip`.

### Phương pháp 3: CH554 USB ISP (Linux / Windows)

Cập nhật chip cầu nối USB CH554 bằng tệp `.bin`.

```bash
./scripts/flash_lot60ble.sh -m usb -f lot60-ble-rev_g-ch554-*.bin
```

**Vào chế độ ISP** (chọn một trong hai cách):
- Phần mềm: `uv run scripts/enter_isp.py`
- Phần cứng: rút USB → giữ nút K1 (mặt sau PCB) → cắm USB → thả sau khi máy nhận

**Công cụ:**
- Linux: `usbisp` từ [librech551](https://github.com/rgwan/librech551)
- Windows: [WCH ISP Tool](http://www.wch.cn/downloads/WCHISPTool_Setup_exe.html)
- macOS: Không hỗ trợ trực tiếp (dùng máy ảo Linux)

### Bộ kiểm tra tương thích Firmware 5 lớp

`scripts/flash_lot60ble.sh` kiểm tra trước khi nạp:
1. **Đuôi tệp vs phương pháp:** `.hex` (pyocd), `.zip` (dfu), `.bin` (usb)
2. **Phiên bản firmware vs phiên bản PCB:** kiểm tra khớp dòng mạch, chip và LED
3. **Loại chip vs phương pháp nạp:** chặn lỗi nạp chéo giữa hai chip
4. **Hỗ trợ CMSIS-DAP:** cảnh báo nếu PCB mục tiêu không có bộ nạp tích hợp
5. **Kiểm tra nhị phân:** kiểm tra địa chỉ Intel HEX (chặn nạp firmware nRF52832 512 KB lên nRF52810 192 KB) và giới hạn 14 KB của `.bin` CH554

```bash
# Mặc định PCB là rev_g
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex

# Chỉ định phiên bản PCB khác
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex -p rev_g_3led

# Bỏ qua kiểm tra (cẩn trọng)
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex --skip-compat
```

### Quy trình nạp nhanh "Không cần Jumper"

1. **Firmware chính nRF52 (`.hex`):** `./scripts/flash_lot60ble.sh -m pyocd -f <file>.hex` (CMSIS-DAP luôn bật)
2. **nRF52 BLE DFU (`.zip`):** `uv run scripts/enter_dfu.py` → nạp `.zip` qua nRF Connect / `nrfutil`
3. **Firmware USB CH554 (`.bin`):** `uv run scripts/enter_isp.py` → nạp `.bin` qua `usbisp` (tự khởi động lại sau khi xong)

---

## 6. Tham khảo Scripts & Các tệp mã nguồn chính

### Tổng hợp Scripts

| Script | Mục đích |
|--------|----------|
| `scripts/build_lot60ble.sh` | Build firmware từ mã nguồn (1 phiên bản hoặc tất cả) |
| `scripts/flash_lot60ble.sh` | Nạp firmware kèm bộ kiểm tra tương thích 5 lớp |
| `scripts/enter_dfu.py` | Khởi động lại nRF52 vào chế độ BLE DFU qua CMSIS-DAP |
| `scripts/enter_isp.py` | Khởi động lại CH554 vào chế độ USB ISP qua gói tin HID |
| `scripts/setup_env.sh` | Tạo môi trường `.venv` cố định với `hidapi` & `pyocd` |

### Tham số thường dùng

**`build_lot60ble.sh`:** `-r, --revision <rev>` | `-D, --docker` | `-j, --jobs <N>` | `-o, --output <dir>`

**`flash_lot60ble.sh`:** `-m, --method <pyocd|dfu|usb>` | `-f, --file <path>` | `-p, --pcb <rev>` (mặc định: `rev_g`) | `-e, --erase` | `-d, --dfu-device <addr>` | `--skip-compat`

### Các tệp mã nguồn chính

| Tệp | Mục đích |
|-----|----------|
| `keyboard/lot60-ble/Makefile` | Điều phối build & quy tắc đặt tên tệp |
| `keyboard/lot60-ble/common/common.mk` | Logic phiên bản phần cứng, LED và chip |
| `keyboard/lot60-ble/common/config.h` | Cấu hình chung USB ID, ma trận, BLE và nguồn |
| `keyboard/lot60-ble/common/config.rev_g.h` | Sơ đồ chân GPIO cho Rev.G/GX |
| `keyboard/lot60-ble/common/user_evt.c` | Logic LED trạng thái 1 bóng RGB |
| `keyboard/lot60-ble/rev_g/Makefile` | Cấu hình build Rev.G (`HARDWARE_VERSION = REV_G`) |
| `usb/main.c` / `usb/usb.mk` | Cầu nối USB CH554, kích hoạt ISP & cấu hình build |
| `.github/workflows/Release.yml` | Pipeline tự động build & phát hành đa phiên bản |
