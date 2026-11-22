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

#include "app_scheduler.h"
#include "app_timer.h"
#include "ble_keyboard.h"
#include "ble_services.h"
#include "keyboard_evt.h"
#include "keyboard_led.h"
#include "nrf_delay.h"
#include "simple_rgb/led_rgb.h"

#define COLOR_IDLE 0xFFFFFF // 白色: 空闲状态
#define COLOR_BLE 0x66FFFF // 青色: 蓝牙连接
#define COLOR_CHARGING 0xFF8000 // 橙色: 充电中
#define COLOR_FULL 0x00FF00 // 绿色: 充电完毕
#define COLOR_USB 0x0099ff // 蓝色: USB 已连接
#define COLOR_PASSKEY_REQ 0xFFFF00 // 黄色: 请求输入配对码
#define COLOR_PASSKEY_SEND 0xFF0080 // 粉红色: 配对码输入完毕
#define COLOR_SLEEP 0xFF00FF // 紫红色: 睡眠

#define BLE_LED_BLINK_ON_MS 400U
#define BLE_LED_BLINK_OFF_MS 400U
#define BLE_LED_REPEAT_PAUSE_MS 2000U

enum ble_led_phase {
    BLE_LED_IDLE = 0,
    BLE_LED_BLINK_ON,
    BLE_LED_BLINK_OFF,
    BLE_LED_REPEAT_PAUSE,
};

APP_TIMER_DEF(m_ble_channel_led_timer);

#ifdef PIN_CHARGING
static bool is_charging = false;
static bool charging_full = false;
#endif
static bool ble_connected = false;
static bool usb_working = false;
static bool leds_inited = false;
static bool s_led_timer_created = false;
static bool s_ble_adv_active = false;
static enum ble_led_phase s_ble_led_phase = BLE_LED_IDLE;
static uint8_t s_blinks_left = 0;
static uint8_t s_blink_channel = 0xFF;

/**
 * @brief 按状态更改 LED 样式
 * 
 */
static void led_status_change()
{
    if (usb_working) {
        keyboard_led_rgb_set(COLOR_USB);
        keyboard_led_rgb_switch(true);
#ifdef PIN_CHARGING
    } else if (is_charging) {
        keyboard_led_rgb_set(charging_full ? COLOR_FULL : COLOR_CHARGING);
        keyboard_led_rgb_switch(true);
#endif
    } else {
        keyboard_led_rgb_set(ble_connected ? COLOR_BLE : COLOR_IDLE);
        keyboard_led_rgb_switch(true);
    }
}

static uint8_t current_ble_channel(void)
{
#ifdef MULTI_DEVICE_SWITCH
    uint8_t ch = switch_device_get_id();
    return (ch < 3) ? ch : 0U;
#else
    return 0U;
#endif
}

static void ble_channel_led_stop(void)
{
    if (s_led_timer_created) {
        (void)app_timer_stop(m_ble_channel_led_timer);
    }
    s_ble_led_phase = BLE_LED_IDLE;
    s_blinks_left = 0;
    s_blink_channel = 0xFF;
}

static void ble_channel_led_start(void)
{
    if (!leds_inited || !s_led_timer_created || usb_working) {
        ble_channel_led_stop();
        return;
    }

    uint8_t ch = current_ble_channel();
    ble_channel_led_stop();
    s_blink_channel = ch;
    s_blinks_left = (uint8_t)(ch + 1U);
    s_ble_led_phase = BLE_LED_BLINK_ON;
    keyboard_led_rgb_set_raw(COLOR_BLE);
    (void)app_timer_start(m_ble_channel_led_timer, APP_TIMER_TICKS(BLE_LED_BLINK_ON_MS), NULL);
}

static void ble_channel_led_step(void* p_data, uint16_t len)
{
    UNUSED_PARAMETER(p_data);
    UNUSED_PARAMETER(len);

    if (!leds_inited || usb_working || s_ble_led_phase == BLE_LED_IDLE) {
        ble_channel_led_stop();
        return;
    }

    switch (s_ble_led_phase) {
    case BLE_LED_BLINK_ON:
        keyboard_led_rgb_set_raw(0);
        if (s_blinks_left > 0) {
            s_blinks_left--;
        }
        if (s_blinks_left > 0 || ble_connected) {
            s_ble_led_phase = BLE_LED_BLINK_OFF;
            (void)app_timer_start(m_ble_channel_led_timer, APP_TIMER_TICKS(BLE_LED_BLINK_OFF_MS), NULL);
        } else if (s_ble_adv_active) {
            s_ble_led_phase = BLE_LED_REPEAT_PAUSE;
            (void)app_timer_start(m_ble_channel_led_timer, APP_TIMER_TICKS(BLE_LED_REPEAT_PAUSE_MS), NULL);
        } else {
            s_ble_led_phase = BLE_LED_IDLE;
            s_blink_channel = 0xFF;
        }
        break;

    case BLE_LED_BLINK_OFF:
        if (s_blinks_left > 0) {
            s_ble_led_phase = BLE_LED_BLINK_ON;
            keyboard_led_rgb_set_raw(COLOR_BLE);
            (void)app_timer_start(m_ble_channel_led_timer, APP_TIMER_TICKS(BLE_LED_BLINK_ON_MS), NULL);
        } else if (ble_connected) {
            s_ble_led_phase = BLE_LED_IDLE;
            led_status_change();
        } else if (s_ble_adv_active) {
            s_ble_led_phase = BLE_LED_REPEAT_PAUSE;
            keyboard_led_rgb_set_raw(0);
            (void)app_timer_start(m_ble_channel_led_timer, APP_TIMER_TICKS(BLE_LED_REPEAT_PAUSE_MS), NULL);
        } else {
            s_ble_led_phase = BLE_LED_IDLE;
            keyboard_led_rgb_set_raw(0);
        }
        break;

    case BLE_LED_REPEAT_PAUSE:
        if (!ble_connected && s_ble_adv_active) {
            ble_channel_led_start();
        } else if (ble_connected) {
            s_ble_led_phase = BLE_LED_IDLE;
            led_status_change();
        } else {
            s_ble_led_phase = BLE_LED_IDLE;
            keyboard_led_rgb_set_raw(0);
        }
        break;

    default:
        ble_channel_led_stop();
        break;
    }
}

static void ble_channel_led_timer_handler(void* p_context)
{
    UNUSED_PARAMETER(p_context);
    (void)app_sched_event_put(NULL, 0, ble_channel_led_step);
}

static void ble_channel_led_timer_init(void)
{
    if (!s_led_timer_created) {
        ret_code_t err_code = app_timer_create(&m_ble_channel_led_timer,
            APP_TIMER_MODE_SINGLE_SHOT,
            ble_channel_led_timer_handler);
        APP_ERROR_CHECK(err_code);
        s_led_timer_created = true;
    }
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
            ble_channel_led_timer_init();
            break;
        case KBD_STATE_INITED: // 初始化完毕
            if (!usb_working && !ble_connected) {
                s_ble_adv_active = true;
                if (s_ble_led_phase == BLE_LED_IDLE) {
                    ble_channel_led_start();
                }
            } else {
                led_status_change();
            }
            break;
        case KBD_STATE_SLEEP: // 准备休眠
            if (leds_inited) {
                ble_channel_led_stop();
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
        if (s_ble_led_phase != BLE_LED_IDLE) {
            break;
        }
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
        default:
            break;
        }
        if (s_ble_led_phase == BLE_LED_IDLE) {
            led_status_change();
        }
        break;
#endif
    case USER_EVT_USB: // USB事件
        switch (arg2) {
        case USB_WORKING:
            usb_working = true;
            ble_channel_led_stop();
            led_status_change();
            break;
        default:
            usb_working = false;
            if (leds_inited && !ble_connected && s_ble_adv_active) {
                ble_channel_led_start();
            } else {
                led_status_change();
            }
            break;
        }
        break;
    case USER_EVT_BLE_STATE_CHANGE: // 蓝牙事件
        switch (arg2) {
        case BLE_STATE_CONNECTED:
            ble_connected = true;
            s_ble_adv_active = false;
            if (!usb_working
                && (s_ble_led_phase == BLE_LED_BLINK_ON || s_ble_led_phase == BLE_LED_BLINK_OFF)) {
                // Let the active channel blink burst finish before solid ON + timeout.
                break;
            }
            ble_channel_led_stop();
            led_status_change();
            break;
        case BLE_STATE_FAST_ADV:
        case BLE_STATE_SLOW_ADV:
        case BLE_STATE_DISCONNECT:
            ble_connected = false;
            s_ble_adv_active = true;
            if (!usb_working && leds_inited && s_ble_led_phase == BLE_LED_IDLE) {
                ble_channel_led_start();
            }
            break;
        case BLE_STATE_IDLE:
            s_ble_adv_active = false;
            if (!usb_working && !ble_connected) {
                ble_channel_led_stop();
                keyboard_led_rgb_set_raw(0);
            }
            break;
        default:
            break;
        }
        break;
    case USER_EVT_BLE_DEVICE_SWITCH: { // 设备切换事件
        uint8_t ch = current_ble_channel();
        s_ble_adv_active = true;
        if (!usb_working && leds_inited) {
            if (s_ble_led_phase == BLE_LED_IDLE
                || s_ble_led_phase == BLE_LED_REPEAT_PAUSE
                || s_blink_channel != ch) {
                ble_channel_led_start();
            }
        } else {
            led_status_change();
        }
        break;
    }
    case USER_EVT_BLE_PASSKEY_STATE: // 请求Passkey
        ble_channel_led_stop();
        switch (arg2) {
        case PASSKEY_STATE_REQUIRE:
            keyboard_led_rgb_set(COLOR_PASSKEY_REQ);
            keyboard_led_rgb_switch(true);
            break;
        case PASSKEY_STATE_SEND:
            keyboard_led_rgb_set(COLOR_PASSKEY_SEND);
            keyboard_led_rgb_switch(true);
            break;
        default:
            break;
        }
        break;
    case USER_EVT_SLEEP: // 睡眠指示
        ble_channel_led_stop();
        keyboard_led_rgb_direct(COLOR_SLEEP);
        break;
    default:
        break;
    }
}

EVENT_HANDLER(rgb_led_event_handler);