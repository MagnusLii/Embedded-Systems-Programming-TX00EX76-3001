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

void writeLedStateToEeprom(const struct ledStatus *ledStatusStruct)
{
    printf("\nwriteLedStateToEeprom\n");

    uint16_t ledStatusAddress = LED_STATE_ADDR - 3; // 3 bytes for the brightness
    uint16_t inverseLedStatusAddress = 500;

    uint8_t ledStatusDataByte = (ledStatusStruct->ledState[0] << 2) | (ledStatusStruct->ledState[1] << 1) | ledStatusStruct->ledState[2];
    uint8_t inverseLedStatusDataByte = (!ledStatusStruct->ledState[0] << 2) | (!ledStatusStruct->ledState[1] << 1) | !ledStatusStruct->ledState[2];

    // Creating buffer to write to EEPROM
    uint8_t buffer[3];
    buffer[0] = (uint8_t)(ledStatusAddress >> 8);
    buffer[1] = (uint8_t)(ledStatusAddress & 0xFF);
    buffer[2] = ledStatusDataByte;

    // Writing LED state to EEPROM
    printf("Buffer: %d %d %d\n", buffer[0], buffer[1], buffer[2]);
    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 3, false);
    sleep_ms(EEPROM_WRITE_DELAY_MS);

    buffer[0] = (uint8_t)(inverseLedStatusAddress >> 8);
    buffer[1] = (uint8_t)(inverseLedStatusAddress & 0xFF);
    buffer[2] = inverseLedStatusDataByte;

    // Writing inverse LED state to EEPROM
    printf("Buffer: %d %d %d\n", buffer[0], buffer[1], buffer[2]);
    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 3, false);
    sleep_ms(EEPROM_WRITE_DELAY_MS);

    printf("ledStatusDataByte: %d\n", ledStatusDataByte);
    printf("inverseLedStatusDataByte: %d\n", inverseLedStatusDataByte);
}

bool readLedStateFromEeprom(struct ledStatus *ledStatusStruct)
{
    printf("\nreadLedStateFromEeprom\n");
    uint16_t ledStatusAddress = LED_STATE_ADDR - 3; // 3 bytes for the brightness
    uint16_t inverseLedStatusAddress = 500;

    uint8_t ledStatusDataByte;
    uint8_t inverseLedStatusDataByte;

    // Creating buffer to hold data address
    uint8_t buffer[2];
    buffer[0] = (uint8_t)(ledStatusAddress >> 8);
    buffer[1] = (uint8_t)(ledStatusAddress & 0xFF);

    // Reading LED state from EEPROM
    printf("Buffer: %d %d\n", buffer[0], buffer[1]);
    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 2, true);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
    i2c_read_blocking(i2c_default, EEPROM_ADDR, &ledStatusDataByte, 1, false);

    buffer[0] = (uint8_t)(inverseLedStatusAddress >> 8);
    buffer[1] = (uint8_t)(inverseLedStatusAddress & 0xFF);

    // Reading inverse LED state from EEPROM
    printf("Buffer: %d %d\n", buffer[0], buffer[1]);
    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 2, true);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
    i2c_read_blocking(i2c_default, EEPROM_ADDR, &inverseLedStatusDataByte, 1, false);

    printf("ledStatusDataByte: %d\n", ledStatusDataByte);
    printf("inverseLedStatusDataByte: %d\n", inverseLedStatusDataByte);

    // Extract the LED state from the data byte
    int unpackedLedState[3];
    unpackedLedState[0] = (ledStatusDataByte >> 2) & 0x01;
    unpackedLedState[1] = (ledStatusDataByte >> 1) & 0x01;
    unpackedLedState[2] = ledStatusDataByte & 0x01;

    // Extract the inverse LED state from the data byte
    int unpackedInverseLedState[3];
    unpackedInverseLedState[0] = (inverseLedStatusDataByte >> 2) & 0x01;
    unpackedInverseLedState[1] = (inverseLedStatusDataByte >> 1) & 0x01;
    unpackedInverseLedState[2] = inverseLedStatusDataByte & 0x01;

    // Verify led logic status.
    for (int i = 0; i < 3; i++)
    {
        if (unpackedLedState[i] != !((bool)unpackedInverseLedState[i]))
        {
            printf("LED state and its inverse do not match\n");
            return false;
        }
    }

    // Save led state to struct
    for (int i = 0; i < 3; i++)
    {
        ledStatusStruct->ledState[i] = unpackedLedState[i];
    }

    return true;
}

void writeBrightnessToEeprom(const struct ledStatus *ledStatusStruct)
{
    printf("\nwriteBrightnessToEeprom\n");
    uint16_t brightnessAddress = BRIGHTNESS_ADDR;

    uint16_t brightnessDataByte = ledStatusStruct->brightness;

    uint8_t buffer[4];
    buffer[0] = (uint8_t)(brightnessAddress >> 8);
    buffer[1] = (uint8_t)(brightnessAddress & 0xFF);
    buffer[2] = (uint8_t)(brightnessDataByte >> 8);
    buffer[3] = (uint8_t)(brightnessDataByte & 0xFF);

    printf("Buffer: %d %d %d %d\n", buffer[0], buffer[1], buffer[2], buffer[3]);
    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 4, false);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
}

void readBrightnessFromEeprom(struct ledStatus *ledStatusStruct)
{
    printf("\nreadBrightnessFromEeprom\n");
    uint16_t brightnessAddress = BRIGHTNESS_ADDR;

    uint16_t brightnessDataByte;

    // Filling buffer with brigthness address
    uint8_t buffer[2];
    buffer[0] = (uint8_t)(brightnessAddress >> 8);
    buffer[1] = (uint8_t)(brightnessAddress & 0xFF);

    printf("Buffer: %d %d\n", buffer[0], buffer[1]);
    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 2, true);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
    i2c_read_blocking(i2c_default, EEPROM_ADDR, &buffer, 2, false); // Fills buffer with brightness data

    brightnessDataByte = (buffer[0] << 8) | buffer[1];
    printf("brightnessDataByte: %d\n", brightnessDataByte);

    ledStatusStruct->brightness = brightnessDataByte;
}