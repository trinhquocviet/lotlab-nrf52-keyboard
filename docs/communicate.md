**English** | [Tiếng Việt](vi/communicate.md) | [中文](zh/communicate.md)

# Communication Protocol Documentation

## UART Communication Protocol

### Basic Format

`CMD DAT ... DAT SUM`

- `CMD`: Command byte
- `DAT`: Data payload
- `SUM`: Checksum of all preceding command and data bytes

Depending on `CMD`, the length of `DAT` may vary. If `DAT` length is 0, `SUM` is omitted.

The host (`CH554`) periodically sends status packets to the slave (`nRF52810`) to request data uploads.

### Commands

#### Ping Packet & Current Status
```
0b0001 xxxx
       ||||
       |||+--- Whether the previous packet was received successfully (1 = success)
       ||+---- Charging status (1 = fully charged)
       |+----- Host connection status (1 = connected to host)
       +------ Current keyboard Protocol
```

No `DAT`.

#### LED Downstream
```
0b001x xxxx
     + ++++--- 5-bit LED status
```

No `DAT`.

#### HID Communication
```
0b01xx xxxx
    ++ ++++--- Report packet length (max 61). DAT length is this value + 1
```
The first byte of `DAT` is the HID Report ID:
`0`: keyboard, `1`: mouse, `2`: consumer, `3`: system, `0x80`: NKRO keyboard.

#### Configuration Communication
```
0b10xx xxxx
    || ||||
    ++-++++--- HID packet length (max 62)
```
`DAT` length is determined by the 6-bit length field above.

#### Configuration Communication Response
Same format as above; see details below.

## Configuration Protocol

### Basic Format

`CMD LEN DAT[0] ... DAT[N-1]`

`N` is at most 60.

> HID supports up to 64 bytes; excluding the Report ID, 63 bytes remain.
> Why 60 instead of 61? In the legacy UART protocol, only a 64-byte UART transmit buffer was allocated. Subtracting the packet header and trailing checksum leaves 62 bytes. Subtracting `CMD` and `LEN` leaves 60 bytes.

For HID-based configuration communication, the payload length is fixed at 63 bytes (64 bytes including the Report ID):

```
0x3F CMD  LEN  DAT[0] ... DAT[LEN-1] 0 ... 0
HID  Cmd  Len        Data payload      Zero-padded
```

### Command Ranges

- `0x00-0x0F`: Response codes
- `0x10-0x1F`: Reserved
- `0x20-0x2F`: Get keyboard information
- `0x30-0x3F`: Set keyboard information
- `0x40-0x7F`: Modular keyboard configuration commands
- `0x80-0xEF`: Vendor-reserved commands
- `0xF0-0xFF`: Special / reserved commands

#### Common Response Codes

- `0x00`: Operation succeeded
- `0x01`: Undefined operation
- `0x02`: Parameter error
- `0x03`: Internal error
- `0x04`: Write data exceeds limit
- `0x05`: UART checksum error (request retransmission)

#### 0x20 Get Keyboard Information

Request:
```
SUBINFO[1]: Detail level (optional, enabled from protocol version 5)
```

##### Basic Info (`0`)

```
VENDOR[2]         Vendor ID (0x1071)
PRODUCT[2]        Product ID (0xab60)
HWVER[1]          Hardware version (3)
PROTOCOL_VER[1]   Communication protocol version (1)
FIRMWARE_VER[4]   Firmware version (0xcb4cde53)
BUILD_DATE[4]     Build timestamp (UNIX timestamp)
FUNCTION_TABLE[4] Supported / enabled features
```

Function Table:
```c
/* 0-7 Core features */
0: BootMagic enabled
1: MouseKey enabled
2: Media and System keys enabled
3: NKRO enabled
/* 4-7 Reserved for future use */

/* 8-15 Additional features */
8:  Keymap storage enabled
9:  Action Map used
10: Macro enabled
11: Config storage enabled

/* 16-31 Extra driver features */
16: RGB Light
17: RGB Matrix
18: Rotary encoder
19: Joystick
...
```

##### Wireless Extended Info (`1`)

```
SERIAL[6] Keyboard unique serial number (MAC address)
MTU[1]    Maximum packet fragment length (N)
RFU[1]    Reserved for future use
```

Because BLE HID cannot directly expose the MAC address, this command returns it along with the transport MTU so configuration tools know how to fragment packets.

Common protocol MTU values:
- Default: `56` (`59` aligned down to `56`)
- USB: `60`
- BLE: `16` (`17` aligned down to `16`)

#### 0x21 Get Single Keycode

Request:
```
LAYER[1] Key layer
ROW[1]   Key row
COL[1]   Key column
```
Response:
```
KEYCODE[2] Keycode for the specified key
```

#### 0x22 Get Single Fn Action

Request:
```
Fn[1] Fn key ID
```
Response:
```
KEYCODE[2] Keycode / action code for the Fn key
```

#### 0x23 Get All Keycodes

Request:
```
OFFSET[2] Current request offset
```
Response:
```
LEN:    Data length (max 56); 0 indicates end of data
DATA[]: Keycode data
```

#### 0x24 Get All Fn Actions

Request:
```
OFFSET[1] Current request offset
```
Response:
```
LEN:    Data length (max 56); 0 indicates end of data
DATA[]: Fn data
```

#### 0x25 Get Specific Config Item

Request:
```
INDEX[1] Config item index
```
Response:
```
DATA[LEN] Value of the specified config item
```
See the Config Types section below for indices.

#### 0x27 Get All Macros

Request:
```
OFFSET[2] Current request offset
```
Response:
```
DATA[]: Macro data
```

#### 0x31 Set Single Keycode

Request:
```
LAYER[1]   Key layer
ROW[1]     Key row
COL[1]     Key column
KEYCODE[2] Keycode to set
```

#### 0x32 Set Single Fn Action

Request:
```
Fn[1]      Fn key ID
KEYCODE[2] Keycode to set
```

#### 0x33 Set All Keycodes

Request:
```
ID[1]   Current packet fragment ID
DATA[N] Data payload
```

Transfer is complete when packet length is less than the fragment length.

#### 0x34 Set All Fn Actions

Request:
```
ID[1]   Current packet fragment ID
DATA[N] Data payload
```

Transfer is complete when packet length is less than the fragment length.

#### 0x35 Set Specific Config Item

Request:
```
ID[1]       Config item ID
DAT[LEN-1]  Config item value
```

#### 0x37 Set All Macros

Request:
```
ID[1]   Current packet fragment ID
DATA[N] Data payload
```

Transfer is complete when packet length is less than the fragment length.

#### 0x3D Discard Pending Write Data

Request:
```
TYPE[1] Data type: 1: Keymap, 2: Fn, 4: Config, 8: Macro, 0xFF: All
```

#### 0x3E Commit Write Data to Storage

Request:
```
TYPE[1] Data type: 1: Keymap, 2: Fn, 4: Config, 8: Macro, 0xFF: All
```

#### 0x3F Reset Keyboard

Request:
```
TYPE[1] Reset type: 1: Keymap, 2: Fn, 4: Config, 8: Macro, 0xFF: All
```

### 0xF0 Enter USB ISP Mode

No request payload, no response. Immediately enters USB ISP mode.

### Modular Custom Handlers

#### 0x40 Get / Set Current Layer

When `LEN = 0`, returns current layer info:

```
LAYER[4]
DEFAULT_LAYER[4]
```

When `LEN = 8`, sets layer info using the same format.

#### 0x41 Execute Action Code

`LEN` is fixed at `2`:

```
ACTION_CODE[2]
```

Executes the specified Action Code.

#### 0x42 Get Battery Info

`LEN` is fixed at `0`, returns:

```
BATT_LEVEL[1]
```

#### 0x43 Get / Set USB Status

`// T.B.D.`

#### 0x44 Get / Set BLE Status

`// T.B.D.`

#### 0x50 Get / Set ESB Status

`// T.B.D.`

### Config Types

- `0x10`: Length 2, slow-scan timeout (seconds)
- `0x11`: Length 2, auto-sleep timeout (minutes)
- `0x12`: Length 2, LED power-saving timeout (seconds)
