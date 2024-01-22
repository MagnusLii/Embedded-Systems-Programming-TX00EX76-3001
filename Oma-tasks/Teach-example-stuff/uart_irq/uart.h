//
// Created by keijo on 4.11.2023.
//

#ifndef UART_IRQ_UART_H
#define UART_IRQ_UART_H


void uart_setup(int uart_nr, int tx_pin, int rx_pin, int speed);
int uart_read(int uart_nr, uint8_t *buffer, int size);
int uart_write(int uart_nr, const uint8_t *buffer, int size);
int uart_send(int uart_nr, const char *str);

#endif //UART_IRQ_UART_H
