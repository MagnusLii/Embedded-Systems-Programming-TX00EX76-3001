#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/util/queue.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

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
#define LED_STATE_ADDR 32767 // Address in the EEPROM to store the LED state
#define INVERSE_LED_ADDR 31000
#define BUFFER_SIZE 512
#define HEX_MID_VALUE 32768

#define LOG_START_ADDR 0
#define LOG_END_ADDR 2048
#define LOG_SIZE 64
#define MAX_LOGS 32
#define MAX_LOG_LEN 61
#define MIN_LOG_LEN 1

typedef struct ledStatus
{
    bool ledState[3];
    uint16_t brightness;
} ledStatus;

static void gpio_callback(uint gpio, uint32_t event_mask);
void toggleLED(uint gpioPin, struct ledStatus *ledStatusStruct);
void incBrightness(struct ledStatus *ledStatusStruct);
void decBrightness(struct ledStatus *ledStatusStruct);
void buttonReleased(int gpioPin);
void changeBrightness(struct ledStatus *ledStatusStruct);
void writeLedStateToEeprom(const struct ledStatus *ledStatusStruct);
void writeBrightnessToEeprom(const struct ledStatus *ledStatusStruct);
bool readLedStateFromEeprom(struct ledStatus *ledStatusStruct);
void defaultLedStatus(struct ledStatus *ledStatusStruct);
void readBrightnessFromEeprom(struct ledStatus *ledStatusStruct);
void handleCommands();
void printLog(const uint8_t *logBuffer, int logBufferLen, int logEntryToRead);
void appendAddrToString(const uint8_t *string, int *stringLen, uint8_t *finalArray, int address);
void enterLogToEeprom(const char *string, int stringLen);

static queue_t irqEvents;

int main()
{
    stdio_init_all();
    uint64_t startTime = time_us_64(); // Initialize start time.
    uint64_t actionTime;

    ledStatus ledStatusStruct;

    // init the I2C bus
    i2c_init(i2c_default, 100 * 1000); // 100khz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    // setup button pin for increase.
    gpio_init(ROT_A);
    gpio_set_dir(ROT_A, GPIO_IN);

    // setup button pin for decrease.
    gpio_init(ROT_B);
    gpio_set_dir(ROT_B, GPIO_IN);

    // setup buttons
    for (int i = BUTTON1_PIN; i < BUTTON1_PIN + N_LED; i++)
    {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_pull_up(i);
    }

    // Verify LED status.
    if (readLedStateFromEeprom(&ledStatusStruct) == false)
    {
        printf("Failed to read LED state from EEPROM\n");
        defaultLedStatus(&ledStatusStruct);
        writeLedStateToEeprom(&ledStatusStruct);
    }
    else
    {
        readBrightnessFromEeprom(&ledStatusStruct);
    }

    // setup led(s).
    for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++)
    {
        uint slice_num = pwm_gpio_to_slice_num(i);
        pwm_config config = pwm_get_default_config();
        pwm_config_set_clkdiv_int(&config, 125);
        pwm_config_set_wrap(&config, 1000); // 1kHz
        pwm_init(slice_num, &config, false);
        gpio_set_function(i, GPIO_FUNC_PWM);
        pwm_set_enabled(slice_num, true);
    }
    changeBrightness(&ledStatusStruct);

    // Print LED status.
    for (int i = 0; i < 3; i++)
    {
        printf("Led %d: %d\n", i + 1, ledStatusStruct.ledState[i]);
    }
    actionTime = time_us_64();
    fprintf(stdout, "Seconds since boot: %d\n", (int)((double)(actionTime - startTime) / 1000000));

    gpio_set_irq_enabled_with_callback(ROT_A, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(ROT_SW, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BUTTON1_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BUTTON2_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BUTTON3_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    queue_init(&irqEvents, sizeof(int), BUFFER_SIZE);

    int value = 0;
    int lastValue = 0;
    char logstring[MAX_LOG_LEN];

    // Enter "Boot" to log.
    enterLogToEeprom("Boot", 4);

    while (true)
    {
        // handling commands from serial.
        if (uart_is_readable(uart0))
        {
            handleCommands();
        }

        while (queue_try_remove(&irqEvents, &value))
        {
            // breakout in case queue contains consecutive interrupts.
            if (value != lastValue && lastValue != 0)
            {
                break;
            }

            if (value == BUTTON1_PIN || value == BUTTON2_PIN || value == BUTTON3_PIN)
            {
                buttonReleased(value);
            }

            lastValue = value;
        }

        // handling interrupt events.
        // LED toggle buttons.
        if (lastValue == BUTTON1_PIN || lastValue == BUTTON2_PIN || lastValue == BUTTON3_PIN)
        {
            actionTime = time_us_64();
            toggleLED(lastValue, &ledStatusStruct);
            writeLedStateToEeprom(&ledStatusStruct);
            sprintf(logstring, "Led %d toggled to state %d, seconds since boot: %d\n", lastValue - BUTTON1_PIN + 1, ledStatusStruct.ledState[lastValue - BUTTON1_PIN], (int)(actionTime - startTime) / 1000000);
            printf("%s", logstring);
            for (int i = 0; i < strlen(logstring); i++)
            {
                if (logstring[i] == '\n')
                {
                    logstring[i] = '\0';
                }
            }
            enterLogToEeprom(logstring, strlen(logstring));
        }

        // RotA increase brightness.
        if (lastValue == ROT_A)
        {
            incBrightness(&ledStatusStruct);
            writeBrightnessToEeprom(&ledStatusStruct);
        }

        // RotB decrease brightness.
        if (lastValue == ROT_B)
        {
            decBrightness(&ledStatusStruct);
            writeBrightnessToEeprom(&ledStatusStruct);
        }

        // Sanity check.
        if (lastValue != BUTTON1_PIN && lastValue != BUTTON2_PIN && lastValue != BUTTON3_PIN && lastValue != ROT_A && lastValue != ROT_B && lastValue != 0)
        {
            printf("Unknown interrupt event: %d\n", lastValue);
            break;
        }

        // Reset values.
        value = 0;
        lastValue = 0;
    }

    return 0;
}

static void gpio_callback(uint gpio, uint32_t event_mask)
{
    if (gpio == ROT_A)
    {
        if (gpio_get(ROT_B))
        {
            gpio = ROT_B;
        }
    }
    queue_try_add(&irqEvents, &gpio);
}

void toggleLED(uint gpioPin, struct ledStatus *ledStatusStruct)
{
    int ledNum = 0;
    if ((int)gpioPin == BUTTON1_PIN)
    {
        ledNum = 2;
    }
    else if ((int)gpioPin == BUTTON2_PIN)
    {
        ledNum = 1;
    }
    else if ((int)gpioPin == BUTTON3_PIN)
    {
        ledNum = 0;
    }

    int ledPin = ledNum + STARTING_LED; // 20 is the offset
    gpio_set_function(ledPin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(ledPin);
    uint chan = pwm_gpio_to_channel(ledPin);

    // Toggled led is off and brightness is 0
    if (ledStatusStruct->ledState[ledNum] == false && ledStatusStruct->brightness == LED_BRIGHT_MIN)
    {
        ledStatusStruct->ledState[ledNum] = !ledStatusStruct->ledState[ledNum]; // Toggle pressed led on.
        ledStatusStruct->brightness = 500;

        // Set all on state LEDs to 50% brightness.
        for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++)
        {
            if (ledStatusStruct->ledState[i - STARTING_LED] == true)
            {
                slice_num = pwm_gpio_to_slice_num(i);
                chan = pwm_gpio_to_channel(i);
                pwm_set_chan_level(slice_num, chan, ledStatusStruct->brightness);
            }
        }
    }

    // Led is off.
    else if (ledStatusStruct->ledState[ledNum] == false)
    {
        ledStatusStruct->ledState[ledNum] = !ledStatusStruct->ledState[ledNum];
        pwm_set_chan_level(slice_num, chan, ledStatusStruct->brightness);
    }

    // Led is on but brightness is 0.
    else if (ledStatusStruct->ledState[ledNum] == true && ledStatusStruct->brightness == LED_BRIGHT_MIN)
    {
        ledStatusStruct->brightness = 500;

        // Set all on state LEDs to 50% brightness.
        for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++)
        {
            if (ledStatusStruct->ledState[i - STARTING_LED] == true)
            {
                slice_num = pwm_gpio_to_slice_num(i);
                chan = pwm_gpio_to_channel(i);
                pwm_set_chan_level(slice_num, chan, ledStatusStruct->brightness);
            }
        }
    }

    // Led is on and brightness is not 0.
    else
    {
        ledStatusStruct->ledState[ledNum] = !ledStatusStruct->ledState[ledNum];
        pwm_set_chan_level(slice_num, chan, 0);
    }
}

void incBrightness(struct ledStatus *ledStatusStruct)
{
    if (ledStatusStruct->brightness < LED_BRIGHT_MAX)
    {
        ledStatusStruct->brightness += LED_BRIGHT_STEP;
    }

    // Fail check
    if (ledStatusStruct->brightness > HEX_MID_VALUE && ledStatusStruct->brightness > LED_BRIGHT_MAX)
    {
        ledStatusStruct->brightness = LED_BRIGHT_MIN;
    }
    if (ledStatusStruct->brightness < HEX_MID_VALUE && ledStatusStruct->brightness > LED_BRIGHT_MAX)
    {
        ledStatusStruct->brightness = LED_BRIGHT_MAX;
    }
    changeBrightness(ledStatusStruct);
}

void decBrightness(struct ledStatus *ledStatusStruct)
{
    if (ledStatusStruct->brightness > LED_BRIGHT_MIN)
    {
        ledStatusStruct->brightness -= LED_BRIGHT_STEP;
    }

    // Fail check
    if (ledStatusStruct->brightness > HEX_MID_VALUE && ledStatusStruct->brightness > LED_BRIGHT_MAX)
    {
        ledStatusStruct->brightness = LED_BRIGHT_MIN;
    }
    if (ledStatusStruct->brightness < HEX_MID_VALUE && ledStatusStruct->brightness > LED_BRIGHT_MAX)
    {
        ledStatusStruct->brightness = LED_BRIGHT_MAX;
    }
    changeBrightness(ledStatusStruct);
}

void buttonReleased(int gpioPin)
{
    int debounceCounter = 0;

    while (debounceCounter < 100000)
    {
        if (gpio_get(gpioPin) == 1)
        {
            debounceCounter++;
        }
        else
        {
            debounceCounter = 0;
        }
    }
}

void changeBrightness(struct ledStatus *ledStatusStruct)
{
    printf("Brightness: %d\n", ledStatusStruct->brightness);
    for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++)
    {
        if (ledStatusStruct->ledState[i - STARTING_LED] == true)
        {
            uint slice_num = pwm_gpio_to_slice_num(i);
            uint chan = pwm_gpio_to_channel(i);
            pwm_set_chan_level(slice_num, chan, ledStatusStruct->brightness);
        }
    }
}

void defaultLedStatus(struct ledStatus *ledStatusStruct)
{
    ledStatusStruct->ledState[0] = false;
    ledStatusStruct->ledState[1] = true;
    ledStatusStruct->ledState[2] = false;
    ledStatusStruct->brightness = 500;
}

void writeLedStateToEeprom(const struct ledStatus *ledStatusStruct)
{
    uint16_t ledStatusAddress = LED_STATE_ADDR; // 3 bytes for the brightness
    uint16_t inverseLedStatusAddress = INVERSE_LED_ADDR;

    uint8_t ledStatusDataByte = (ledStatusStruct->ledState[0] << 2) | (ledStatusStruct->ledState[1] << 1) | ledStatusStruct->ledState[2];
    uint8_t inverseLedStatusDataByte = (!ledStatusStruct->ledState[0] << 2) | (!ledStatusStruct->ledState[1] << 1) | !ledStatusStruct->ledState[2];

    // Creating buffer to write to EEPROM
    uint8_t buffer[3];
    buffer[0] = (uint8_t)(ledStatusAddress >> 8);
    buffer[1] = (uint8_t)(ledStatusAddress & 0xFF);
    buffer[2] = ledStatusDataByte;

    // Writing LED state to EEPROM
    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 3, false);
    sleep_ms(EEPROM_WRITE_DELAY_MS);

    buffer[0] = (uint8_t)(inverseLedStatusAddress >> 8);
    buffer[1] = (uint8_t)(inverseLedStatusAddress & 0xFF);
    buffer[2] = inverseLedStatusDataByte;

    // Writing inverse LED state to EEPROM
    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 3, false);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
}

bool readLedStateFromEeprom(struct ledStatus *ledStatusStruct)
{
    uint16_t ledStatusAddress = LED_STATE_ADDR; // 3 bytes for the brightness
    uint16_t inverseLedStatusAddress = INVERSE_LED_ADDR;

    uint8_t ledStatusDataByte;
    uint8_t inverseLedStatusDataByte;

    // Creating buffer to hold data address
    uint8_t buffer[2];
    buffer[0] = (uint8_t)(ledStatusAddress >> 8);
    buffer[1] = (uint8_t)(ledStatusAddress & 0xFF);

    // Reading LED state from EEPROM
    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 2, true);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
    i2c_read_blocking(i2c_default, EEPROM_ADDR, &ledStatusDataByte, 1, false);

    buffer[0] = (uint8_t)(inverseLedStatusAddress >> 8);
    buffer[1] = (uint8_t)(inverseLedStatusAddress & 0xFF);

    // Reading inverse LED state from EEPROM
    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 2, true);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
    i2c_read_blocking(i2c_default, EEPROM_ADDR, &inverseLedStatusDataByte, 1, false);

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
            printf("LED state and its inverse do not match\nResetting to default configuration\n");
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
    uint16_t brightnessAddress = BRIGHTNESS_ADDR;

    uint16_t brightnessDataByte = (uint16_t)ledStatusStruct->brightness;

    uint8_t buffer[4];
    buffer[0] = (uint8_t)(brightnessAddress >> 8);
    buffer[1] = (uint8_t)(brightnessAddress & 0xFF);
    buffer[2] = (uint8_t)(brightnessDataByte >> 8);
    buffer[3] = (uint8_t)(brightnessDataByte & 0xFF);

    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 4, false);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
}

void readBrightnessFromEeprom(struct ledStatus *ledStatusStruct)
{
    uint16_t brightnessAddress = BRIGHTNESS_ADDR;

    uint16_t brightnessDataByte;

    // Filling buffer with brightness address
    uint8_t buffer[2];
    buffer[0] = (uint8_t)(brightnessAddress >> 8);
    buffer[1] = (uint8_t)(brightnessAddress & 0xFF);

    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 2, true);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
    i2c_read_blocking(i2c_default, EEPROM_ADDR, buffer, 2, false); // Fills buffer with brightness data

    brightnessDataByte = (buffer[0] << 8) | buffer[1];

    // Fail check
    if ((int)brightnessDataByte < LED_BRIGHT_MIN || (int)brightnessDataByte > LED_BRIGHT_MAX)
    {
        brightnessDataByte = (LED_BRIGHT_MIN + LED_BRIGHT_MAX) / 2;
    }

    ledStatusStruct->brightness = (int)brightnessDataByte;
}

// Ex2 stuff
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

// Calculates the checksum of the given base 8 string, validates its length and returns the checksum.
int getChecksum(uint8_t *base8String, int *stringLen)
{
    // Locate terminating zero
    int zeroIndex = 0;

    for (int i = 0; i < *stringLen; i++)
    {
        if (base8String[i] == 0)
        {
            zeroIndex = i;
            break;
        }
    }

    if (zeroIndex < MIN_LOG_LEN || zeroIndex > MAX_LOG_LEN)
    {
        return -1; // String too long or too short to be valid
    }

    // Get rid of the terminating zero
    base8String[zeroIndex] = base8String[zeroIndex + 1];
    base8String[zeroIndex + 1] = base8String[zeroIndex + 2];
    *stringLen = zeroIndex + 2;

    return crc16(base8String, *stringLen);
}

// Reads specified log entry from EEPROM and saves it to the given array
int readLogFromEeprom(const int logEntryToRead, uint8_t *logBuffer, const int logBufferLen)
{
    int logStartAddr = LOG_START_ADDR + (logEntryToRead * LOG_SIZE);

    if (logStartAddr > LOG_END_ADDR || logStartAddr < LOG_START_ADDR || logBufferLen != LOG_SIZE)
    {
        return -1;
    }

    // Log start address buffer
    uint8_t logStartAddrBuffer[2];
    logStartAddrBuffer[0] = logStartAddr >> 8;
    logStartAddrBuffer[1] = logStartAddr & 0xFF;

    // Read log from EEPROM
    i2c_write_blocking(i2c_default, EEPROM_ADDR, logStartAddrBuffer, 2, true);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
    i2c_read_blocking(i2c_default, EEPROM_ADDR, logBuffer, LOG_SIZE, false);
    sleep_ms(EEPROM_WRITE_DELAY_MS);

    return 0;
}

void zeroAllLogs()
{
    printf("Clearing all logs\n");
    int count = 0;
    uint16_t logAddr = 0;

    uint8_t buffer[3] = {0, 0, 0};
    while (count <= 32)
    {
        buffer[0] = logAddr >> 8;
        buffer[1] = logAddr & 0xFF;
        i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 3, false);
        sleep_ms(EEPROM_WRITE_DELAY_MS);
        logAddr += LOG_SIZE;
        count++;
    }
    printf("Logs cleared\n");
}

void handleCommands()
{
    sleep_ms(100); // Wait for the command to be fully received
    uint8_t buffer[5];
    char uartread[5];

    int index = 0;
    while (uart_is_readable(uart0))
    {
        buffer[index] = uart_getc(uart0);
        index++;
    }

    int tempLen = LOG_SIZE;

    for (int i = 0; i < 5; i++)
    {
        uartread[i] = (char)buffer[i];
    }

    // If command is "read"
    if (strncmp(uartread, "read", 4) == 0)
    {
        printf("Printing all logs\n");
        uint8_t logBuffer[LOG_SIZE];
        for (int i = 0; i < MAX_LOGS; i++)
        {
            readLogFromEeprom(i, logBuffer, LOG_SIZE);
            if (getChecksum(logBuffer, &tempLen) == 0)
            {
                printLog(logBuffer, tempLen - 2, i + 1);
                tempLen = LOG_SIZE;
            }
        }
        printf("All logs printed\n");
    }

    // If command is "erase"
    else if (strncmp(uartread, "erase", 5) == 0)
    {
        zeroAllLogs();
    }
}

void printLog(const uint8_t *logBuffer, const int logBufferLen, const int logEntryToRead)
{
    printf("Log %d: ", logEntryToRead);
    for (int i = 0; i < logBufferLen; i++)
    {
        printf("%c", (char)logBuffer[i]);
    }
    printf("\n");
}

void appendAddrToString(const uint8_t *string, int *stringLen, uint8_t *finalArray, const int address)
{
    uint16_t address16 = address;
    uint8_t finalBuffer[2];

    memcpy(finalArray + 2, string, *stringLen);
    *stringLen += 2;

    finalBuffer[0] = address16 >> 8;
    finalBuffer[1] = address16 & 0xFF;
    finalArray[0] = finalBuffer[0];
    finalArray[1] = finalBuffer[1];
}

void enterLogToEeprom(const char *string, const int stringLen)
{
    char logString[stringLen + 1];
    strncpy(logString, string, stringLen);
    logString[stringLen] = '\0';

    int logStringLen = strlen(logString);
    uint8_t base8LogString[LOG_SIZE];
    uint8_t base8LogStringFinal[LOG_SIZE + 2]; // 2 bytes for the address

    // Find the first empty log
    int logIndex = 0;
    int charsToRead = LOG_SIZE;
    do
    {
        charsToRead = LOG_SIZE;
        readLogFromEeprom(logIndex, base8LogString, LOG_SIZE);
        logIndex++;
    } while (getChecksum(base8LogString, &charsToRead) == 0 && logIndex <= MAX_LOGS);
    logIndex--;

    // If all logs are full, erase them all
    if (logIndex == MAX_LOGS)
    {
        zeroAllLogs();
        logIndex = 0;
    }

    // Create base8 log string.
    logStringLen = strlen(logString); // Reset logStringLen
    convertStringToBase8(logString, logStringLen, base8LogString);
    appendCrcToBase8String(base8LogString, &logStringLen);
    appendAddrToString(base8LogString, &logStringLen, base8LogStringFinal, logIndex * LOG_SIZE);

    // Write log to EEPROM
    i2c_write_blocking(i2c_default, EEPROM_ADDR, base8LogStringFinal, logStringLen, false);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
}
