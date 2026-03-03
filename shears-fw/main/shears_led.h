/*
 * shears_led.h
 *
 * Status LED interface for the shears node.
 *
 * Provides GPIO configuration and a small control API for driving the
 * shears status LED. Blink timing is handled internally by a background
 * FreeRTOS task.
 */

#pragma once

#include "driver/gpio.h"
#include <stdbool.h>

/* GPIO used for the shears status LED. */
#define SHEARS_STATUS_LED_GPIO GPIO_NUM_33

/* --- Public API ----------------------------------------------------------- */

/* Initializes the status LED and starts the background blink task. */
void shearsLedInit(void);

/* Enables or disables blinking mode. */
void shearsLedSetBlinking(bool enable);

/* Forces the LED to a solid ON state and disables blinking. */
void shearsLedSetSolidOn(void);

/* Forces the LED to OFF and disables blinking. */
void shearsLedSetOff(void);
