/*
Copyright (C) 2019 Jim Jiang <jim@lotlab.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

#include "ble_keyboard.h"
#include "ble_services.h"
#include "data_storage.h"
#include "keyboard_evt.h"
#include "keyboard_led.h"
#include "nrf_delay.h"
#include "simple_rgb/led_rgb.h"
#include "store_config.h"

#ifdef PIN_CHARGING
static bool is_charging = false;
static bool charging_full = false;
#endif
static bool ble_connected = false;
static bool usb_working = false;
static bool leds_inited = false;

static uint8_t current_ble_channel(void)
{
#ifdef MULTI_DEVICE_SWITCH
    uint8_t ch = switch_device_get_id();
    return (ch < 3) ? ch : 0U;
#else
    return 0U;
#endif
}

static uint32_t active_ble_color(void)
{
    return store_config_get_led_color((uint8_t)(LED_SLOT_BLE0 + current_ble_channel()));
}

/**
 * @brief Update RGB LED color for the active connection / charging state.
 */
static void led_status_change(void)
{
    if (usb_working) {
        keyboard_led_rgb_set(store_config_get_led_color(LED_SLOT_USB));
#ifdef PIN_CHARGING
    } else if (is_charging) {
        keyboard_led_rgb_set(store_config_get_led_color(charging_full ? LED_SLOT_FULL : LED_SLOT_CHARGING));
#endif
    } else {
        keyboard_led_rgb_set(ble_connected ? active_ble_color() : store_config_get_led_color(LED_SLOT_IDLE));
    }
    keyboard_led_rgb_switch(true);
}

void rgb_led_event_handler(enum user_event event, void* arg)
{
    uint8_t arg2 = (uint32_t)arg;
    switch (event) {
    case USER_EVT_STAGE:
        switch (arg2) {
        case KBD_STATE_POST_INIT: // 初始化LED
            keyboard_led_rgb_init();
            leds_inited = true;
            break;
        case KBD_STATE_INITED: // 初始化完毕
            led_status_change();
            break;
        case KBD_STATE_SLEEP: // 准备休眠
            if (leds_inited) {
                nrf_delay_ms(20); // 等待睡眠指示灯显示完毕
                keyboard_led_rgb_deinit();
                leds_inited = false;
            }
            break;
        default:
            break;
        }
        break;
    case USER_EVT_POWERSAVE:
        switch (arg2) {
        case PWR_SAVE_ENTER: // 进入省电模式
            keyboard_led_rgb_switch(false);
            break;
        case PWR_SAVE_EXIT: // 退出省电模式
            keyboard_led_rgb_switch(true);
            break;
        default:
            break;
        }
        break;
#ifdef PIN_CHARGING
    case USER_EVT_CHARGE: // 充电事件
        switch (arg2) {
        case BATT_NOT_CHARGING:
            is_charging = false;
            break;
        case BATT_CHARGING:
            is_charging = true;
            charging_full = false;
            break;
        case BATT_CHARGED:
            is_charging = true;
            charging_full = true;
            break;
        default:
            break;
        }
        led_status_change();
        break;
#endif
    case USER_EVT_USB: // USB事件
        usb_working = (arg2 == USB_WORKING);
        led_status_change();
        break;
    case USER_EVT_BLE_STATE_CHANGE: // 蓝牙事件
        switch (arg2) {
        case BLE_STATE_CONNECTED:
            ble_connected = true;
            led_status_change();
            break;
        case BLE_STATE_DISCONNECT:
            ble_connected = false;
            led_status_change();
            break;
        default:
            break;
        }
        break;
    case USER_EVT_BLE_DEVICE_SWITCH: // Show selected BLE channel color immediately
        if (!usb_working && leds_inited) {
            keyboard_led_rgb_set(active_ble_color());
            keyboard_led_rgb_switch(true);
        } else {
            led_status_change();
        }
        break;
    case USER_EVT_BLE_PASSKEY_STATE: // 请求Passkey
        switch (arg2) {
        case PASSKEY_STATE_REQUIRE:
            keyboard_led_rgb_set(store_config_get_led_color(LED_SLOT_PASSKEY_REQ));
            keyboard_led_rgb_switch(true);
            break;
        case PASSKEY_STATE_SEND:
            keyboard_led_rgb_set(store_config_get_led_color(LED_SLOT_PASSKEY_SEND));
            keyboard_led_rgb_switch(true);
            break;
        default:
            break;
        }
        break;
    case USER_EVT_SLEEP: // 睡眠指示
        keyboard_led_rgb_direct(store_config_get_led_color(LED_SLOT_SLEEP));
        break;
    case USER_EVT_LED:
        if (leds_inited) {
            led_status_change();
        }
        break;
    default:
        break;
    }
}

EVENT_HANDLER(rgb_led_event_handler);