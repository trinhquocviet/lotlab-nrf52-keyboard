# nrf52-keyboard

[English](README.md) | **Tiếng Việt** | [中文](README.zh.md)

![Build Status](https://api.travis-ci.org/Lotlab/nrf52-keyboard.svg?branch=master)

## Tổng quan

Đây là phiên bản chỉnh sửa cho Lot60-BLE của Lotlab — firmware bàn phím dựa trên TMK dành cho dòng vi điều khiển Nordic nRF52 (hỗ trợ cả **nRF52810** và **nRF52832**), sử dụng nRF5 SDK 15.3 làm driver phần cứng tầng thấp và thư viện TMK cho logic bàn phím tầng trên.

Xem firmware gốc tại [Lotlab/nrf52-keyboard](https://github.com/Lotlab/nrf52-keyboard).

## Tính năng nổi bật

- **Chế độ kép BLE / USB** hỗ trợ USB NKRO, phím chuột, phím đa phương tiện (gồm phím chức năng macOS F3–F6) và macro
- **Chuyển nhiều thiết bị BLE** — chuyển đổi linh hoạt giữa tối đa 3 thiết bị
- **Tên BLE UTF-8 tùy chỉnh & profile theo kênh** — Thiết bị hiển thị theo tên tùy chỉnh kèm hậu tố `_<number>` để phân biệt
- **Đồng bộ mức pin trực tiếp** — đo SAADC thời gian thực, tự động gửi thông báo BLE BAS (đồng bộ widget pin macOS) và truy vấn pin qua HID (`0x16`)
- **Đèn LED RGB ESC tùy biến** — hiển thị màu theo từng kênh BLE và trạng thái kết nối/sạc
- **Cấu hình keymap / macro động** tự động nạp mặc định và timeout FDS khi nạp mới
- **Tiết kiệm điện & tối ưu nRF52810** — dòng nghỉ ~200 uA (tắt LED trên Lot60-BLE), `DYNAMIC_TX_POWER`, quét ma trận đơn thanh ghi `NRF_P0->IN` và thu hồi 2 KB RAM
- **Gia cố lưu trữ & UART** — hàng đợi FDS chống lỗi có chế độ dự phòng RAM và `NRF_UARTE0` EasyDMA + giao thức PPI
- **Công cụ build & nạp không cần jumper** — script build/nạp có kiểm tra tương thích 5 lớp, hỗ trợ GCC 12+ và chuyển chế độ DFU/ISP bằng phần mềm
- **Hệ thống driver & sự kiện mở rộng** — núm xoay (rotary encoder), WS2812, OLED và các hook mô-đun (xem `application/main/src/driver/`)

> Tải firmware tùy biến này tại trang **[Releases](https://github.com/trinhquocviet/lotlab-nrf52-keyboard/releases)**.

## Phần cứng hỗ trợ

Hiện hỗ trợ cả **nRF52810** và **nRF52832**. Xem thư mục `keyboard/` để biết danh sách PCB và các phiên bản được hỗ trợ.

## Cấu trúc thư mục

```text
.
├── application              # Bootloader và firmware ứng dụng chính cho nRF52
│   ├── main                 # Chương trình chính cho nRF52
│   │   ├── src              # Mã nguồn
│   │   │   ├── ble          # Ngăn xếp và dịch vụ Bluetooth Low Energy (BLE)
│   │   │   ├── config       # Cấu hình SDK và phần cứng
│   │   │   ├── driver       # Driver cho thiết bị ngoại vi
│   │   │   ├── keyboard     # Logic ma trận phím, nguồn, lưu trữ và sự kiện
│   │   │   ├── protocol     # Giao thức truyền thông UART / HID
│   │   │   └── tmk          # Lớp cầu nối TMK
│   │   └── project          # Project biên dịch và linker script
│   └── bootloader           # Secure DFU bootloader cho nRF52
├── docs                     # Tài liệu và hướng dẫn của dự án (EN / VI / ZH)
├── keyboard                 # Cấu hình phần cứng và keymap riêng cho từng bàn phím (vd. lot60-ble)
├── scripts                  # Script hỗ trợ biên dịch, nạp firmware và chuyển chế độ
├── SDK                      # Thư mục chứa Nordic nRF5 SDK 15.3
├── template                 # Template Makefile cho nRF SDK
├── tmk                      # Mã nguồn lõi TMK
├── usb                      # Firmware cầu nối USB CH554 & CMSIS-DAP
├── Dockerfile
├── README.md
└── ...
```

## Tài liệu

- [Hướng dẫn Phát triển](docs/vi/development.md)
- [Hướng dẫn Phần cứng, Chuyển chế độ & Nạp Firmware Lot60-BLE](docs/vi/lot60-ble-guide.md)
- [Giao thức](docs/vi/communicate.md)
- [Driver ngoại vi & Hook mô-đun](docs/vi/driver.md)
- [Tham khảo các Hook bàn phím](docs/vi/hook.md)
- [Hướng dẫn port phần cứng mới](docs/vi/porting.md)
