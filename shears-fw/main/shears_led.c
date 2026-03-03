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
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}

/* --- Public API ----------------------------------------------------------- */

void shearsLedInit(void)
{
	/* Configure the status LED GPIO as an output. */
	gpio_config_t io_conf = {
		.pin_bit_mask = 1ULL << SHEARS_STATUS_LED_GPIO,
		.mode         = GPIO_MODE_OUTPUT,
		.pull_up_en   = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type    = GPIO_INTR_DISABLE
	};
	gpio_config(&io_conf);

	/* Default to LED OFF at startup. */
	gpio_set_level(SHEARS_STATUS_LED_GPIO, 0);

	/* Start the background LED task. */
	xTaskCreate(ledTask,
	            "shears_led",
	            2048,
	            NULL,
	            5,
	            &ledTaskHandle);
}

void shearsLedSetBlinking(bool enable)
{
	ledBlinking = enable;

	/* Solid ON/OFF state is controlled explicitly by the caller. */
}

void shearsLedSetSolidOn(void)
{
	ledBlinking = false;
	gpio_set_level(SHEARS_STATUS_LED_GPIO, 1);
}

void shearsLedSetOff(void)
{
	ledBlinking = false;
	gpio_set_level(SHEARS_STATUS_LED_GPIO, 0);
}
