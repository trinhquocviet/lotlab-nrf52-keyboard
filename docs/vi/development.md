[English](../development.md) | **Tiếng Việt** | [中文](../zh/development.md)

# Hướng dẫn Phát triển (Development Guide)

Tài liệu này hướng dẫn cài đặt toolchain biên dịch, build firmware và bootloader, nạp phần cứng đích và các tài liệu kỹ thuật liên quan.

---

## Nhánh mã nguồn

- `master`: Nhánh phát triển chính, có thể chứa các thay đổi đang hoàn thiện.
- `develop`: Nhánh thử nghiệm, có thể chứa tính năng chưa hoàn chỉnh hoặc lỗi biên dịch.

Nếu sử dụng hàng ngày, hãy tải bản ổn định đã build sẵn tại trang **[Releases](https://github.com/trinhquocviet/lotlab-nrf52-keyboard/releases)**.

---

## Cài đặt Toolchain

1. **nRF5 SDK 15.3**
   Tải [nRF5 SDK 15.3](https://www.nordicsemi.com/Software-and-Tools/Software/nRF5-SDK/Download#infotabs), giải nén và đặt vào thư mục `SDK/` (sao cho có `SDK/components`).

2. **ARM GCC Toolchain**
   Cài đặt `gcc-arm-none-eabi` (`7-2018-q2-update` hoặc GCC 12+, tương thích GCC 15.x thông qua `-Wno-array-bounds` trong `application/main/project/app.mk`).
   Sao chép `template/Makefile.posix.template` hoặc `template/Makefile.windows.template` thành `Makefile.posix` hoặc `Makefile.windows` tùy hệ điều hành và sửa đường dẫn toolchain bên trong.

3. **SDCC (cho firmware USB CH554)**
   Cài đặt [SDCC](http://sdcc.sourceforge.net/) (`>= 4.0`). Các bản SDCC cũ (như trên Ubuntu 18.04 / Debian 10) sẽ không biên dịch được firmware CH554.

4. **uECC (chỉ cần khi build Bootloader)**
   Để biên dịch Secure DFU bootloader, cần biên dịch thư viện `uECC` trước. Xem hướng dẫn tại [Getting Started with Nordic's Secure DFU Bootloader](https://devzone.nordicsemi.com/b/blog/posts/getting-started-with-nordics-secure-dfu-bootloader).

---

## Biên dịch

### Build cục bộ

```bash
cd keyboard/lot60-ble
make            # Biên dịch chương trình chính nRF52 và firmware USB CH554
make bootloader # Biên dịch Secure DFU bootloader cho nRF52
```

### Script hỗ trợ & Docker

Bạn cũng có thể build bằng script đi kèm hoặc Docker image (đã tích hợp sẵn toolchain và SDK):

```bash
# Build một phiên bản cụ thể (vd. Lot60-BLE Rev.G/GX)
./scripts/build_lot60ble.sh --revision rev_g

# Build tất cả các phiên bản
./scripts/build_lot60ble.sh

# Build qua Docker
./scripts/build_lot60ble.sh --docker
# hoặc chạy trực tiếp:
docker run lotlab/nrf52-keyboard
```

---

## Nạp firmware (Flashing)

- **nRF52 (DAP-Link / SWD):** Khuyên dùng. Cài đặt [pyOCD](https://github.com/mbedmicro/pyOCD) (hoặc chạy qua `uvx pyocd`).
- **nRF52 (BLE DFU):** Cài đặt [nrfutil](https://github.com/NordicSemiconductor/pc-nrfutil/) để tạo gói DFU `.zip`, hoặc nạp không dây qua ứng dụng nRF Connect trên điện thoại.
- **CH554 (USB ISP):** Dùng công cụ chính thức [WCH ISP Tool](http://www.wch.cn/downloads/WCHISPTool_Setup_exe.html) trên Windows, hoặc [librech551 (`usbisp`)](https://github.com/rgwan/librech551) trên Linux.

Chạy `make help` trong thư mục bàn phím bất kỳ để xem toàn bộ các lệnh nạp và đóng gói.

Để xem hướng dẫn nạp chi tiết, chuyển chế độ bằng phần mềm (DFU / ISP không cần nhấn nút vật lý) và kiểm tra tương thích, xem [Hướng dẫn Phần cứng, Chuyển chế độ & Nạp Firmware Lot60-BLE](lot60-ble-guide.md).

---

## Tài liệu chuyên sâu

- [Hướng dẫn Port phần cứng nhanh](porting.md) — Thêm hỗ trợ cho mạch bàn phím mới
- [Driver ngoại vi & Hook mô-đun](driver.md) — Viết driver ngoại vi, hàm xử lý Fn, kênh ADC và cấu hình HID tùy chỉnh
- [Tham khảo các Hook bàn phím](hook.md) — `hook_bootmagic()` và `action_function()`
- [Giao thức truyền thông](communicate.md) — Đặc tả giao thức cấu hình UART và HID
