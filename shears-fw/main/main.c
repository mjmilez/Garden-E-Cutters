/**
 * @file main.c
 * @brief High-level entry point for the shears firmware.
 *
 * This file keeps the logic simple:
 *   - Initialize the status LED subsystem
 *   - Initialize BLE peripheral behavior
 *   - Update LED state whenever the BLE link comes up or goes down
 *
 * GPS / sensors / SPIFFS can be layered on later without touching BLE details.
 */

#include "shears_led.h"
#include "shears_ble.h"

/**
 * @brief BLE connection state callback.
 *
 * Invoked by shearsBleInit() whenever a central connects or disconnects.
 */
static void bleConnChanged(bool connected) {
    if (connected) {
        /* Solid LED when the shears are connected to the base */
        shearsLedSetSolidOn();
    } else {
        /* Blink while advertising / waiting for a connection */
        shearsLedSetOff();          /* ensure it starts from off */
        shearsLedSetBlinking(true);
    }
}

void app_main(void) {
    /* Initialize the status LED module and start its blink task */
    shearsLedInit();

    /* Start in "looking for base" state: blink LED */
    shearsLedSetBlinking(true);

    /* Initialize BLE peripheral logic (advertising begins from onSync) */
    shearsBleInit(bleConnChanged);

    /* No foreground work here: BLE and LED are handled in their own tasks */
}
