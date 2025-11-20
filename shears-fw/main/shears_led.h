/**
 * @file shears_led.h
 * @brief Status LED driver for the shears node.
 *
 * This module owns:
 *  - GPIO config for the shears status LED
 *  - A FreeRTOS task that implements the blink behavior
 *  - Simple functions to switch between blink / solid / off
 */

#pragma once

#include "driver/gpio.h"
#include <stdbool.h>

/* Hardware GPIO used for the shears status LED */
#define SHEARS_STATUS_LED_GPIO GPIO_NUM_33

/**
 * @brief Initialize the status LED and start its background task.
 *
 * Call this once from app_main() before using any other shearsLed* functions.
 */
void shearsLedInit(void);

/**
 * @brief Enable or disable blinking mode.
 *
 * When enabled, the LED blinks at ~10 Hz while the background task runs.
 * When disabled, the LED stays in whatever solid state the other setters choose.
 */
void shearsLedSetBlinking(bool enable);

/**
 * @brief Force the LED to solid ON.
 *
 * Also disables blinking.
 */
void shearsLedSetSolidOn(void);

/**
 * @brief Force the LED to OFF.
 *
 * Also disables blinking.
 */
void shearsLedSetOff(void);
