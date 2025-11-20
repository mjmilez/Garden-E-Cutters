/**
 * @file shears_led.c
 * @brief Implements status LED behavior for the shears node.
 *
 * Policy:
 *  - Blink quickly while advertising / waiting for a connection
 *  - Solid ON while a BLE link to the base is active
 *
 * The blinking itself is handled in a dedicated FreeRTOS task so it
 * never blocks BLE or sensor work.
 */

#include "shears_led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

/* Background task handle (not strictly needed yet, kept for future control) */
static TaskHandle_t ledTaskHandle = NULL;

/* When true, LED blinks. When false, LED stays in its last solid state. */
static volatile bool ledBlinking = false;

/**
 * @brief FreeRTOS task that drives the LED blink pattern.
 *
 * The task wakes up frequently and checks ledBlinking:
 *  - If true  → toggles the LED ON/OFF at 100 ms intervals.
 *  - If false → short sleep and re-check.
 *
 * The actual solid ON/OFF state is controlled by shearsLedSetSolidOn/Off().
 */
static void ledTask(void *arg) {
    (void)arg;

    while (1) {
        if (ledBlinking) {
            /* LED ON */
            gpio_set_level(SHEARS_STATUS_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(100));

            /* Re-check before turning it off so we don't override a
             * recent "connected" event that wants solid ON.
             */
            if (!ledBlinking) {
                continue;
            }

            /* LED OFF */
            gpio_set_level(SHEARS_STATUS_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            /* Idle quickly and re-check flag */
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void shearsLedInit(void) {
    /* Configure status LED as a push-pull output */
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << SHEARS_STATUS_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    /* Default to OFF at startup */
    gpio_set_level(SHEARS_STATUS_LED_GPIO, 0);

    /* Start LED task that implements blink behavior */
    xTaskCreate(ledTask,
                "shears_led",
                2048,
                NULL,
                5,
                &ledTaskHandle);
}

void shearsLedSetBlinking(bool enable) {
    ledBlinking = enable;

    /* When we stop blinking, we don't force any particular solid state here.
     * The app is expected to call shearsLedSetSolidOn/Off as needed.
     */
}

void shearsLedSetSolidOn(void) {
    ledBlinking = false;
    gpio_set_level(SHEARS_STATUS_LED_GPIO, 1);
}

void shearsLedSetOff(void) {
    ledBlinking = false;
    gpio_set_level(SHEARS_STATUS_LED_GPIO, 0);
}
