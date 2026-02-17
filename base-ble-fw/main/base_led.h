/*
 * base_led.h
 *
 * Status LED interface for the base station.
 *
 * This module centralizes all LED-related behavior:
 *   - GPIO configuration
 *   - Background blinking task
 *   - Simple helpers for blink / solid / off states
 *
 * Higher-level modules (e.g. BLE) drive the LED state
 * based on connection status.
 */

#pragma once

#include "driver/gpio.h"
#include <stdbool.h>

/* GPIO used for the base station status LED. */
#define BASE_STATUS_LED_GPIO GPIO_NUM_33

/* --- Public API ----------------------------------------------------------- */

void baseLedInit(void);
void baseLedSetBlinking(bool enable);
void baseLedSetSolidOn(void);
void baseLedSetOff(void);