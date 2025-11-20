/**
 * @file shears_ble.h
 * @brief BLE peripheral (shears) interface.
 *
 * This module:
 *  - Initializes NimBLE for the shears node
 *  - Advertises under the name "WM-SHEARS"
 *  - Exposes a callback for connection state changes
 *
 * It does NOT touch any LEDs directly. The app layer decides what
 * to do when the connection state changes (e.g. call shearsLed*).
 */

#pragma once

#include <stdbool.h>

/**
 * @brief Callback type used by the app when BLE connection changes.
 *
 * @param connected true when a central is connected, false when disconnected.
 */
typedef void (*shearsBleConnCallback_t)(bool connected);

/**
 * @brief Initialize BLE peripheral stack and start advertising.
 *
 * @param cb Callback invoked whenever a central connects or disconnects.
 */
void shearsBleInit(shearsBleConnCallback_t cb);
