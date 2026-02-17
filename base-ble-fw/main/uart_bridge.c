/*
 * uart_bridge.c
 *
 * UART bridge implementation for ESP32 → Raspberry Pi communication.
 *
 * All outgoing data is wrapped in a simple frame so the Pi receiver
 * can reliably synchronize, detect boundaries, and verify integrity:
 *
 *   [0xAA] [type] [len_lo] [len_hi] [payload...] [checksum]
 *
 * The checksum is an XOR of all bytes from type through the last
 * payload byte.  Simple, fast, and catches single-bit errors.
 */

#include "uart_bridge.h"

#include <string.h>
#include "esp_log.h"
#include "driver/uart.h"

static const char *TAG = "uart_bridge";

/* TX/RX buffer sizes for the UART driver. */
#define UART_TX_BUF_SIZE  512
#define UART_RX_BUF_SIZE  256

/* ── Initialization ─────────────────────────────────────────────────────── */

void uart_bridge_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate  = UART_BRIDGE_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /* Install the UART driver with TX buffer only.
     * RX buffer is minimal since Pi → ESP32 traffic is not expected
     * in the initial design (could be used later for commands). */
    ESP_ERROR_CHECK(uart_driver_install(UART_BRIDGE_PORT_NUM,
                                        UART_RX_BUF_SIZE,
                                        UART_TX_BUF_SIZE,
                                        0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_BRIDGE_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_BRIDGE_PORT_NUM,
                                 UART_BRIDGE_TX_PIN,
                                 UART_BRIDGE_RX_PIN,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART%d initialized: TX=GPIO%d RX=GPIO%d @ %d baud",
             UART_BRIDGE_PORT_NUM,
             UART_BRIDGE_TX_PIN,
             UART_BRIDGE_RX_PIN,
             UART_BRIDGE_BAUD);
}

/* ── Frame builder + send ───────────────────────────────────────────────── */

esp_err_t uart_bridge_send(uart_msg_type_t type,
                           const uint8_t *payload,
                           uint16_t len)
{
    if (len > UART_FRAME_MAX_PAYLOAD) {
        ESP_LOGE(TAG, "Payload too large: %u > %d", len, UART_FRAME_MAX_PAYLOAD);
        return ESP_ERR_INVALID_SIZE;
    }

    /*
     * Build the frame in a local buffer.
     * Max size: 1 (start) + 1 (type) + 2 (len) + payload + 1 (checksum)
     */
    uint8_t frame[4 + UART_FRAME_MAX_PAYLOAD + 1];
    uint16_t idx = 0;

    /* Header */
    frame[idx++] = UART_FRAME_START;
    frame[idx++] = (uint8_t)type;
    frame[idx++] = (uint8_t)(len & 0xFF);         /* len low byte */
    frame[idx++] = (uint8_t)((len >> 8) & 0xFF);  /* len high byte */

    /* Payload */
    if (payload && len > 0) {
        memcpy(&frame[idx], payload, len);
        idx += len;
    }

    /* Checksum: XOR bytes [1] through [idx-1] (skip the 0xAA start byte) */
    uint8_t cksum = 0;
    for (uint16_t i = 1; i < idx; i++) {
        cksum ^= frame[i];
    }
    frame[idx++] = cksum;

    /* Write the entire frame in one call for atomicity. */
    int written = uart_write_bytes(UART_BRIDGE_PORT_NUM, frame, idx);
    if (written < 0) {
        ESP_LOGE(TAG, "uart_write_bytes failed");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Sent frame: type=0x%02X len=%u total=%u",
             type, len, idx);
    return ESP_OK;
}

/* ── Convenience wrappers ───────────────────────────────────────────────── */

esp_err_t uart_bridge_send_cut(const uart_cut_record_t *record)
{
    if (!record) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "TX cut: seq=%lu lat=%.6f lon=%.6f force=%.2f",
             (unsigned long)record->sequence_id,
             record->latitude,
             record->longitude,
             record->force);

    return uart_bridge_send(UART_MSG_CUT_RECORD,
                            (const uint8_t *)record,
                            sizeof(uart_cut_record_t));
}

esp_err_t uart_bridge_send_status(uart_status_code_t status)
{
    uint8_t code = (uint8_t)status;

    ESP_LOGI(TAG, "TX status: 0x%02X", code);

    return uart_bridge_send(UART_MSG_STATUS, &code, 1);
}

esp_err_t uart_bridge_send_log_line(const char *line, uint16_t len)
{
    if (!line || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return uart_bridge_send(UART_MSG_LOG_LINE,
                            (const uint8_t *)line,
                            len);
}
