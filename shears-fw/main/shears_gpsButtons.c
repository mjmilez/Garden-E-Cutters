#include "shears_gpsButtons.h"

#include <stddef.h>

#include "esp_attr.h"
#include "esp_err.h"

static shearsGpsButtonsCallbacks_t gCallbacks;

static void IRAM_ATTR buttonsIsrHandler(void *arg)
{
	gpio_num_t pin = (gpio_num_t)(intptr_t)arg;
	int level = gpio_get_level(pin);

	if (pin == SHEARS_PRIME_BUTTON_PIN) {
		if (gCallbacks.onPrimeLevel) {
			gCallbacks.onPrimeLevel(level);
		}
		return;
	}

	if (pin == SHEARS_GPS_BUTTON_PIN) {
		if (gCallbacks.onGpsButtonLevel) {
			gCallbacks.onGpsButtonLevel(level);
		}
		return;
	}

	if ((pin == SHEARS_CUT2_BUTTON_PIN || pin == SHEARS_CUT3_BUTTON_PIN) && level == 0) {
		if (gCallbacks.onCutPress) {
			gCallbacks.onCutPress(pin);
		}
		return;
	}
}

void shearsGpsButtonsInit(const shearsGpsButtonsCallbacks_t *callbacks)
{
	if (callbacks) {
		gCallbacks = *callbacks;
	} else {
		gCallbacks.onPrimeLevel = NULL;
		gCallbacks.onGpsButtonLevel = NULL;
		gCallbacks.onCutPress = NULL;
	}

	gpio_config_t ioConf = {
		.pin_bit_mask = (1ULL << SHEARS_GPS_BUTTON_PIN) |
		                (1ULL << SHEARS_PRIME_BUTTON_PIN) |
		                (1ULL << SHEARS_CUT2_BUTTON_PIN) |
		                (1ULL << SHEARS_CUT3_BUTTON_PIN),
		.mode         = GPIO_MODE_INPUT,
		.pull_up_en   = 1,
		.pull_down_en = 0,
		.intr_type    = GPIO_INTR_ANYEDGE
	};
	gpio_config(&ioConf);

	/* Safe to call even if already installed (ESP_OK / ESP_ERR_INVALID_STATE) */
	esp_err_t err = gpio_install_isr_service(0);
	(void)err;

	gpio_isr_handler_add(SHEARS_GPS_BUTTON_PIN, buttonsIsrHandler, (void *)(intptr_t)SHEARS_GPS_BUTTON_PIN);
	gpio_isr_handler_add(SHEARS_PRIME_BUTTON_PIN, buttonsIsrHandler, (void *)(intptr_t)SHEARS_PRIME_BUTTON_PIN);
	gpio_isr_handler_add(SHEARS_CUT2_BUTTON_PIN, buttonsIsrHandler, (void *)(intptr_t)SHEARS_CUT2_BUTTON_PIN);
	gpio_isr_handler_add(SHEARS_CUT3_BUTTON_PIN, buttonsIsrHandler, (void *)(intptr_t)SHEARS_CUT3_BUTTON_PIN);
}