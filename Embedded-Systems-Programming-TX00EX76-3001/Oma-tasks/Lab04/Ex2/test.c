#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

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
#define EEPROM_WRITE_DELAY_MS 5
#define BRIGHTNESS_ADDR 30000
#define LED_STATE_ADDR 32768 // Address in the EEPROM to store the LED state
#define INVERSE_LED_ADDR 31000
#define BUFFER_SIZE 512

#define LOG_START_ADDR 0
#define LOG_END_ADDR 2048
#define LOG_SIZE 64
#define MAX_LOGS 32
#define MAX_LOG_LEN 61
#define MIN_LOG_LEN 1

uint16_t crc16(const uint8_t *data, size_t length);
void convertStringToBase8(const char *string, const int stringLen, uint8_t *base8String);
void appendCrcToBase8String(uint8_t *base8String, int *stringLen);
int getChecksum(uint8_t *base8String, int *stringLen);
int readLogFromEeprom(const int logEntryToRead, uint8_t *logBuffer, const int logBufferLen);
void zeroAllLogs();


/*
int main()
{
    uint8_t buffer[64] = {51, 32, 93, 84, 75, 16, 17, 28, 00};
    uint16_t crc = crc16(buffer, 9);

    buffer[9] = crc >> 8;
    buffer[10] = crc & 0xFF;

    printf("CRC: %d\n", crc16(buffer, 64));

    if (crc16(buffer, 64) == 0)
    {
        printf("CRC OK\n");
    }
    else
    {
        printf("CRC FAIL\n");
    }

    return 0;
}


int main()
{
    char string[61] = "Hello asdasdfg world!";
    int origStrLen = strlen(string);
    uint8_t base8String[64];

    convertStringToBase8(string, origStrLen, base8String);

    appendCrcToBase8String(base8String, &origStrLen);

    getChecksum(base8String, &origStrLen);

    int crc = crc16(base8String, origStrLen);
    printf("CRC: %d\n", crc);

    return 0;
}


int main()
{
    zeroAllLogs();
    
    return 0;
}
*/

int main()
{
    uint8_t buffer[64] = {76, 101, 100, 32, 50, 32, 116, 111, 103, 103, 108, 101, 100, 32, 116, 111, 32, 115, 116, 97, 116, 101, 32, 48, 44, 32, 115, 101, 99, 111, 110, 100, 115, 32, 115, 105, 110, 99, 101, 32, 98, 111, 111, 116, 58, 32, 51, 53, 54, 0, 6, 1};
    int stringLen = 54;

    int checksum = getChecksum(buffer, &stringLen);
    printf("Checksum: %d\n", checksum);
    

    return 0;
}

uint16_t crc16(const uint8_t *data, size_t length)
{
    uint8_t x;
    uint16_t crc = 0xFFFF;

    while (length--)
    {
        x = crc >> 8 ^ *data++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x << 5)) ^ ((uint16_t)x);
    }
    printf("\n");
    return crc;
}

// Creates a string with base 8 representation of the given string
void convertStringToBase8(const char *string, const int stringLen, uint8_t *base8String)
{
    for (int i = 0; i < stringLen; i++)
    {
        base8String[i] = (uint8_t)string[i];
    }
}

// Appends null terminator and CRC to the given base 8 string and increments stringLen to match the new length.
void appendCrcToBase8String(uint8_t *base8String, int *stringLen)
{
    base8String[*stringLen] = 0;
    uint16_t crc = crc16(base8String, *stringLen);

    base8String[*stringLen + 1] = crc >> 8;   // MSB
    base8String[*stringLen + 2] = crc & 0xFF; // LSB

    *stringLen += 3;
}

int getChecksum(uint8_t *base8String, int *stringLen)
{
    // Locate terminating zero
    int zeroIndex = 0;
    printf("String: ");
    for (int i = 0; i < *stringLen; i++)
    {
        printf("%d ", base8String[i]);
        if (base8String[i] == 0 && i == 0)
        {
            return -1; // String too short to be valid
        }
        else if (base8String[i] == 0)
        {
            printf("\nZero index: %d\n", i);
            zeroIndex = i;
            break;
        }
        else if (i > MAX_LOG_LEN)
        {
            return -1; // String too long to be valid
        }
    }
    printf("\n");

    // Get rid of the terminating zero
    base8String[zeroIndex] = base8String[zeroIndex + 1];
    base8String[zeroIndex + 1] = base8String[zeroIndex + 2];

    *stringLen = zeroIndex + 2;

    printf("String length: %d\n", *stringLen);
    printf("String: ");
    for (int i = 0; i < *stringLen; i++)
    {
        printf("%c", base8String[i]);
    }
    printf("\n");

    return  crc16(base8String, *stringLen);
}

// Reads specified log entry from EEPROM and saves it to the given array
int readLogFromEeprom(const int logEntryToRead, uint8_t *logBuffer, const int logBufferLen){
    int logStartAddr = LOG_START_ADDR + (logEntryToRead * LOG_SIZE);

    if (logStartAddr > LOG_END_ADDR || logStartAddr < LOG_START_ADDR || logBufferLen != LOG_SIZE){
        return -1;
    }

    // Log start address buffer
    uint8_t logStartAddrBuffer[2];
    logStartAddrBuffer[0] = logStartAddr >> 8;
    logStartAddrBuffer[1] = logStartAddr & 0xFF;

    // Read log from EEPROM
    //i2c_write_blocking(i2c_default, EEPROM_ADDR, logStartAddrBuffer, 2, true);
    //sleep_ms(EEPROM_WRITE_DELAY_MS);
    //i2c_read_blocking(i2c_default, EEPROM_ADDR, logBuffer, LOG_SIZE, false);
    //sleep_ms(EEPROM_WRITE_DELAY_MS);
    return 0;
}

void zeroAllLogs(){
    int count = 0;
    uint16_t logAddr = 0;
    
    uint8_t buffer[3] = {0, 0, 0};
    while (count <= MAX_LOGS)
    {
        printf("Writing to address %d\n", logAddr);
        buffer[0] = logAddr >> 8;
        buffer[1] = logAddr & 0xFF;
        //i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 3, true);
        //sleep_ms(EEPROM_WRITE_DELAY_MS);
        logAddr += LOG_SIZE;
        count++;
    }
    
}