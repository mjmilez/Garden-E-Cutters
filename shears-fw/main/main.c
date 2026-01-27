/*
 * main.c
 *
 * Top-level entry point for the shears firmware.
 *
 * Startup sequence:
 *   - initialize the status LED subsystem
 *   - start the GPS logger (SPIFFS, UART, button ISR, background tasks)
 *   - start BLE in peripheral mode and advertise as "WM-SHEARS"
 *
 * Ongoing work (GPS reads, save requests, BLE activity, log transfers)
 * runs inside module-specific FreeRTOS tasks.
 */

#include "shears_led.h"
#include "shears_ble.h"
#include "gps_logger.h"

/* --- BLE connection state ------------------------------------------------- */

/* Connection callback used by the BLE layer to drive LED state. */
static void bleConnChanged(bool connected)
{
	if (connected) {
		/* Connected to the base: solid LED. */
		shearsLedSetSolidOn();
	} else {
		/* Not connected: blink while advertising. */
		shearsLedSetOff();
		shearsLedSetBlinking(true);
	}
}

/* --- Entry point ---------------------------------------------------------- */

void app_main(void)
{
	/* Initialize status LED and indicate idle/advertising state. */
	shearsLedInit();
	shearsLedSetBlinking(true);

	/* Start GPS logging subsystem. */
	gpsLoggerInit();

	/* Start BLE peripheral mode and begin advertising. */
	shearsBleInit(bleConnChanged);

	/* Foreground remains idle; work is handled by background tasks. */
}
