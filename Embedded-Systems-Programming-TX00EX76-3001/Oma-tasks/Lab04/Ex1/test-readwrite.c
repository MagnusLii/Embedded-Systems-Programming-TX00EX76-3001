#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include <stdio.h>

#define ROT_A 10
#define ROT_B 11
#define ROT_SW 12

#define N_LED 3
#define STARTING_LED 20
#define LED_BRIGHT_MAX 999
#define LED_BRIGHT_MIN 0
#define LED_BRIGHT_STEP 10

#define EEPROM_ADDR 0x50  // I2C address of the EEPROM
#define SDA_PIN 16
#define SCL_PIN 17

void write_eeprom(uint16_t mem_addr, uint8_t data);
uint8_t read_eeprom(uint16_t mem_addr);
void write_eeprom_multi(uint16_t mem_addr, const uint8_t* data, size_t len);
void read_eeprom_multi(uint16_t mem_addr, uint8_t* data, size_t len);

int main()
{

    // init the I2C bus
    i2c_init(i2c_default, 100 * 10000); //1Mhz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);


    stdio_init_all();
    sleep_ms(1000);
    printf("Starting...\n");

    printf("Writing...\n");
    write_eeprom(1, 1);
    sleep_ms(100);
    printf("Reading...\n");
    uint8_t data = read_eeprom(1);
    printf("Data: %d\n", data);    
    
    return 0;
}

// Function to write data to a specific EEPROM address
void write_eeprom(uint16_t mem_addr, uint8_t data) {
    uint8_t reg_addr[2] = {mem_addr >> 8, mem_addr & 0xFF};  // High and low bytes of the EEPROM address
    uint8_t combined[3] = {reg_addr[0], reg_addr[1], data};  // Combine the register address and data
    i2c_write_blocking(i2c_default, EEPROM_ADDR, combined, 3, false);
}

// Function to read data from a specific EEPROM address
uint8_t read_eeprom(uint16_t mem_addr) {
    uint8_t reg_addr[2] = {mem_addr >> 8, mem_addr & 0xFF};  // High and low bytes of the EEPROM address
    i2c_write_blocking(i2c_default, EEPROM_ADDR, reg_addr, 2, true);  // Write the register address with nostop=true
    uint8_t data;
    i2c_read_blocking(i2c_default, EEPROM_ADDR, &data, 1, false);
    return data;
}

// Function to write multiple bytes of data to a specific EEPROM address
void write_eeprom_multi(uint16_t mem_addr, const uint8_t* data, size_t len) {
    uint8_t reg_addr[2] = {mem_addr >> 8, mem_addr & 0xFF};  // High and low bytes of the EEPROM address
    uint8_t combined[len + 2];  // Array to hold the register address and data
    combined[0] = reg_addr[0];
    combined[1] = reg_addr[1];
    memcpy(&combined[2], data, len);  // Copy the data into the combined array
    i2c_write_blocking(i2c_default, EEPROM_ADDR, combined, len + 2, false);
}

// Function to read multiple bytes of data from a specific EEPROM address
void read_eeprom_multi(uint16_t mem_addr, uint8_t* data, size_t len) {
    uint8_t reg_addr[2] = {mem_addr >> 8, mem_addr & 0xFF};  // High and low bytes of the EEPROM address
    i2c_write_blocking(i2c_default, EEPROM_ADDR, reg_addr, 2, true);  // Write the register address with nostop=true
    i2c_read_blocking(i2c_default, EEPROM_ADDR, data, len, false);
}
