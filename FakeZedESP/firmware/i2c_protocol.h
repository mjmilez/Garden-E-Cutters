#ifndef I2C_PROTOCOL_H
#define I2C_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// I2C Configuration
#define FAKE_ZED_I2C_ADDR    0x42        // Same as real ZED-F9P
#define I2C_FREQ_HZ          100000      // 100 kHz standard mode
#define I2C_FREQ_FAST_HZ     400000      // 400 kHz fast mode

// Register addresses (emulate real ZED-F9P)
#define REG_BYTES_AVAILABLE_HIGH  0xFF   // High byte of available data count
#define REG_BYTES_AVAILABLE_LOW   0xFE   // Low byte of available data count
#define REG_DATA_STREAM          0xFD   // Data stream register

// Buffer sizes
#define I2C_TX_BUFFER_SIZE   512         // Outgoing NMEA buffer
#define MAX_NMEA_SENTENCE    256         // Max single sentence length

// Retry configuration
#define MAX_RETRY_COUNT      3
#define RETRY_DELAY_MS       10

// Status codes
typedef enum {
    I2C_OK = 0,
    I2C_NACK,
    I2C_BUS_ERROR,
    I2C_TIMEOUT,
    I2C_BUFFER_FULL
} I2CStatus;

#endif