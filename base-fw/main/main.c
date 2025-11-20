/**
 * @file main.c
 * @brief High-level entry point for the base station.
 *
 * Responsibilities:
 *   - Initialize the status LED subsystem
 *   - Initialize BLE central logic
 *   - React to BLE connection events and update LED state
 *
 * This file intentionally contains NO BLE logic and NO hardware details.
 * All complexity is contained in base_ble.c and base_led.c.
 */

#include "base_led.h"
#include "base_ble.h"

/**
 * @brief Called whenever BLE connection state changes.
 *
 * @param connected true if connected to shears, false otherwise
 */
static void bleConnChanged(bool connected) {
    if (connected) {
        /* Solid LED → connected */
        baseLedSetSolidOn();
    } else {
        /* Blinking → scanning or reconnecting */
        baseLedSetBlinking(true);
    }
}

void app_main(void) {
    /* Initialize LED + start blinking immediately */
    baseLedInit();
    baseLedSetBlinking(true);

    /* Initialize BLE stack (async), scanning begins automatically */
    bleBaseInit(bleConnChanged);

    /* Nothing else to do — BLE and LED run in background tasks */
}
