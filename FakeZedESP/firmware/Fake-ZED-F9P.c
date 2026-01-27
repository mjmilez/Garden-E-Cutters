// Fake-ZED-F9P-UART.c - simple NMEA output over UART + solid LED
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

//lets see 
#define UART_PORT    uart1      
#define UART_TX_PIN  24         // TX1 on Feather RP2040
#define UART_RX_PIN  25         // RX1 on Feather RP2040
#define UART_BAUD    38400      // Baud rate for testing

// LED pin - GPIO 2 on Adafruit Feather RP2040
#define LED_PIN      13

static const char GGA[] =
    "$GPGGA,111111.00,2940.1234567,N,08219.7654321,W,4,20,0.8,46.123,M,-34.000,M,1.2,0101*5E\r\n";

    //keep this in mind
int main(void) {
    stdio_init_all();
    sleep_ms(500); //checking to see merge

    // Init LED and turn it on solid
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    // Init UART
    uart_init(UART_PORT, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    printf("Fake ZED-F9P UART streaming started on TX=%d at %d baud\n",
           UART_TX_PIN, UART_BAUD);

    while (true) {
        uart_puts(UART_PORT, GGA);   // Send NMEA over UART
        sleep_ms(1000);              // 1 sentence per second
    }
}