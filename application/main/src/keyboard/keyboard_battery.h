#pragma once
#include <stdbool.h>
#include <stdint.h>

struct BatteryInfo {
    uint16_t voltage; // Voltage (mV)
    uint8_t percentage; // Battery percentage (0..100)
};

/**
 * Battery info
 */
extern struct BatteryInfo battery_info;

/**@brief Perform one synchronous boot SAADC measurement and prime the rolling filter. */
void keyboard_battery_init_sample(void);

/**@brief Return whether boot battery sampling has populated battery_info. */
bool keyboard_battery_is_initialized(void);

/**@brief 输出电池剩余电量.
 *
 */
void print_battery_percentage();
