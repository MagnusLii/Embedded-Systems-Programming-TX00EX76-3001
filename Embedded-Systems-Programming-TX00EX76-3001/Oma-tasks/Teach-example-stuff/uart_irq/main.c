#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "uart.h"

#define STRLEN 80


// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#if 0
#define UART_NR 0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#else
#define UART_NR 1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#endif

#define BAUD_RATE 9600


int main() {

    const uint led_pin = 22;
    const uint button = 9;

    // Initialize LED pin
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

    gpio_init(button);
    gpio_set_dir(button, GPIO_IN);
    gpio_pull_up(button);

    // Initialize chosen serial port
    stdio_init_all();

    uart_setup(UART_NR, UART_TX_PIN, UART_RX_PIN, BAUD_RATE);

    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    while (gpio_get(button)) {
        sleep_ms(10);
    }
    printf("\r\nRun\r\n");
    while (!gpio_get(button)) {
        sleep_ms(10);
    }
    sleep_ms(100);

    //const uint8_t send[] = "at\r\n";
    const char send[] = "at+VER\r\n";
    char str[STRLEN];
    int pos = 0;

#if 1
    for (int i = 0; i < 3; ++i) {
        uart_send(UART_NR,
                  "another very long string that is longer than 32 characters long to test IRQ driver operation\r\n");
        if(i==0) sleep_ms(200);
    }
#endif
    while (true) {
        if (!gpio_get(button)) {
            while (!gpio_get(button)) {
                sleep_ms(50);
            }
            uart_send(UART_NR, send);
        }
        pos = uart_read(UART_NR, (uint8_t *) str, STRLEN);
        if (pos > 0) {
            str[pos] = '\0';
            printf("%d, received: %s\n", time_us_32() / 1000, str);
        }
    }

}

