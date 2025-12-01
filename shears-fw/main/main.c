/*
 * Top-level entry point for the shears firmware.
 *
 * Responsibilities:
 *   - bring up the status LED subsystem
 *   - start the GPS logger (SPIFFS, UART, button ISR, background tasks)
 *   - start BLE peripheral mode and begin advertising as "WM-SHEARS"
 *
 * All ongoing work (GPS reads, save requests, BLE reads/writes, log transfers)
 * runs inside module-specific FreeRTOS tasks.
 */

#include "shears_led.h"
#include "shears_ble.h"
#include "gps_logger.h"

/*
 * BLE connection callback from shearsBleInit().
 * Controls the LED to show link state to the base.
 */
static void bleConnChanged(bool connected)
{
	if (connected) {
		/* Connected to base → solid LED. */
		shearsLedSetSolidOn();
	} else {
		/* Not connected → blink while advertising. */
		shearsLedSetOff();
		shearsLedSetBlinking(true);
	}
}

void app_main(void)
{
	/* Bring up LED status system; blink while waiting for connection. */
	shearsLedInit();
	shearsLedSetBlinking(true);

	/* Bring up GPS logging subsystem (SPIFFS, UART, button, tasks). */
	gpsLoggerInit();

	/* Bring up BLE peripheral mode and begin advertising as WM-SHEARS. */
	shearsBleInit(bleConnChanged);

	/* Foreground is idle; all modules run in their own FreeRTOS tasks. */
}
