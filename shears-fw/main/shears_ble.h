/*
 * shears_ble.h
 *
 * BLE peripheral interface for the shears node.
 *
 * Provides a small public API for initializing the BLE stack and reporting
 * connection state changes to the application. LED behavior stays in the
 * application layer (e.g. app_main driving shearsLed*()).
 */

#pragma once

#include <stdbool.h>

/* --- Public API ----------------------------------------------------------- */

/* Application callback for BLE connection state changes. */
typedef void (*shearsBleConnCallback_t)(bool connected);

/* Initializes BLE peripheral mode and starts advertising. */
void shearsBleInit(shearsBleConnCallback_t cb);
