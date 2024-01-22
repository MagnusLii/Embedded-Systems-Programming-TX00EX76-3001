#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define SW_0_PIN 9
#define TX_PIN 4
#define RX_PIN 5
#define UART_ID uart1
#define BAUD_RATE 9600
#define TIMEOUT_MS 500
#define BUFFER_SIZE 256
#define SEPARATOR "\n----------------------------------------\n"

void send_command(const char* command);
bool read_response(const char expected_response[], int response_len, int max_attempts, char *pstring_to_store_to);
void uart_rx_handler();
bool process_uart_data(const char expected_response[], int response_len, char *pstring_to_store_to);
void process_DevEui(char DevEui[], int DevEui_len);

char circular_buffer[BUFFER_SIZE];
volatile int buffer_head = 0;
volatile int buffer_tail = 0;

int main() {
    stdio_init_all();

    gpio_init(SW_0_PIN);
    gpio_set_dir(SW_0_PIN, GPIO_IN);
    gpio_pull_up(SW_0_PIN);

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(RX_PIN, GPIO_FUNC_UART);

    // Set up the UART receive interrupt
    uart_set_irq_enables(UART_ID, true, false);
    irq_set_exclusive_handler(UART1_IRQ, uart_rx_handler);
    irq_set_enabled(UART1_IRQ, true);

    int state = 1;
    char read_data[BUFFER_SIZE];

    while (true) {
        if (state == 1) {
            printf(SEPARATOR);
            printf("Press SW_0 to start communication with the LoRa module...\n");
            while (gpio_get(SW_0_PIN)) {
                sleep_ms(10);
            }
            state = 2;
        } else if (state == 2) {
            printf("Connecting to LoRa module...\n");
            send_command("AT\r\n");
            if (read_response("+AT: OK", strlen("+AT: OK"), 5, read_data) == true) {
                printf("Connected to LoRa module\n");
                printf("Response: %s", read_data);
                state = 3;
            } else {
                printf("Module not responding\n");
                state = 1;
            }
        } else if (state == 3) {
            printf("Reading firmware ver...\n");
            send_command("AT+VER\r\n");
            if (read_response("+VER: ", strlen("+VER: "), 5, read_data) == true) {
                printf("Response: %s", read_data);
                state = 4;
            } else {
                printf("Module not responding\n");
                state = 1;
            }
        } else if(state == 4) {
            printf("Reading DevEui...\n");
            send_command("AT+ID=DevEui\r\n");
            if (read_response("+ID: DevEui,", strlen("+ID: DevEui,"), 5, read_data) == true) {
                printf("Response: %s", read_data);
                process_DevEui(read_data, strlen(read_data));
                printf("Formatted DevEui: %s", read_data);
            } else {
                printf("Module not responding\n");
            }
            state = 1;
        }
    }
    return 0;
}

void send_command(const char* command) {
    uart_puts(UART_ID, command);
}

bool read_response(const char expected_response[], const int response_len, int max_attempts, char *pstring_to_store_to) {
    for (int i = 0; i < max_attempts; i++) {
        sleep_ms(TIMEOUT_MS);
        bool msg_status = process_uart_data(expected_response, response_len, pstring_to_store_to);
        if (msg_status == true) {
            return true;
        }
        printf("No response from module, retrying...\n");
    }
    return false;
}

void uart_rx_handler() {
    while (uart_is_readable(UART_ID)) {
        char received_char = uart_getc(UART_ID);
        int next_head = (buffer_head + 1) % BUFFER_SIZE;

        if (next_head != buffer_tail) {
            circular_buffer[buffer_head] = received_char;
            buffer_head = next_head;
        } else {
            // Discard the data
        }
    }
}

bool process_uart_data(const char expected_response[], const int response_len, char *pstring_to_store_to) {
    int datalen = 0;
    char data[BUFFER_SIZE];

    // Check if there is data in the circular buffer
    while (buffer_tail != buffer_head) {
        data[datalen] = circular_buffer[buffer_tail];
        buffer_tail = (buffer_tail + 1) % BUFFER_SIZE;
        datalen++;
    }
    if (strncmp(data, expected_response, response_len) == 0){
        strncpy(pstring_to_store_to, data, datalen);
        pstring_to_store_to[datalen] = '\0';
        return true;
    }
    return false;
}

void process_DevEui(char DevEui[], const int DevEui_len) {

    for (int strlen = 0; strlen < DevEui_len; strlen++){
        if (DevEui[strlen] == ':'){
            for (int i = strlen; i < DevEui_len; i++){
                DevEui[i] = DevEui[i + 1]; // remove : from the string
            }
        }
        DevEui[strlen] = tolower(DevEui[strlen]); // convert to lowercase
    }

    // remove "+ID: DevEui, " from the string
    for (int i = 0; i < strlen("+ID: DevEui "); i++){
        for (int j = 0; j < DevEui_len; j++){
            DevEui[j] = DevEui[j + 1];
        }
    }
}

