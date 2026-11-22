/*
Copyright (C) 2018,2019 Jim Jiang <jim@lotlab.org>

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

#include "ble_bas_service.h"
#include <stdint.h>
#include <string.h>

#include "app_timer.h"
#include "ble_bas.h"
#include "ble_config.h"
#include "ble_services.h"
#include "events.h"
#include "keyboard_battery.h"
#include "keyboard_evt.h"

#include "../config/keyboard_config.h"

#define BAS_RECONN_DELAY_FIRST_MS  1500U
#define BAS_RECONN_DELAY_SECOND_MS 2500U

BLE_BAS_DEF(m_bas); /**< Structure used to identify the battery service. */
APP_TIMER_DEF(m_bas_reconn_timer);
static bool bas_ready = false;
static uint8_t s_bas_reconn_retries = 0;

static bool bas_error_ignorable(ret_code_t err_code)
{
    return (err_code == NRF_SUCCESS)
        || (err_code == NRF_ERROR_BUSY)
        || (err_code == NRF_ERROR_RESOURCES)
        || (err_code == NRF_ERROR_FORBIDDEN)
        || (err_code == NRF_ERROR_INVALID_STATE)
        || (err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING)
        || (err_code == BLE_ERROR_INVALID_CONN_HANDLE);
}

/**@brief Function for performing a battery measurement, and update the Battery Level characteristic in the Battery Service.
 */
static void battery_level_update(uint8_t battery_level)
{
    ret_code_t err_code;

    if (battery_level > 100U) {
        battery_level = 100U;
    }

    err_code = ble_bas_battery_level_update(&m_bas, battery_level, BLE_CONN_HANDLE_ALL);
    if (!bas_error_ignorable(err_code)) {
        APP_ERROR_HANDLER(err_code);
    }
}

static void battery_level_notify_reconnection(uint16_t conn_handle)
{
    if (conn_handle == BLE_CONN_HANDLE_INVALID) {
        return;
    }

    ret_code_t err_code = ble_bas_battery_lvl_on_reconnection_update(&m_bas, conn_handle);
    if (!bas_error_ignorable(err_code)) {
        APP_ERROR_HANDLER(err_code);
    }
}

static void battery_level_push(uint16_t conn_handle)
{
    if (!bas_ready || conn_handle == BLE_CONN_HANDLE_INVALID) {
        return;
    }

    uint8_t level = battery_info.percentage;
    if (level > 100U) {
        level = 100U;
    }

    if (level != m_bas.battery_level_last) {
        battery_level_update(level);
    } else {
        battery_level_notify_reconnection(conn_handle);
    }
}

static void bas_reconn_schedule(void)
{
    if (!bas_ready) {
        return;
    }
    (void)app_timer_stop(m_bas_reconn_timer);
    s_bas_reconn_retries = 1U;
    (void)app_timer_start(m_bas_reconn_timer, APP_TIMER_TICKS(BAS_RECONN_DELAY_FIRST_MS), NULL);
}

static void bas_reconn_timer_handler(void* p_context)
{
    UNUSED_PARAMETER(p_context);
    if (!bas_ready || m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        s_bas_reconn_retries = 0;
        return;
    }

    battery_level_push(m_conn_handle);

    if (s_bas_reconn_retries > 0U) {
        s_bas_reconn_retries--;
        (void)app_timer_start(m_bas_reconn_timer, APP_TIMER_TICKS(BAS_RECONN_DELAY_SECOND_MS), NULL);
    }
}

static void on_bas_evt(ble_bas_t* p_bas, ble_bas_evt_t* p_evt)
{
    UNUSED_PARAMETER(p_bas);
    if (p_evt == NULL) {
        return;
    }

    if (p_evt->evt_type == BLE_BAS_EVT_NOTIFICATION_ENABLED) {
        battery_level_push(p_evt->conn_handle);
    }
}

/**@brief Function for initializing Battery Service.
 */
void battery_service_init(void)
{
    ret_code_t err_code;
    ble_bas_init_t bas_init_obj;

    memset(&bas_init_obj, 0, sizeof(bas_init_obj));

    uint8_t initial_level = battery_info.percentage;
    if (initial_level > 100U) {
        initial_level = 100U;
    }

    bas_init_obj.evt_handler = on_bas_evt;
    bas_init_obj.support_notification = true;
    bas_init_obj.p_report_ref = NULL;
    bas_init_obj.initial_batt_level = initial_level;

    bas_init_obj.bl_rd_sec = SEC_CURRENT;
    bas_init_obj.bl_cccd_wr_sec = SEC_CURRENT;
    bas_init_obj.bl_report_rd_sec = SEC_CURRENT;

    err_code = ble_bas_init(&m_bas, &bas_init_obj);
    APP_ERROR_CHECK(err_code);
    m_bas.battery_level_last = initial_level;

    err_code = app_timer_create(&m_bas_reconn_timer, APP_TIMER_MODE_SINGLE_SHOT, bas_reconn_timer_handler);
    APP_ERROR_CHECK(err_code);

    bas_ready = true;
}

void battery_service_on_db_cache_applied(uint16_t conn_handle)
{
    if (!bas_ready || conn_handle == BLE_CONN_HANDLE_INVALID) {
        return;
    }
    battery_level_push(conn_handle);
    bas_reconn_schedule();
}

static void battery_service_event_handler(enum user_event event, void* arg)
{
    if (!bas_ready) {
        return;
    }
    switch (event) {
    case USER_EVT_BATTERY_INFO_REFRESH:
        battery_level_update((uint8_t)(uint32_t)(uintptr_t)arg);
        break;
    case USER_EVT_BLE_STATE_CHANGE: {
        uint32_t ble_state = (uint32_t)(uintptr_t)arg;
        if (ble_state == BLE_STATE_CONNECTED && m_conn_handle != BLE_CONN_HANDLE_INVALID) {
            battery_level_push(m_conn_handle);
            bas_reconn_schedule();
        } else if (ble_state == BLE_STATE_DISCONNECT || ble_state == BLE_STATE_IDLE) {
            (void)app_timer_stop(m_bas_reconn_timer);
            s_bas_reconn_retries = 0;
        }
        break;
    }
    default:
        break;
    }
}

EVENT_HANDLER(battery_service_event_handler);
