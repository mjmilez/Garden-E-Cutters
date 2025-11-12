#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

// -------- Pins & ports (your current setup) --------
#define I2C_PORT    i2c0
#define I2C_SDA     8
#define I2C_SCL     9

#define UART_ID     uart1
#define BAUD_RATE   115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5   // not used, but configured

// -------- Fake ZED-F9P register map --------
#define I2C_ADDR        0x42
#define REG_DATA_HIGH   0xFD
#define REG_DATA_LOW    0xFE
#define REG_DATA_STREAM 0xFF

// -------- TX ring buffer for NMEA --------
#define BUFFER_SIZE 512
static uint8_t  tx_buf[BUFFER_SIZE];
static uint16_t tx_head = 0, tx_tail = 0, tx_count = 0;
static uint8_t  current_reg = 0xFF;

// Example high-precision GGA (fix quality=4 = RTK fixed)
static const char *GGA =
  "$GPGGA,123519.00,2940.1234567,N,08219.7654321,W,4,20,0.8,46.123,M,-34.000,M,1.2,0101*5E\r\n";

static inline void enqueue_sentence(const char *s) {
    uint16_t len = (uint16_t)strlen(s);

    // Make room by discarding oldest bytes if needed
    while (tx_count + len > BUFFER_SIZE) {
        tx_tail = (uint16_t)((tx_tail + 1) % BUFFER_SIZE);
        tx_count--;
    }

    // Copy into ring buffer
    for (uint16_t i = 0; i < len; i++) {
        tx_buf[tx_head] = (uint8_t)s[i];
        tx_head = (uint16_t)((tx_head + 1) % BUFFER_SIZE);
        tx_count++;
    }

    // Mirror to USB serial (your NMEA already ends with \r\n, but add one more just in case)
    printf("%s", s);              // prints the full $GPGGA... line
    printf("enqueued %u bytes (buffer %u/%u)\n", len, tx_count, BUFFER_SIZE);
}


static inline uint8_t pop_stream_byte(void) {
    if (tx_count == 0) return 0xFF;   // matches real F9P behavior
    uint8_t b = tx_buf[tx_tail];
    tx_tail = (uint16_t)((tx_tail + 1) % BUFFER_SIZE);
    tx_count--;
    return b;
}

int main(void) {
    stdio_init_all();
    sleep_ms(800); // give USB time to come up

    printf("\n===========================================\n");
    printf("Fake ZED-F9P I2C Slave (RP2040 / Pico SDK)\n");
    printf("I2C addr 0x%02X  SDA=%d SCL=%d  @400kHz\n", I2C_ADDR, I2C_SDA, I2C_SCL);
    printf("REG 0xFD=hi, 0xFE=lo, 0xFF=stream\n");
    printf("UART mirror on uart1 TX=%d @%d (optional)\n", UART_TX_PIN, BAUD_RATE);
    printf("===========================================\n");

    // ------ I2C target (slave) init ------
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    i2c_set_slave_mode(I2C_PORT, true, I2C_ADDR);

    // ------ UART mirror (optional) ------
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    absolute_time_t last = get_absolute_time();

    while (true) {
        // 1) Generate a GGA once per second
        if (absolute_time_diff_us(last, get_absolute_time()) > 1000000) {
            last = get_absolute_time();
            enqueue_sentence(GGA);
            // also mirror to UART for a quick sniff/parse path
            uart_puts(UART_ID, GGA);
        }

        // 2) Handle master "random access" writes (register select)
        int rx_avail = i2c_get_read_available(I2C_PORT);
        while (rx_avail--) {
            uint8_t v = i2c_read_byte_raw(I2C_PORT);
            current_reg = v; // just the last byte matters; extra bytes are ignored
            printf("master selected reg 0x%02X\n", current_reg);
        }

        // 3) Serve reads using the TX FIFO
        int tx_space = i2c_get_write_available(I2C_PORT);
        while (tx_space--) {
            uint8_t out = 0x00;
            switch (current_reg) {
                case REG_DATA_HIGH:  out = (tx_count >> 8) & 0xFF; break;
                case REG_DATA_LOW:   out = tx_count & 0xFF;        break;
                case REG_DATA_STREAM:out = pop_stream_byte();      break;
                default:             out = 0x00;                   break;
            }
            i2c_write_byte_raw(I2C_PORT, out);
        }

        tight_loop_contents();
    }
}
