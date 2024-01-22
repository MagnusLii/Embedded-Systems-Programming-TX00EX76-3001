#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/util/queue.h"
#include <stdio.h>
#include <stdbool.h>

#define ROT_A 10
#define ROT_B 11
#define ROT_SW 12

#define BUTTON1_PIN 7
#define BUTTON2_PIN 8
#define BUTTON3_PIN 9
#define SDA_PIN 17
#define SCL_PIN 16

#define N_LED 3
#define STARTING_LED 20
#define LED_BRIGHT_MAX 999
#define LED_BRIGHT_MIN 0
#define LED_BRIGHT_STEP 10

#define EEPROM_ADDR 0x50 // I2C address of the EEPROM
#define BRIGHTNESS_ADDR 2
#define EEPROM_WRITE_DELAY_MS 5
#define LED_STATE_ADDR 32768 // Address in the EEPROM to store the LED state
#define BUFFER_SIZE 512

typedef struct ledStatus
{
    bool ledState[3];
    uint16_t brightness;
} ledStatus;

int main()
{
    
    
    return 0;
}

void write(){

    uint8_t data[3] = {1, 1, 0}
    uint8_t len = 3;
    uint8_t addr = 100;

    uint8_t newlen = len + 1;
    uint8_t newdata[newlen];
    newdata[0] = addr;
    memcpy(&newdata[1], data, len);
}