[English](../porting.md) | **Tiếng Việt** | [中文](../zh/porting.md)

# Hướng dẫn Port phần cứng nhanh

0. Sao chép thư mục `keyboard/template` và đổi tên thành tên bàn phím của bạn.
1. Mở `Makefile` và cập nhật các tùy chọn cấu hình cho đúng dòng chip điều khiển.
2. Chuẩn bị sơ đồ mạch (schematic) của bàn phím để tra cứu cho các bước tiếp theo.
3. Mở `config.h`, cấu hình số hàng/cột của ma trận phím và danh sách chân IO; cấu hình chân LED, chân đo pin và chân UART.
4. Điều chỉnh các tùy chọn cấu hình còn lại trong `config.h` theo nhu cầu.
5. Mở `keymap_common.h` để định nghĩa ánh xạ giữa các phím và ma trận công tắc. Có thể sao chép từ các mẫu có sẵn trong `tmk/keyboard`.
6. Mở `keymap_plain.c` và chỉnh sửa keymap mặc định.
7. Chạy lệnh `make` trong terminal để thử biên dịch.

## Tính năng nâng cao

Xem [hook.md](hook.md) để tìm hiểu các hook bàn phím và chức năng của chúng.
