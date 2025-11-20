#pragma once

#include <stdint.h>
#include <stdbool.h>

// Call once after BLE stack is up and before/around advertising.
void log_transfer_server_init(void);
