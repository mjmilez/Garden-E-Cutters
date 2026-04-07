#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

void shears7SegmentInit(void);
void shears7SegmentSetRaw(uint8_t value);
bool shears7SegmentSetDigit(uint8_t digit);
void shears7SegmentClear(void);
void shears7SegmentShowFixType(uint8_t fixType);

#ifdef __cplusplus
}
#endif