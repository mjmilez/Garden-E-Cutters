/*
 * shears_led.c
 *
 * Status LED driver for the shears node.
 *
 * LED behavior:
 *   - blink while advertising / waiting for a BLE connection
 *   - solid ON while connected to the base
 *
 * Blinking is implemented in a dedicated FreeRTOS task so LED timing
 * never blocks BLE or sensor-related work.
 */

#include "shears_led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

/* Handle for the LED task (kept for possible future control). */
static TaskHandle_t ledTaskHandle = NULL;

/* When true, the LED blinks. When false, the LED remains in a solid state. */
static volatile bool ledBlinking = false;

/* --- LED task ------------------------------------------------------------- */

static void ledTask(void *arg)
{
	(void)arg;

	while (1) {
		if (ledBlinking) {
			/* LED ON */
			gpio_set_level(SHEARS_STATUS_LED_GPIO, 1);
			vTaskDelay(pdMS_TO_TICKS(100));

			/* Re-check before turning OFF so a recent state change
			 * (e.g. connection established) is not overridden.
			 */
			if (!ledBlinking) {
				continue;
			}

			/* LED OFF */
			gpio_set_level(SHEARS_STATUS_LED_GPIO, 0);
			vTaskDelay(pdMS_TO_TICKS(100));
		} else {
			/* Idle briefly and poll the blink flag again. */
			vTaskDelay(pdMS_TO_TICKS(50)
