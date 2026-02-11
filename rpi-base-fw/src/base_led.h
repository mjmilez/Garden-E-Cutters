#ifndef GPIO_LED_H
#define GPIO_LED_H

#include <stdint.h>

void gpioLedOn(int gpioPinBcm);
void gpioLedOff(int gpioPinBcm);
void gpioLedBlink(int gpioPinBcm);

void gpioLedSetBlinkMs(uint32_t onMs, uint32_t offMs);

// Optional cleanup
void gpioLedShutdown(void);

#endif