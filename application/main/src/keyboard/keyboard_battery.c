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
#include <string.h>

#include "action.h"
#include "action_util.h"
#include "app_scheduler.h"
#include "keyboard_battery.h"

#include "../config/keyboard_config.h"
#include "adc_convert.h"
#include "keyboard_evt.h"

#define ADC_BUFFER_SIZE 6
struct BatteryInfo battery_info;

uint16_t adc_buffer[ADC_BUFFER_SIZE];
uint8_t adc_buffer_index;
static bool batt_initialized = false;
static uint8_t last_reported_percentage = 0;
static uint16_t last_reported_voltage = 0;

static void calculate_battery_persentage(struct BatteryInfo* info)
{
    if (info->voltage >= 4100)
        info->percentage = 100;
    else if (info->voltage >= 3335)
        info->percentage = 15 + (info->voltage - 3335) / 9;
    else if (info->voltage >= 2900)
        info->percentage = (info->voltage - 2900) / 29;
    else
        info->percentage = 0;
}

static void update_battery_metrics(void)
{
    uint32_t result = 0;
    for (uint8_t i = 0; i < ADC_BUFFER_SIZE; i++) {
        result += adc_buffer[i];
    }
    result /= ADC_BUFFER_SIZE;
    // RESULT = [V(P) - V(N)] * GAIN/REFERENCE * 2 ^ (RESOLUTION - m)
    // value  = V_in / 1.2 * 1024
    // V_in   = V_bat * 2.2 / 12.2
    battery_info.voltage = result * 1200 * 122 / 1024 / 22;
    calculate_battery_persentage(&battery_info);
}

static void battery_process_sample(uint16_t sample)
{
    if (!batt_initialized) {
        for (uint8_t i = 0; i < ADC_BUFFER_SIZE; i++) {
            adc_buffer[i] = sample;
        }
        adc_buffer_index = 0;
        batt_initialized = true;
        update_battery_metrics();
        last_reported_percentage = battery_info.percentage;
        last_reported_voltage = battery_info.voltage;
        trig_event_param(USER_EVT_BATTERY_INFO_REFRESH, battery_info.percentage);
        return;
    }

    adc_buffer[adc_buffer_index] = sample;
    adc_buffer_index = (adc_buffer_index + 1) % ADC_BUFFER_SIZE;
    update_battery_metrics();

    int32_t pct_diff = (int32_t)battery_info.percentage - (int32_t)last_reported_percentage;
    int32_t volt_diff = (int32_t)battery_info.voltage - (int32_t)last_reported_voltage;
    if (pct_diff != 0
        && (pct_diff >= 2 || pct_diff <= -2
            || volt_diff >= 12 || volt_diff <= -12
            || battery_info.percentage == 100
            || battery_info.percentage == 0)) {
        last_reported_percentage = battery_info.percentage;
        last_reported_voltage = battery_info.voltage;
        trig_event_param(USER_EVT_BATTERY_INFO_REFRESH, battery_info.percentage);
    }
}

static void battery_sample_sched_handler(void* p_event_data, uint16_t event_size)
{
    if (p_event_data == NULL || event_size != sizeof(uint16_t)) {
        return;
    }
    uint16_t sample = 0;
    memcpy(&sample, p_event_data, sizeof(sample));
    battery_process_sample(sample);
}

static void adc_result_handler(nrf_saadc_value_t value)
{
    uint16_t sample = (value > 0) ? (uint16_t)value : 0;
    (void)app_sched_event_put(&sample, sizeof(sample), battery_sample_sched_handler);
}

static nrf_saadc_channel_config_t channel_config = {
    .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
    .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
    .gain = NRF_SAADC_GAIN1_2,
    .reference = NRF_SAADC_REFERENCE_INTERNAL,
    .acq_time = NRF_SAADC_ACQTIME_10US,
    .mode = NRF_SAADC_MODE_SINGLE_ENDED,
    .burst = NRF_SAADC_BURST_DISABLED,
    .pin_p = (nrf_saadc_input_t)(BATTERY_ADC_PIN),
    .pin_n = NRF_SAADC_INPUT_DISABLED
};

void keyboard_battery_init_sample(void)
{
    nrf_saadc_value_t raw = 0;
    if (!adc_read_channel_once(&channel_config, &raw)) {
        return;
    }
    uint16_t sample = (raw > 0) ? (uint16_t)raw : 0;

    for (uint8_t i = 0; i < ADC_BUFFER_SIZE; i++) {
        adc_buffer[i] = sample;
    }
    adc_buffer_index = 0;
    batt_initialized = true;
    update_battery_metrics();
    last_reported_percentage = battery_info.percentage;
    last_reported_voltage = battery_info.voltage;
}

bool keyboard_battery_is_initialized(void)
{
    return batt_initialized;
}

static struct adc_channel_config batt_channel = {
    .adc_start = 0,
    .adc_finish = &adc_result_handler,
    .period = 10000,
    .config = &channel_config,
};

ADC_CONVERT_CHANNEL(batt_channel);

/**
 * @brief 输出电池剩余电量.
 *
 */
void print_battery_percentage()
{
    int digits[10] = { KC_0, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9 };
    char percentage = battery_info.percentage;

    if (percentage <= 0) {
        type_code(KC_0);
    } else if (percentage >= 100) {
        type_code(KC_O);
        type_code(KC_K);
    } else {
        if (percentage >= 10) {
            int index10 = (percentage / 10) % 10;
            int digit10 = digits[index10];
            type_code(digit10);
        }
        int index1 = percentage % 10;
        int digit1 = digits[index1];
        type_code(digit1);
    }
}