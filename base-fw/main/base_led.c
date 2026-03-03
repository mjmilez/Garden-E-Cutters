/*
 * base_led.c
 *
 * Status LED control for the base station.
 *
 * The LED exposes two visible states:
 *   - Fast blink (100 ms on / 100 ms off): scanning or attempting to connect
 *   - Solid ON: connected to the shears
 *
 * Blinking is handled in a dedicated FreeRTOS task so LED timing
 * does not block BLE or application logic.
 */

#include "base_led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

/* Task handle for the LED background task. */
static TaskHandle_t ledTaskHandle = NULL;

/* Current LED mode: blinking (true) or steady (false). */
static volatile bool ledBlinking = false;

/* --- LED task ------------------------------------------------------------- */

/* Background task that drives the LED state. */
static void ledTask(void *arg)
{
	(void)arg;

	while (1) {
		if (ledBlinking) {
			/* LED ON phase. */
			gpio_set_level(BASE_STATUS_LED_GPIO, 1);
			vTaskDelay(pdMS_TO_TICKS(100));

			/* Re-check state in case blinking was disabled mid-cycle. */
			if (!ledBlinking) {
				continue;
			}

			/* LED OFF phase. */
			gpio_set_level(BASE_STATUS_LED_GPIO, 0);
			vTaskDelay(pdMS_TO_TICKS(100));
		} else {
			/* Idle delay when not blinking. */
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
}

/* --- Public API ----------------------------------------------------------- */

void baseLedInit(void)
{
	/* Configure the status LED GPIO as output. */
	gpio_config_t io_conf = {
		.pin_bit_mask = 1ULL << BASE_STATUS_LED_GPIO,
		.mode         = GPIO_MODE_OUTPUT,
		.pull_up_en   = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type    = GPIO_INTR_DISABLE
	};

	gpio_config(&io_conf);

	/* Default LED state on boot is OFF. */
	gpio_set_level(BASE_STATUS_LED_GPIO, 0);

	/* Start the LED background task. */
	xTaskCreate(ledTask,
	            "base_led",
	            2048,
	            NULL,
	            5,
	            &ledTaskHandle);
}

void baseLedSetBlinking(bool enable)
{
	ledBlinking = enable;

	if (!enable) {
		/* When blinking is disabled, default to solid ON. */
		gpio_set_level(BASE_STATUS_LED_GPIO, 1);
	}
}

void baseLedSetSolidOn(void)
{
	ledBlinking = false;
	gpio_set_level(BASE_STATUS_LED_GPIO, 1);
}

void baseLedSetOff(void)
{
	ledBlinking = false;
	gpio_set_level(BASE_STATUS_LED_GPIO, 0);
}
