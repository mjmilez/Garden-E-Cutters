/*
 * shears_piezo.h
 *
 * Piezo buzzer driver (LEDC) for the shears firmware.
 *
 * Notes:
 * - Call shearsPiezoInit() once at startup before using anything else.
 * - These helpers use vTaskDelay(), so beep/tone calls block the caller task.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Sets up LEDC + both piezo pins. Call once during init. */
void shearsPiezoInit(void);

/* Turns the tone on/off. Useful if you want manual control instead of patterns. */
void shearsPiezoSet(bool enable);

/* Plays N short beeps (count = 1,2,3...). Blocks while it plays. */
void shearsPiezoBeepPattern(int count);

/* Plays a steady tone for durationMs milliseconds. Blocks while it plays. */
void shearsPiezoToneMs(uint32_t durationMs);

#ifdef __cplusplus
}
#endif