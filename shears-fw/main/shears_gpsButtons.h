#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "driver/gpio.h"

/* Pins owned by this module */
#define SHEARS_GPS_BUTTON_PIN   GPIO_NUM_23
#define SHEARS_PRIME_BUTTON_PIN GPIO_NUM_22
#define SHEARS_CUT2_BUTTON_PIN  GPIO_NUM_19
#define SHEARS_CUT3_BUTTON_PIN  GPIO_NUM_18

typedef struct {
	/* PRIME switch is level-based; called on any edge with the current level. */
	void (*onPrimeLevel)(int level);

	/* GPS button is press/release; called on any edge with the current level. */
	void (*onGpsButtonLevel)(int level);

	/* CUT2/CUT3 are press events; called on falling edge only (level==0). */
	void (*onCutPress)(gpio_num_t pin);
} shearsGpsButtonsCallbacks_t;

/*
 * Configures GPIO inputs + interrupts and installs the ISR handlers.
 * Call once at startup.
 *
 * Callbacks run inside the GPIO ISR (keep them short / ISR-safe).
 */
void shearsGpsButtonsInit(const shearsGpsButtonsCallbacks_t *callbacks);

#ifdef __cplusplus
}
#endif