[English](../communicate.md) | **Tiếng Việt** | [中文](../zh/communicate.md)

# Tài liệu Giao thức Truyền thông

## Giao thức Truyền thông UART

### Định dạng cơ bản

`CMD DAT ... DAT SUM`

- `CMD`: Mã lệnh
- `DAT`: Dữ liệu
- `SUM`: Checksum của toàn bộ các byte lệnh và dữ liệu phía trước

Tùy theo `CMD`, độ dài của `DAT` có thể thay đổi. Nếu độ dài `DAT` bằng 0 thì không cần `SUM`.

Chip chủ (`CH554`) định kỳ gửi gói tin trạng thái tới chip phụ (`nRF52810`) để yêu cầu gửi dữ liệu lên.

### Các lệnh

#### Gói Ping & Trạng thái hiện tại
```
0b0001 xxxx
       ||||
       |||+--- Gói dữ liệu nhận trước đó có thành công không (1 = thành công)
       ||+---- Trạng thái sạc (1 = đã sạc đầy)
       |+----- Trạng thái kết nối host (1 = đã kết nối với host)
       +------ Protocol hiện tại của bàn phím
```

Không có `DAT`.

#### Gửi trạng thái LED xuống
```
0b001x xxxx
     + ++++--- Trạng thái LED 5-bit
```

Không có `DAT`.

#### Giao tiếp HID
```
0b01xx xxxx
    ++ ++++--- Độ dài gói báo cáo (tối đa 61). Độ dài DAT là giá trị này + 1
```
Byte đầu tiên của `DAT` là HID Report ID:
`0`: keyboard, `1`: mouse, `2`: consumer, `3`: system, `0x80`: NKRO keyboard.

#### Giao tiếp Cấu hình
```
0b10xx xxxx
    || ||||
    ++-++++--- Độ dài gói HID (tối đa 62)
```
Độ dài `DAT` được xác định bởi trường 6-bit ở trên.

#### Phản hồi Giao tiếp Cấu hình
Định dạng giống như trên; chi tiết xem bên dưới.

## Giao thức Cấu hình

### Định dạng cơ bản

`CMD LEN DAT[0] ... DAT[N-1]`

`N` tối đa là 60.

> HID hỗ trợ tối đa 64 byte; trừ Report ID còn lại 63 byte.
> Tại sao là 60 thay vì 61? Trong giao thức UART cũ, bộ đệm gửi UART chỉ cấp 64 byte. Trừ header gói và checksum cuối còn 62 byte. Trừ tiếp `CMD` và `LEN` thì còn 60 byte.

Đối với giao tiếp cấu hình qua HID, độ dài payload cố định là 63 byte (64 byte nếu tính cả Report ID):

```
0x3F CMD  LEN  DAT[0] ... DAT[LEN-1] 0 ... 0
HID  Lệnh Độ dài     Phần dữ liệu     Đệm 0
```

### Dải mã lệnh

- `0x00-0x0F`: Mã phản hồi
- `0x10-0x1F`: Dự phòng
- `0x20-0x2F`: Lấy thông tin bàn phím
- `0x30-0x3F`: Thiết lập thông tin bàn phím
- `0x40-0x7F`: Lệnh cấu hình mô-đun bên trong bàn phím
- `0x80-0xEF`: Lệnh dự phòng cho nhà sản xuất
- `0xF0-0xFF`: Lệnh đặc biệt / dự phòng

#### Mã phản hồi chung

- `0x00`: Thao tác thành công
- `0x01`: Thao tác chưa được định nghĩa
- `0x02`: Lỗi tham số
- `0x03`: Lỗi nội bộ
- `0x04`: Dữ liệu ghi vượt quá giới hạn
- `0x05`: Lỗi checksum UART (yêu cầu gửi lại)

#### 0x20 Lấy thông tin bàn phím

Yêu cầu:
```
SUBINFO[1]: Mức chi tiết (tùy chọn, hỗ trợ từ phiên bản giao thức 5)
```

##### Thông tin cơ bản (`0`)

```
VENDOR[2]         Vendor ID (0x1071)
PRODUCT[2]        Product ID (0xab60)
HWVER[1]          Phiên bản phần cứng (3)
PROTOCOL_VER[1]   Phiên bản giao thức truyền thông (1)
FIRMWARE_VER[4]   Phiên bản firmware (0xcb4cde53)
BUILD_DATE[4]     Thời gian build (UNIX timestamp)
FUNCTION_TABLE[4] Các tính năng được hỗ trợ / bật
```

Function Table:
```c
/* 0-7 Tính năng nền tảng */
0: Bật BootMagic
1: Bật MouseKey
2: Bật phím đa phương tiện và hệ thống
3: Bật NKRO
/* 4-7 Dự phòng */

/* 8-15 Tính năng bổ sung */
8:  Bật lưu trữ Keymap
9:  Sử dụng Action Map
10: Bật Macro
11: Bật lưu trữ cấu hình

/* 16-31 Tính năng driver mở rộng */
16: RGB Light
17: RGB Matrix
18: Rotary encoder (núm xoay)
19: Joystick
...
```

##### Thông tin mở rộng không dây (`1`)

```
SERIAL[6] Số sê-ri duy nhất của bàn phím (địa chỉ MAC)
MTU[1]    Độ dài phân mảnh gói tối đa (N)
RFU[1]    Dự phòng
```

Do BLE HID không thể lấy trực tiếp địa chỉ MAC, lệnh này trả về địa chỉ MAC cùng MTU truyền tải để công cụ cấu hình biết cách chia nhỏ gói tin.

Các giá trị MTU phổ biến:
- Mặc định: `56` (`59` làm tròn xuống `56`)
- USB: `60`
- BLE: `16` (`17` làm tròn xuống `16`)

#### 0x21 Lấy mã phím đơn lẻ

Yêu cầu:
```
LAYER[1] Tầng phím
ROW[1]   Hàng
COL[1]   Cột
```
Phản hồi:
```
KEYCODE[2] Mã phím tương ứng
```

#### 0x22 Lấy chức năng Fn đơn lẻ

Yêu cầu:
```
Fn[1] ID phím Fn
```
Phản hồi:
```
KEYCODE[2] Mã phím / action code của phím Fn
```

#### 0x23 Lấy toàn bộ mã phím

Yêu cầu:
```
OFFSET[2] Vị trí offset yêu cầu hiện tại
```
Phản hồi:
```
LEN:    Độ dài dữ liệu (tối đa 56); 0 nghĩa là đã truyền xong
DATA[]: Dữ liệu mã phím
```

#### 0x24 Lấy toàn bộ chức năng Fn

Yêu cầu:
```
OFFSET[1] Vị trí offset yêu cầu hiện tại
```
Phản hồi:
```
LEN:    Độ dài dữ liệu (tối đa 56); 0 nghĩa là đã truyền xong
DATA[]: Dữ liệu Fn
```

#### 0x25 Lấy giá trị mục cấu hình cụ thể

Yêu cầu:
```
INDEX[1] Chỉ mục (Index) của mục cấu hình
```
Phản hồi:
```
DATA[LEN] Giá trị của mục cấu hình được chỉ định
```
Xem mục Loại cấu hình bên dưới để biết danh sách Index.

#### 0x27 Lấy toàn bộ Macro

Yêu cầu:
```
OFFSET[2] OFFSET yêu cầu hiện tại
```
Phản hồi:
```
DATA[]: Dữ liệu Macro
```

#### 0x31 Thiết lập mã phím đơn lẻ

Yêu cầu:
```
LAYER[1]   Tầng phím
ROW[1]     Hàng
COL[1]     Cột
KEYCODE[2] Mã phím cần đặt
```

#### 0x32 Thiết lập chức năng Fn đơn lẻ

Yêu cầu:
```
Fn[1]      ID phím Fn
KEYCODE[2] Mã phím cần đặt
```

#### 0x33 Thiết lập toàn bộ mã phím

Yêu cầu:
```
ID[1]   ID phân mảnh gói hiện tại
DATA[N] Dữ liệu
```

Quá trình gửi hoàn tất khi độ dài gói nhỏ hơn độ dài phân mảnh.

#### 0x34 Thiết lập toàn bộ giá trị Fn

Yêu cầu:
```
ID[1]   ID phân mảnh gói hiện tại
DATA[N] Dữ liệu
```

Quá trình gửi hoàn tất khi độ dài gói nhỏ hơn độ dài phân mảnh.

#### 0x35 Thiết lập giá trị mục cấu hình cụ thể

Yêu cầu:
```
ID[1]       ID mục cấu hình
DAT[LEN-1]  Giá trị mục cấu hình
```

#### 0x37 Thiết lập toàn bộ Macro

Yêu cầu:
```
ID[1]   ID phân mảnh gói hiện tại
DATA[N] Dữ liệu
```

Quá trình gửi hoàn tất khi độ dài gói nhỏ hơn độ dài phân mảnh.

#### 0x3D Hủy bỏ dữ liệu đang ghi

Yêu cầu:
```
TYPE[1] Loại dữ liệu: 1: Keymap, 2: Fn, 4: Config, 8: Macro, 0xFF: Tất cả
```

#### 0x3E Lưu dữ liệu vào bộ nhớ

Yêu cầu:
```
TYPE[1] Loại dữ liệu: 1: Keymap, 2: Fn, 4: Config, 8: Macro, 0xFF: Tất cả
```

#### 0x3F Đặt lại (Reset) bàn phím

Yêu cầu:
```
TYPE[1] Loại reset: 1: Keymap, 2: Fn, 4: Config, 8: Macro, 0xFF: Tất cả
```

### 0xF0 Vào chế độ USB ISP

Không có tham số yêu cầu, không có giá trị trả về. Thực thi xong sẽ vào ngay chế độ USB ISP.

### Xử lý tùy biến theo mô-đun

#### 0x40 Lấy / Thiết lập Layer hiện tại

Khi `LEN = 0`, trả về thông tin layer hiện tại:

```
LAYER[4]
DEFAULT_LAYER[4]
```

Khi `LEN = 8`, thiết lập thông tin layer theo định dạng trên.

#### 0x41 Thực thi Action Code

`LEN` cố định là `2`:

```
ACTION_CODE[2]
```

Thực thi Action Code được truyền vào.

#### 0x42 Lấy thông tin pin

`LEN` cố định là `0`, trả về:

```
BATT_LEVEL[1]
```

#### 0x43 Lấy / Thiết lập trạng thái USB

`// T.B.D.`

#### 0x44 Lấy / Thiết lập trạng thái BLE

`// T.B.D.`

#### 0x50 Lấy / Thiết lập trạng thái ESB

`// T.B.D.`

### Các loại cấu hình

- `0x10`: Độ dài 2, thời gian chờ quét chậm (giây)
- `0x11`: Độ dài 2, thời gian chờ tự động ngủ (phút)
- `0x12`: Độ dài 2, thời gian tắt LED tiết kiệm pin (giây)
