/* shears_primeSwitch.h
 *
 * Tracks the SAFE/PRIMED state from the PRIME switch pin level.
 * Call shearsPrimeSwitchInit() once at startup, then feed pin levels into
 * shearsPrimeSwitchUpdateFromLevel() from the GPIO ISR.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Init prime state from the current pin level at boot. */
void shearsPrimeSwitchInit(int initialLevel);

/* Call from the PRIME pin ISR (pass gpio_get_level(PRIME_BUTTON_PIN)). */
void shearsPrimeSwitchUpdateFromLevel(int level);

bool shearsPrimeSwitchIsPrimed(void);

/* Returns true once per SAFE->PRIMED transition (then clears the flag). */
bool shearsPrimeSwitchConsumePrimedEdge(void);

/* Returns true once per PRIMED->SAFE transition (then clears the flag). */
bool shearsPrimeSwitchConsumeUnprimedEdge(void);

#ifdef __cplusplus
}
#endif