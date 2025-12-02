/**
 * @file base_led.c
 * @brief Implements the status LED logic for the base station.
 *
 * The LED has two behaviors:
 *   1. Fast blink (100ms on/off) — scanning or trying to connect
 *   2. Solid ON — connected to shears
 *
 * The blinking is handled by a dedicated FreeRTOS task so that
 * LED behavior never blocks BLE or UI logic.
 */

#include "base_led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

/* Task handle for the LED blinking task */
static TaskHandle_t ledTaskHandle = NULL;

/* Whether the LED task should blink (true) or stay steady (false) */
static volatile bool ledBlinking = false;

/**
 * @brief Background LED task.
 *
 * This task loops forever:
 *  - If blinking enabled → flash LED
 *  - If not blinking → remain idle (solid ON/OFF already set)
 */
static void ledTask(void *arg) {
    while (1) {
        if (ledBlinking) {
            /* LED ON */
            gpio_set_level(BASE_STATUS_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(100));

            /* Check again before turning OFF in case state changed */
            if (!ledBlinking) {
                continue;
            }

            /* LED OFF */
            gpio_set_level(BASE_STATUS_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            /* Idle quickly (no blink) */
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

/**
 * @brief Initialize LED GPIO and start LED task.
 */
void baseLedInit(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << BASE_STATUS_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);

    /* LED starts OFF */
    gpio_set_level(BASE_STATUS_LED_GPIO, 0);

    /* Start the LED task */
    xTaskCreate(ledTask, "base_led", 2048, NULL, 5, &ledTaskHandle);
}

/**
 * @brief Enable or disable blinking mode.
 *
 * @param enable true → blink, false → stop blinking
 */
void baseLedSetBlinking(bool enable) {
    ledBlinking = enable;

    if (!enable) {
        /* When stopping blinking, default to solid ON */
        gpio_set_level(BASE_STATUS_LED_GPIO, 1);
    }
}

/**
 * @brief Set the LED to solid ON state.
 */
void baseLedSetSolidOn(void) {
    ledBlinking = false;
    gpio_set_level(BASE_STATUS_LED_GPIO, 1);
}

/**
 * @brief Turn the LED completely OFF.
 */
void baseLedSetOff(void) {
    ledBlinking = false;
    gpio_set_level(BASE_STATUS_LED_GPIO, 0);
}
