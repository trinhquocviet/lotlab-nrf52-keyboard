[English](../driver.md) | **Tiếng Việt** | [中文](../zh/driver.md)

# Driver ngoại vi

Các driver thiết bị được định nghĩa trong `application/main/src/driver/`. Xem tệp `README.md` bên trong từng thư mục driver để biết cách sử dụng.

# Hướng dẫn viết Driver

Hệ thống driver được xây dựng dựa trên kiến trúc hướng sự kiện (event-driven). Mỗi driver đăng ký một hàm xử lý sự kiện để phản hồi các sự kiện của hệ thống và bàn phím.

## Bắt đầu nhanh

Giả sử bạn cần viết `custom_driver.c`:

```c
// Nhúng file header hệ thống sự kiện
#include "../keyboard/keyboard_evt.h"

// Viết hàm xử lý sự kiện
static void custom_driver_event_handler(enum user_event event, void* arg)
{
    // code...
}

// Đăng ký hàm xử lý sự kiện
EVENT_HANDLER(custom_driver_event_handler);
```

Sau đó thêm quy tắc biên dịch vào `driver.mk`.

## Sự kiện (Events)

Xem chi tiết danh sách sự kiện trong `events.h`.
Mặc định, `arg` **không phải** con trỏ mà là một giá trị enum ép kiểu sang `void*`. Khi sử dụng hãy ép kiểu ngược lại về enum tương ứng.

Nếu cần sự kiện tùy chỉnh, hãy tự định nghĩa ID sự kiện và kiểu tham số ngay trong module driver của bạn.

## Các mở rộng mô-đun khác

### Tùy chỉnh xử lý Fn

```c
#include "../keyboard/keyboard_fn.h"

void custom_fn_handler(keyrecord_t* record, uint8_t id, uint8_t opt) {
    // Luồng xử lý TMK Fn thông thường
}

FN_HANDLER(custom_fn_handler);
```

### Thêm kênh đo ADC mở rộng

```c
#include "../keyboard/adc_convert.h"

static struct adc_channel_config custom_adc_channel = {
    .adc_start = &adc_on_start,         // Callback khi bắt đầu đo
    .adc_finish = &adc_result_handler,  // Hàm xử lý kết quả đo
    .period = 2000,                     // Chu kỳ đo (ms)
    .config = &channel_config,          // Cấu hình kênh ADC
};

ADC_CONVERT_CHANNEL(custom_adc_channel);
```

### Đăng ký một mục trong Config Storage

```c
#include "../keyboard/data_storage.h"

// Đăng ký vùng lưu trữ tên 'item' dài 4 byte
CONFIG_SECTION(item, 4);

void write_item() {
    // Thao tác dữ liệu
    memcpy(item.data, src, item.len);
    // Ghi xuống flash
    storage_write(1 << STORAGE_CONFIG);
}
```

### Đăng ký một mục cấu hình HID tùy chỉnh

```c
#include "../protocol/hid_configuration.h"
#include "../keyboard/data_storage.h"

CONFIG_SECTION(item, 4);  // Đăng ký vùng lưu trữ
HID_CONFIG(0x20, item);   // Đăng ký mục cấu hình HID với ID 0x20
```

### Đăng ký một phương thức giao tiếp Host mới

```c
#include "keyboard_host_driver.h"

static struct host_driver custom_driver = {
    .keyboard_leds = &get_keyboard_led,        // Lấy trạng thái LED
    .queue_empty = &queue_empty,               // Kiểm tra hàng đợi gửi có rỗng không
    .send_packet = &custom_driver_send,        // Gửi gói HID
    .driver_working = &custom_driver_working,  // Kiểm tra driver có đang hoạt động không
    .mtu = 62,                                 // Độ dài truyền tối đa cho công cụ cấu hình
};

// Đăng ký driver với mức ưu tiên 3.
// Số càng nhỏ ưu tiên càng cao; driver đang hoạt động có ưu tiên cao nhất sẽ được dùng.
KEYBOARD_HOST_DRIVER(3, custom_driver);
```
