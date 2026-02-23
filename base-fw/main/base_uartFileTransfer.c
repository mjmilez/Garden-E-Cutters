// uartFileTransfer.c

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_timer.h"

#define TAG "uartTx"

/* ───────────────────────── Config ───────────────────────── */

#define UART_PORT			UART_NUM_1
#define UART_BAUD			115200

#define UART_TX_GPIO		(GPIO_NUM_17)   // Purple w/ white stripe on breadboard
#define UART_RX_GPIO		(GPIO_NUM_16)   // Blue w/ white stripe on breadboard

#define BUTTON_GPIO			(GPIO_NUM_32)
#define BUTTON_ACTIVE_LOW	1

#define CSV_PATH			"/spiffs/gps_points.csv"

#define START_BYTE			0xAA

#define TYPE_START			0x01
#define TYPE_DATA			0x02
#define TYPE_END			0x03
#define TYPE_ACK			0x04
#define TYPE_COMMIT			0x05

#define CHUNK_SIZE			255

#define ACK_TIMEOUT_MS		500
#define MAX_RETRIES			5

/* ───────────────────────── Events ───────────────────────── */

typedef enum {
	TRANSFER_TRIGGER_BUTTON = 1,
	TRANSFER_TRIGGER_EVENT = 2,
} transferTrigger_t;

typedef struct {
	transferTrigger_t trigger;
} transferReq_t;

static QueueHandle_t transferQueue;

/* Busy lock: drop triggers while a transfer is in progress */
static volatile bool transferBusy = false;

/* ───────────────────────── Packet helpers ───────────────────────── */

static uint8_t checksumXor(const uint8_t* data, int len)
{
	uint8_t c = 0;
	for (int i = 0; i < len; i++) {
		c ^= data[i];
	}
	return c;
}

/* Frame: [0xAA][type][len][payload...][xor(type,len,payload)] */
static int buildPacket(uint8_t type, const uint8_t* payload, uint8_t payloadLen, uint8_t* out, int outCap)
{
	int needed = 1 + 1 + 1 + payloadLen + 1;
	if (outCap < needed) {
		return -1;
	}

	out[0] = START_BYTE;
	out[1] = type;
	out[2] = payloadLen;

	if (payloadLen > 0 && payload != NULL) {
		memcpy(&out[3], payload, payloadLen);
	}

	uint8_t c = 0;
	c ^= out[1];
	c ^= out[2];
	if (payloadLen > 0) {
		c ^= checksumXor(payload, payloadLen);
	}
	out[3 + payloadLen] = c;

	return needed;
}

static esp_err_t uartSendPacket(uint8_t type, const uint8_t* payload, uint8_t payloadLen)
{
	uint8_t buf[1 + 1 + 1 + CHUNK_SIZE + 1];
	int n = buildPacket(type, payload, payloadLen, buf, sizeof(buf));
	if (n < 0) {
		return ESP_FAIL;
	}

	int wrote = uart_write_bytes(UART_PORT, (const char*)buf, n);
	return (wrote == n) ? ESP_OK : ESP_FAIL;
}

/* Minimal parser for incoming packets.
   Returns true if a full valid packet was parsed into type, payload, and payloadLen */
static bool uartTryReadPacket(uint8_t* type, uint8_t* payload, uint8_t* payloadLen, int timeoutMs)
{
	uint8_t b = 0;

	/* Find START_BYTE */
	int r = uart_read_bytes(UART_PORT, &b, 1, pdMS_TO_TICKS(timeoutMs));
	if (r != 1) {
		return false;
	}
	while (b != START_BYTE) {
		r = uart_read_bytes(UART_PORT, &b, 1, pdMS_TO_TICKS(timeoutMs));
		if (r != 1) {
			return false;
		}
	}

	uint8_t hdr[2]; /* type,len */
	r = uart_read_bytes(UART_PORT, hdr, 2, pdMS_TO_TICKS(timeoutMs));
	if (r != 2) {
		return false;
	}

	uint8_t t = hdr[0];
	uint8_t len = hdr[1];

	if (len > CHUNK_SIZE) {
		return false;
	}

	uint8_t tmp[CHUNK_SIZE + 1]; /* payload + checksum */
	r = uart_read_bytes(UART_PORT, tmp, len + 1, pdMS_TO_TICKS(timeoutMs));
	if (r != (len + 1)) {
		return false;
	}

	uint8_t calc = 0;
	calc ^= t;
	calc ^= len;
	if (len > 0) {
		calc ^= checksumXor(tmp, len);
	}

	if (calc != tmp[len]) {
		return false;
	}

	*type = t;
	*payloadLen = len;
	if (len > 0 && payload != NULL) {
		memcpy(payload, tmp, len);
	}
	return true;
}

static bool waitForAck(uint32_t timeoutMs)
{
	int64_t start = esp_timer_get_time();
	while ((uint32_t)((esp_timer_get_time() - start) / 1000) < timeoutMs) {
		uint8_t type = 0;
		uint8_t payload[8];
		uint8_t len = 0;

		if (!uartTryReadPacket(&type, payload, &len, 50)) {
			continue;
		}

		if (type == TYPE_ACK) {
			return true;
		}

		/* Ignore anything else for now */
	}
	return false;
}

static bool waitForCommit(uint32_t timeoutMs, uint8_t* outStatus)
{
	int64_t start = esp_timer_get_time();
	while ((uint32_t)((esp_timer_get_time() - start) / 1000) < timeoutMs) {
		uint8_t type = 0;
		uint8_t payload[8];
		uint8_t len = 0;

		if (!uartTryReadPacket(&type, payload, &len, 50)) {
			continue;
		}

		if (type == TYPE_COMMIT && len >= 1) {
			*outStatus = payload[0];
			return true;
		}
	}
	return false;
}

/* ───────────────────────── Transfer logic ───────────────────────── */

static bool sendWithAck(uint8_t type, const uint8_t* payload, uint8_t payloadLen)
{
	for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
		if (uartSendPacket(type, payload, payloadLen) != ESP_OK) {
			continue;
		}
		if (waitForAck(ACK_TIMEOUT_MS)) {
			return true;
		}
	}
	return false;
}

static bool transferCsvFile(void)
{
	FILE* f = fopen(CSV_PATH, "rb");
	if (!f) {
		ESP_LOGE(TAG, "Failed to open %s", CSV_PATH);
		return false;
	}

	/* Determine file size */
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size <= 0) {
		ESP_LOGW(TAG, "File empty or invalid size (%ld), skipping", size);
		fclose(f);
		return false;
	}

	if (size > 0xFFFFFFFF) {
		ESP_LOGE(TAG, "File too large");
		fclose(f);
		return false;
	}

	uint32_t fileSize = (uint32_t)size;

	/* START payload: [fileSize u32 little-endian] */
	uint8_t startPayload[4];
	startPayload[0] = (uint8_t)(fileSize & 0xFF);
	startPayload[1] = (uint8_t)((fileSize >> 8) & 0xFF);
	startPayload[2] = (uint8_t)((fileSize >> 16) & 0xFF);
	startPayload[3] = (uint8_t)((fileSize >> 24) & 0xFF);

	ESP_LOGI(TAG, "START (size=%u)", fileSize);
	if (!sendWithAck(TYPE_START, startPayload, 4)) {
		ESP_LOGE(TAG, "START not ACKed");
		fclose(f);
		return false;
	}

	uint8_t chunk[CHUNK_SIZE];
	uint32_t sent = 0;

	while (sent < fileSize) {
		size_t toRead = CHUNK_SIZE;
		if (fileSize - sent < CHUNK_SIZE) {
			toRead = fileSize - sent;
		}

		size_t got = fread(chunk, 1, toRead, f);
		if (got != toRead) {
			ESP_LOGE(TAG, "Read error (%u/%u)", (unsigned)got, (unsigned)toRead);
			fclose(f);
			return false;
		}

		if (!sendWithAck(TYPE_DATA, chunk, (uint8_t)got)) {
			ESP_LOGE(TAG, "DATA not ACKed (sent=%u)", sent);
			fclose(f);
			return false;
		}

		sent += (uint32_t)got;
	}

	fclose(f);

	ESP_LOGI(TAG, "END");
	if (!sendWithAck(TYPE_END, NULL, 0)) {
		ESP_LOGE(TAG, "END not ACKed");
		return false;
	}

	uint8_t commitStatus = 0xFF;
	if (!waitForCommit(2000, &commitStatus)) {
		ESP_LOGE(TAG, "No COMMIT received");
		return false;
	}

	if (commitStatus != 0x00) {
		ESP_LOGE(TAG, "COMMIT error status=0x%02X", commitStatus);
		return false;
	}

	ESP_LOGI(TAG, "COMMIT ok -> clearing file");
	/* Clear file: simplest is truncate to 0 bytes */
	FILE* wf = fopen(CSV_PATH, "wb");
	if (!wf) {
		ESP_LOGE(TAG, "Failed to clear file");
		return false;
	}
	fclose(wf);

	return true;
}

/* ───────────────────────── Trigger plumbing ───────────────────────── */

void transferStart(transferTrigger_t trigger)
{
	if (transferBusy) {
		return;
	}

	transferReq_t req = {
		.trigger = trigger,
	};
	xQueueSend(transferQueue, &req, 0);
}

/* ISR: keep it tiny, just notify */
static void IRAM_ATTR buttonIsr(void* arg)
{
	(void)arg;

	if (transferBusy) {
		return;
	}

	BaseType_t hpTaskWoken = pdFALSE;

	transferReq_t req = {
		.trigger = TRANSFER_TRIGGER_BUTTON,
	};
	xQueueSendFromISR(transferQueue, &req, &hpTaskWoken);

	if (hpTaskWoken) {
		portYIELD_FROM_ISR();
	}
}

static void transferTask(void* arg)
{
	(void)arg;

	while (1) {
		transferReq_t req;
		if (xQueueReceive(transferQueue, &req, portMAX_DELAY) != pdTRUE) {
			continue;
		}

		if (transferBusy) {
			continue;
		}

		transferBusy = true;

		/* Drop any extra queued triggers (bounce / spam) */
		xQueueReset(transferQueue);

		ESP_LOGI(TAG, "Transfer requested (trigger=%d)", (int)req.trigger);

		bool ok = transferCsvFile();
		ESP_LOGI(TAG, "Transfer %s", ok ? "OK" : "FAIL");

		transferBusy = false;
	}
}

/* ───────────────────────── Init ───────────────────────── */

void uartFileTransferInit(void)
{
	/* UART setup */
	uart_config_t cfg = {
		.baud_rate = UART_BAUD,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	uart_driver_install(UART_PORT, 4096, 4096, 0, NULL, 0);
	uart_param_config(UART_PORT, &cfg);
	uart_set_pin(UART_PORT, UART_TX_GPIO, UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

	transferQueue = xQueueCreate(4, sizeof(transferReq_t));

	xTaskCreate(transferTask, "transferTask", 4096, NULL, 10, NULL);

	/* Button setup */
	gpio_config_t io = {
		.pin_bit_mask = 1ULL << BUTTON_GPIO,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = BUTTON_ACTIVE_LOW ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
		.pull_down_en = BUTTON_ACTIVE_LOW ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
		.intr_type = BUTTON_ACTIVE_LOW ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE,
	};
	gpio_config(&io);

	gpio_install_isr_service(0);
	gpio_isr_handler_add(BUTTON_GPIO, buttonIsr, NULL);

	ESP_LOGI(TAG, "uartFileTransferInit done");
}