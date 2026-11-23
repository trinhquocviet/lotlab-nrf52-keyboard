**English** | [Tiếng Việt](vi/driver.md) | [中文](zh/driver.md)

# External Drivers

Device drivers are defined under `application/main/src/driver/`. See the `README.md` inside each driver's directory for usage details.

# Driver Development Guide

The driver system is built on top of an event-driven architecture. Drivers register an event handler function to react to system and keyboard events.

## Quick Start

Suppose you are writing `custom_driver.c`:

```c
// Include the event system header
#include "../keyboard/keyboard_evt.h"

// Implement the event handler
static void custom_driver_event_handler(enum user_event event, void* arg)
{
    // code...
}

// Register the event handler
EVENT_HANDLER(custom_driver_event_handler);
```

Then add the source file build rule to `driver.mk`.

## Events

See `events.h` for the full list of events.
By default, `arg` is **not** a pointer, but an enum value cast to `void*`. Cast `arg` back to the appropriate enum type when reading it.

If you need custom events, define your event IDs and argument types within your driver module.

## Other Modular Extensions

### Custom Fn Handler

```c
#include "../keyboard/keyboard_fn.h"

void custom_fn_handler(keyrecord_t* record, uint8_t id, uint8_t opt) {
    // Standard TMK Fn handling
}

FN_HANDLER(custom_fn_handler);
```

### Add Extra ADC Measurement Channel

```c
#include "../keyboard/adc_convert.h"

static struct adc_channel_config custom_adc_channel = {
    .adc_start = &adc_on_start,         // Measurement start callback
    .adc_finish = &adc_result_handler,  // Measurement result handler
    .period = 2000,                     // Measurement period (ms)
    .config = &channel_config,          // ADC channel configuration
};

ADC_CONVERT_CHANNEL(custom_adc_channel);
```

### Register an Entry in Config Storage

```c
#include "../keyboard/data_storage.h"

// Register a storage section named 'item' with length 4 bytes
CONFIG_SECTION(item, 4);

void write_item() {
    // Modify data
    memcpy(item.data, src, item.len);
    // Persist to flash storage
    storage_write(1 << STORAGE_CONFIG);
}
```

### Register a Custom HID Configuration Item

```c
#include "../protocol/hid_configuration.h"
#include "../keyboard/data_storage.h"

CONFIG_SECTION(item, 4);  // Register storage section
HID_CONFIG(0x20, item);   // Register HID config item with ID 0x20
```

### Register a New Host Communication Driver

```c
#include "keyboard_host_driver.h"

static struct host_driver custom_driver = {
    .keyboard_leds = &get_keyboard_led,        // Get LED state
    .queue_empty = &queue_empty,               // Check if TX queue is empty
    .send_packet = &custom_driver_send,        // Send HID packet
    .driver_working = &custom_driver_working,  // Check if driver is active
    .mtu = 62,                                 // Max transmission unit for config tools
};

// Register this driver with priority 3.
// Lower numbers have higher priority; the highest-priority active driver is used.
KEYBOARD_HOST_DRIVER(3, custom_driver);
```
