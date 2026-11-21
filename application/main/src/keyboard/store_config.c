/*
Copyright 2019 Jim Jiang <jim@lotlab.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "store_config.h"
#include "../ble/ble_services.h"
#include "../events.h"
#include "action.h"
#include "data_storage.h"
#include "hid_configuration.h"
#include "keyboard_config.h"
#include "keyboard_evt.h"
#include "util.h"
#include <string.h>

#ifdef CONFIG_STORAGE

CONFIG_SECTION(config_slow_scan, 2);
HID_CONFIG(0x10, config_slow_scan);

/**
 * @brief 获取慢速扫描进入时间
 * 
 * @return uint16_t 
 */
uint16_t get_slow_scan_timeout(void)
{
    uint16_t data = UINT16_READ(config_slow_scan.data, 0);
    return data > 0 ? data : SLEEP_SLOW_TIMEOUT;
}

CONFIG_SECTION(config_sleep_timeout, 2);
HID_CONFIG(0x11, config_sleep_timeout);

/**
 * @brief 获取自动关机时间
 * 
 * @return uint16_t 
 */
uint16_t get_sleep_timeout(void)
{
    uint16_t data = UINT16_READ(config_sleep_timeout.data, 0);
    return data > 0 ? data : SLEEP_OFF_TIMEOUT;
}

CONFIG_SECTION(config_powersave_timeout, 2);
HID_CONFIG(0x12, config_powersave_timeout);

/**
 * @brief 获取LED省电模式进入时间
 * 
 * @return uint16_t 
 */
uint16_t get_led_powersave_timeout(void)
{
    uint16_t data = UINT16_READ(config_powersave_timeout.data, 0);
    return data > 0 ? data : LED_AUTOOFF_TIME;
}

static void write_u16_if_zero(uint8_t* ptr, uint16_t default_val)
{
    if (ptr != 0 && UINT16_READ(ptr, 0) == 0 && default_val > 0) {
        ptr[0] = (uint8_t)(default_val & 0xFF);
        ptr[1] = (uint8_t)((default_val >> 8) & 0xFF);
    }
}

void store_config_load_defaults(void)
{
    write_u16_if_zero(config_slow_scan.data, SLEEP_SLOW_TIMEOUT);
    write_u16_if_zero(config_sleep_timeout.data, SLEEP_OFF_TIMEOUT);
    write_u16_if_zero(config_powersave_timeout.data, LED_AUTOOFF_TIME);
}

static bool is_forbidden_codepoint(uint32_t cp)
{
    if (cp < 0x20 || cp == 0x7F)
        return true; // C0 & DEL
    if (cp >= 0x80 && cp <= 0x9F)
        return true; // C1 control characters
    if (cp >= 0x200B && cp <= 0x200F)
        return true; // Zero-width & LRM/RLM
    if (cp >= 0x202A && cp <= 0x202E)
        return true; // BiDi embeddings/overrides
    if (cp >= 0x2066 && cp <= 0x2069)
        return true; // BiDi isolates
    return false;
}

bool is_valid_utf8_device_name(const uint8_t* data, uint8_t len)
{
    if (len > MAX_CUSTOM_NAME_BYTES)
        return false;
    if (len > 0 && data == NULL)
        return false;

    uint8_t i = 0;
    while (i < len) {
        uint8_t b0 = data[i];
        uint32_t cp = 0;
        if (b0 <= 0x7F) {
            cp = b0;
            i += 1;
        } else if ((b0 & 0xE0) == 0xC0) {
            if (b0 < 0xC2 || i + 1 >= len)
                return false;
            uint8_t b1 = data[i + 1];
            if ((b1 & 0xC0) != 0x80)
                return false;
            cp = ((uint32_t)(b0 & 0x1F) << 6) | (b1 & 0x3F);
            i += 2;
        } else if ((b0 & 0xF0) == 0xE0) {
            if (i + 2 >= len)
                return false;
            uint8_t b1 = data[i + 1];
            uint8_t b2 = data[i + 2];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80)
                return false;
            if (b0 == 0xE0 && b1 < 0xA0)
                return false; // overlong
            if (b0 == 0xED && b1 >= 0xA0)
                return false; // UTF-16 surrogate range
            cp = ((uint32_t)(b0 & 0x0F) << 12) | ((uint32_t)(b1 & 0x3F) << 6) | (b2 & 0x3F);
            i += 3;
        } else if ((b0 & 0xF8) == 0xF0) {
            if (b0 > 0xF4 || i + 3 >= len)
                return false;
            uint8_t b1 = data[i + 1];
            uint8_t b2 = data[i + 2];
            uint8_t b3 = data[i + 3];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80)
                return false;
            if (b0 == 0xF0 && b1 < 0x90)
                return false; // overlong
            if (b0 == 0xF4 && b1 > 0x8F)
                return false; // > U+10FFFF
            cp = ((uint32_t)(b0 & 0x07) << 18) | ((uint32_t)(b1 & 0x3F) << 12)
                | ((uint32_t)(b2 & 0x3F) << 6) | (b3 & 0x3F);
            i += 4;
        } else {
            return false;
        }

        if (is_forbidden_codepoint(cp))
            return false;
    }
    return true;
}

uint8_t store_config_get_ble_name(uint8_t* out_buf, uint8_t max_len)
{
    ext_config_t* ext = storage_get_ext_config();
    if (ext->magic != EXT_CONFIG_MAGIC || ext->name_len == 0 || ext->name_len > MAX_CUSTOM_NAME_BYTES)
        return 0;
    uint8_t copy_len = ext->name_len < max_len ? ext->name_len : max_len;
    if (out_buf != NULL && copy_len > 0) {
        memcpy(out_buf, ext->custom_name, copy_len);
    }
    return copy_len;
}

bool store_config_set_ble_name(const uint8_t* data, uint8_t len, bool* overflow_err)
{
    if (overflow_err != NULL)
        *overflow_err = false;
    if (data == NULL || len != 1 + MAX_CUSTOM_NAME_BYTES)
        return false;

    uint8_t name_len = data[0];
    if (name_len > MAX_CUSTOM_NAME_BYTES) {
        if (overflow_err != NULL)
            *overflow_err = true;
        return false;
    }

    if (!is_valid_utf8_device_name(&data[1], name_len))
        return false;

    ext_config_t* ext = storage_get_ext_config();
    ext->magic = EXT_CONFIG_MAGIC;
    ext->version = EXT_CONFIG_VERSION;
    ext->name_len = name_len;
    memset(ext->custom_name, 0, MAX_CUSTOM_NAME_BYTES);
    if (name_len > 0) {
        memcpy(ext->custom_name, &data[1], name_len);
    }

    ble_device_name_refresh();
    return true;
}

void store_config_get_channel_profiles(uint8_t out[CHANNEL_PROFILE_COUNT])
{
    ext_config_t* ext = storage_get_ext_config();
    if (ext->magic != EXT_CONFIG_MAGIC) {
        memset(out, 0, CHANNEL_PROFILE_COUNT);
        return;
    }
    memcpy(out, ext->channel_profile, CHANNEL_PROFILE_COUNT);
}

bool store_config_set_channel_profiles(const uint8_t data[CHANNEL_PROFILE_COUNT])
{
    if (data == NULL)
        return false;
    for (uint8_t i = 0; i < CHANNEL_PROFILE_COUNT; i++) {
        if ((data[i] & 0x80) != 0)
            return false; // reserved bit must remain 0
    }

    ext_config_t* ext = storage_get_ext_config();
    ext->magic = EXT_CONFIG_MAGIC;
    ext->version = EXT_CONFIG_VERSION;
    memcpy(ext->channel_profile, data, CHANNEL_PROFILE_COUNT);

    keyboard_profile_apply_active();
    return true;
}

static bool s_usb_active = false;
static uint8_t s_ble_id = 0;
static uint8_t s_last_applied_profile = 0xFF;

void keyboard_profile_apply_active(void)
{
    ext_config_t* ext = storage_get_ext_config();
    uint8_t ch = s_usb_active ? 3 : (s_ble_id < 3 ? s_ble_id : 0);
    uint8_t prof = (ext->magic == EXT_CONFIG_MAGIC) ? (ext->channel_profile[ch] & 0x7F) : 0;

    if (prof != s_last_applied_profile) {
        clear_keyboard();
        s_last_applied_profile = prof;
    }
    keyboard_set_active_profile(prof & CH_BASE_LAYER_MASK, prof & ~CH_BASE_LAYER_MASK);
}

static void ext_config_event_handler(uint8_t code, void* arg)
{
    switch (code) {
    case USER_EVT_STAGE:
        if ((uintptr_t)arg == KBD_STATE_INITED) {
            s_ble_id = switch_device_get_id();
            keyboard_profile_apply_active();
        }
        break;
    case USER_EVT_USB: {
        uint8_t usb_st = (uint8_t)(uintptr_t)arg;
        bool next_usb = (usb_st == USB_WORKING);
        if (next_usb != s_usb_active) {
            s_usb_active = next_usb;
            keyboard_profile_apply_active();
        }
        break;
    }
    case USER_EVT_BLE_DEVICE_SWITCH:
        s_ble_id = (uint8_t)(uintptr_t)arg;
        keyboard_profile_apply_active();
        break;
    default:
        break;
    }
}

EVENT_HANDLER(ext_config_event_handler);

#endif
