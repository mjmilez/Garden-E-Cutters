#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Mounts SPIFFS at /spiffs on partition "storage". */
bool shearsSpiffsInit(void);

#ifdef __cplusplus
}
#endif