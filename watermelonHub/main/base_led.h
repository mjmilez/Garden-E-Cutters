/**
 * @file base_led.h
 * @brief Status LED driver for the base station.
 *
 * This module isolates all LED-related behavior:
 *  - GPIO configuration
 *  - Dedicated FreeRTOS blinking task
 *  - Functions to switch between blinking/solid/off
 *
 * The BLE module calls baseLedSetBlinking()/SolidOn() depending on the link state.
 */

#pragma once

#include "driver/gpio.h"
#include <stdbool.h>

/* Hardware GPIO used for base station status LED */
#define BASE_STATUS_LED_GPIO GPIO_NUM_33

/* Public API */
void baseLedInit(void);
void baseLedSetBlinking(bool enable);
void baseLedSetSolidOn(void);
void baseLedSetOff(void);
