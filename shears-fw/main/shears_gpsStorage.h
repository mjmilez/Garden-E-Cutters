#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool shearsGpsStorageEnsureCsvExists(const char* csvPath);

bool shearsGpsStorageClearCsv(const char* csvPath);

bool shearsGpsStorageAppendGngga(const char* csvPath, const char* nmea);

void shearsGpsStoragePrintNewest(const char* csvPath, int maxLines);

#ifdef __cplusplus
}
#endif