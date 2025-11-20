/**
 * @file base_ble.h
 * @brief BLE central module for the base station.
 *
 * Handles:
 *  - NimBLE initialization
 *  - Device scanning
 *  - Filtering for WM-SHEARS advertisements
 *  - Auto-connect and reconnect logic
 *  - Reporting connection state back to the app layer
 */

#pragma once

#include <stdbool.h>

/**
 * @brief Callback type used by the base app to receive BLE link state changes.
 *
 * @param connected true when link to shears established, false when lost
 */
typedef void (*bleBaseConnCallback_t)(bool connected);

/**
 * @brief Initialize BLE controller + host and start scanning.
 *
 * @param cb Application callback invoked whenever connection state changes.
 */
void bleBaseInit(bleBaseConnCallback_t cb);
