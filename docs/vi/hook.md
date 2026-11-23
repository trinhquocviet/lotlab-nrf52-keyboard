[English](../hook.md) | **Tiếng Việt** | [中文](../zh/hook.md)

# Tham khảo các Hook bàn phím

## `hook_bootmagic()`

Hook xử lý TMK BootMagic khi khởi động.

Hook này có sẵn trong TMK. Trong dự án này, nó thực hiện chức năng xóa ghép nối (bond) và đánh thức/bật nguồn bằng phím. Bạn có thể ghi đè hook này nếu muốn tùy chỉnh quy trình khởi động hoặc thêm/bớt lệnh BootMagic.

## `action_function()`

Hook tích hợp sẵn của TMK để xử lý các hành động tùy chỉnh.

Mặc định, hook này xử lý chuyển đổi USB/BLE, ngủ/tắt nguồn và điều phối tới các module xử lý hành động tùy chỉnh. Không khuyến khích ghi đè trực tiếp hook này. Nếu chỉ muốn thêm chức năng `Fn` mới, hãy xem [driver.md](driver.md) để đăng ký `FN_HANDLER` dạng mô-đun.
