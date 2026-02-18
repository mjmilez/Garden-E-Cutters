#ifndef UART_FILE_TRANSFER_H
#define UART_FILE_TRANSFER_H

#include <stdint.h>

typedef enum {
	TRANSFER_TRIGGER_BUTTON = 1,
	TRANSFER_TRIGGER_EVENT  = 2,
} transferTrigger_t;

/* Initialize UART + transfer task + button */
void uartFileTransferInit(void);

/* Trigger a transfer manually (event-based) */
void transferStart(transferTrigger_t trigger);

#endif