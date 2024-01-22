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
#define BRIGHTNESS_ADDR 50
#define EEPROM_WRITE_DELAY_MS 5
#define LED_STATE_ADDR 32768 // Address in the EEPROM to store the LED state
#define BUFFER_SIZE 512

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

static queue_t irqEvents;

int main()
{
    stdio_init_all();
    uint64_t startTime = time_us_64(); // Initialize start time.
    uint64_t actionTime = 0;

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
    if (!readLedStateFromEeprom(&ledStatusStruct))
    {
        printf("Failed to read LED state from EEPROM\n");
        defaultLedStatus(&ledStatusStruct);
        writeLedStateToEeprom(&ledStatusStruct);
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
    fprintf(stdout, "Time since boot: %f\n", (double)(actionTime - startTime) / 1000000);

    gpio_set_irq_enabled_with_callback(ROT_A, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(ROT_SW, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BUTTON1_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BUTTON2_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BUTTON3_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    queue_init(&irqEvents, sizeof(int), BUFFER_SIZE);

    int value = 0;
    int lastValue = 0;

    while (true)
    {
        while (queue_try_remove(&irqEvents, &value))
        {
            // breakout incase queue contains consecutive interrupts.
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
            fprintf(stdout, "Led %d toggled to state %d, seconds since boot: %f\n", lastValue - BUTTON1_PIN + 1, ledStatusStruct.ledState[lastValue - BUTTON1_PIN], (double)(actionTime - startTime) / 1000000);
        }

        // RotA increaste brightness.
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
    return;
}

void toggleLED(uint gpioPin, struct ledStatus *ledStatusStruct)
{
    printf("ToggleLED\n");
    int ledNum = gpioPin - BUTTON1_PIN; // Results in 0, 1 or 2.

    int ledPin = gpioPin + 13; // 13 is the offset between the button and led pins.
    gpio_set_function(ledPin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(ledPin);
    uint chan = pwm_gpio_to_channel(ledPin);

    // Toggled led is off and brightness is 0
    if (ledStatusStruct->ledState[ledNum] == false && ledStatusStruct->brightness == LED_BRIGHT_MIN)
    {
        ledStatusStruct->ledState[ledNum] = !ledStatusStruct->ledState[ledNum]; // Toggle pressed led on.
        ledStatusStruct->brightness = 500;

        // Set all on state leds to 50% brightness.
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

        // Set all on state leds to 50% brightness.
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
        printf("ledStatusStruct->brightness: %d\n", ledStatusStruct->brightness);
    }
    changeBrightness(ledStatusStruct);
}

void decBrightness(struct ledStatus *ledStatusStruct)
{
    if (ledStatusStruct->brightness > LED_BRIGHT_MIN)
    {
        ledStatusStruct->brightness -= LED_BRIGHT_STEP;
        printf("ledStatusStruct->brightness: %d\n", ledStatusStruct->brightness);
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

    return;
}

void changeBrightness(struct ledStatus *ledStatusStruct)
{
    printf("ChangeBrightness\n");
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
    printf("DefaultLedStatus\n");
    ledStatusStruct->ledState[0] = false;
    ledStatusStruct->ledState[1] = true;
    ledStatusStruct->ledState[2] = false;
    ledStatusStruct->brightness = 500;
}

void writeLedStateToEeprom(const struct ledStatus *ledStatusStruct)
{
    printf("\nwriteLedStateToEeprom\n");
    printf("Led 1: %d\n Led 2: %d\n Led 3: %d\n", ledStatusStruct->ledState[0], ledStatusStruct->ledState[1], ledStatusStruct->ledState[2]);
    
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

    i2c_write_blocking(i2c_default, EEPROM_ADDR, buffer, 2, true);
    sleep_ms(EEPROM_WRITE_DELAY_MS);
    i2c_read_blocking(i2c_default, EEPROM_ADDR, buffer, 2, false); // Fills buffer with brightness data

    brightnessDataByte = (buffer[0] << 8) | buffer[1];

    ledStatusStruct->brightness = brightnessDataByte;
}
