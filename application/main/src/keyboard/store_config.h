#pragma once
#include "config.h"
#include "data_storage.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef CONFIG_STORAGE
uint16_t get_slow_scan_timeout(void);
uint16_t get_sleep_timeout(void);
uint16_t get_led_powersave_timeout(void);
void store_config_load_defaults(void);

bool is_valid_utf8_device_name(const uint8_t* data, uint8_t len);
uint8_t store_config_get_ble_name(uint8_t* out_buf, uint8_t max_len);
bool store_config_set_ble_name(const uint8_t* data, uint8_t len, bool* overflow_err);
void store_config_get_channel_profiles(uint8_t out[CHANNEL_PROFILE_COUNT]);
bool store_config_set_channel_profiles(const uint8_t data[CHANNEL_PROFILE_COUNT]);
void store_config_get_led_colors(uint8_t out[LED_COLOR_COUNT * 3]);
bool store_config_set_led_colors(const uint8_t data[LED_COLOR_COUNT * 3]);
uint32_t store_config_get_led_color(uint8_t slot);
void keyboard_profile_apply_active(void);
#else
#define get_slow_scan_timeout() (SLEEP_SLOW_TIMEOUT)
#define get_sleep_timeout() (SLEEP_OFF_TIMEOUT)
#define get_led_powersave_timeout() (LED_AUTOOFF_TIME)
#define store_config_load_defaults()
#define store_config_get_ble_name(out_buf, max_len) (0)
#define store_config_get_led_colors(out) ((void)(out))
#define store_config_set_led_colors(data) (false)
static inline uint32_t store_config_get_led_color(uint8_t slot)
{
    static const uint32_t defaults[LED_COLOR_COUNT] = {
        0x00FFFF, 0x4B00FF, 0xFFD700, 0xFFFFFF, 0x0099FF,
        0xFF8000, 0x00FF00, 0xFFFF00, 0xFF0080, 0xFF00FF,
    };
    return (slot < LED_COLOR_COUNT) ? defaults[slot] : 0xFFFFFF;
}
#define keyboard_profile_apply_active()
#endif
