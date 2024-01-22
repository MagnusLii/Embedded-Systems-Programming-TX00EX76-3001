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
#define LED_STATE_ADDR 1  // Address in the EEPROM to store the LED state
#define SDA_PIN 17
#define SCL_PIN 16

typedef struct ledstate {
    bool state;  // The actual state of the LEDs
    bool not_state;  // The inverted state of the LEDs
} ledstate;

volatile uint brightness = 500;
volatile bool status_changed = false;
volatile bool led_status_changed = false;

void set_led_state(ledstate *ls, bool value);
bool led_state_is_valid(ledstate *ls);
void write_led_state_to_eeprom(ledstate *ls, uint16_t mem_addr);
void read_led_state_from_eeprom(ledstate *ls, uint16_t mem_addr);
void change_bright();
void toggle_leds(ledstate *ls);
void gpio_callback(uint gpio, uint32_t events);


int main(){
    stdio_init_all();

    // init the I2C bus
    i2c_init(i2c_default, 100 * 1000); //100khz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    char OnOff[2][10] = {"OFF", "ON"};

    // setup button pin for on/off.
    gpio_init(ROT_SW);
    gpio_set_dir(ROT_SW, GPIO_IN);
    gpio_pull_up(ROT_SW);

    // setup button pin for increase.
    gpio_init(ROT_A);
    gpio_set_dir(ROT_A, GPIO_IN);

    // setup button pin for decrease.
    gpio_init(ROT_B);
    gpio_set_dir(ROT_B, GPIO_IN);

    gpio_set_irq_enabled_with_callback(ROT_A, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(ROT_SW, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    ledstate ls;

    // If the LED state is not valid, set all LEDs on and write to EEPROM
    /*if (!led_state_is_valid(&ls)) {
        printf("LED state is not valid\n");
        set_led_state(&ls, true);  // LEDs on
        write_led_state_to_eeprom(&ls, 1);
        sleep_ms(10);
    }*/

    read_led_state_from_eeprom(&ls, LED_STATE_ADDR);
    sleep_ms(10);

    // setup led(s).
    for (int led_pin = STARTING_LED; led_pin < STARTING_LED + N_LED; led_pin++){
        uint slice_num = pwm_gpio_to_slice_num(led_pin);
        pwm_set_enabled(slice_num, false);
        pwm_config config = pwm_get_default_config();
        pwm_config_set_clkdiv_int(&config, 125);
        pwm_config_set_wrap(&config, 1000); // 1kHz
        pwm_init(slice_num, &config, false);
        gpio_set_function(led_pin, GPIO_FUNC_PWM);
        pwm_set_enabled(slice_num, true);
    }
    
    if (ls.state == false){
        brightness = 0;
    } else {
        brightness = 500;
    }
    change_bright();

    while (1) {
        if (status_changed == true){
            if (ls.state != false){
                change_bright();
                printf("Brightness: %d\n", brightness);
            }
            status_changed = false;
        }
        if (led_status_changed == true){
            toggle_leds(&ls);
            write_led_state_to_eeprom(&ls, LED_STATE_ADDR);
            sleep_ms(10);
            read_led_state_from_eeprom(&ls, LED_STATE_ADDR);
            sleep_ms(10);
            printf("ls.state: %s\n", OnOff[ls.state]);
            led_status_changed = false;
        }
    }
    return 0;
}


// Function to set the LED state and its inverted value in the ledstate structure
void set_led_state(ledstate *ls, bool value) {
    ls->state = value;
    ls->not_state = !value;
}

// Function to check if the LED state and its inverted value match
bool led_state_is_valid(ledstate *ls) {
    printf("state: %d, not_state: %d\n", ls->state, ls->not_state);
    return ls->state == !ls->not_state;
}

// Function to write the LED state to the EEPROM
void write_led_state_to_eeprom(ledstate *ls, uint16_t mem_addr) {
    printf("writing\n");
    uint8_t data[2] = {ls->state, ls->not_state};
    printf("data: %d, %d\n", data[0], data[1]);
    uint8_t reg_addr[2] = {mem_addr >> 8, mem_addr & 0xFF};  // High and low bytes of the EEPROM address
    uint8_t combined[4] = {reg_addr[0], reg_addr[1], data[0], data[1]};  // Combine the register address and data {[led state], [not led state]}
    i2c_write_blocking(i2c_default, EEPROM_ADDR, combined, 4, false);
}

// Function to read the LED state from the EEPROM
void read_led_state_from_eeprom(ledstate *ls, uint16_t mem_addr) {
    printf("reading\n");
    uint8_t reg_addr[2] = {mem_addr >> 8, mem_addr & 0xFF};  // High and low bytes of the EEPROM address
    i2c_write_blocking(i2c_default, EEPROM_ADDR, reg_addr, 2, true);  // Write the register address with nostop=true
    uint8_t data[2];
    i2c_read_blocking(i2c_default, EEPROM_ADDR, data, 2, false);
    ls->state = data[0];
    ls->not_state = data[1];
    printf("data: %d, %d\n", data[0], data[1]);
}

void change_bright(){
    for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++){
        uint slice_num = pwm_gpio_to_slice_num(i);
        uint chan = pwm_gpio_to_channel(i);
        pwm_set_chan_level(slice_num, chan, brightness);
    }
}

void toggle_leds(ledstate *ls){
    if (brightness == 0 && ls->state == true){
        brightness = 500;
        change_bright();
        write_led_state_to_eeprom(ls, LED_STATE_ADDR);
        sleep_ms(10);
    } else if (ls->state == false){
        set_led_state(ls, true);
        change_bright();
        write_led_state_to_eeprom(ls, LED_STATE_ADDR);
        sleep_ms(10);
    } else if (ls->state == true){
        set_led_state(ls, false);
        write_led_state_to_eeprom(ls, LED_STATE_ADDR);
        sleep_ms(10);
        for (int led_pin = STARTING_LED; led_pin < STARTING_LED + N_LED; led_pin++){
            uint slice_num = pwm_gpio_to_slice_num(led_pin);
            uint chan = pwm_gpio_to_channel(led_pin);
            pwm_set_chan_level(slice_num, chan, 0);
        }
    }
}

void gpio_callback(uint gpio, uint32_t events){
    int debounce_counter = 0;

    if (gpio == ROT_A){
        if (gpio_get(ROT_B)) {
            if (brightness > LED_BRIGHT_MIN){
                brightness -= LED_BRIGHT_STEP;
            }
        } else {
            if (brightness < LED_BRIGHT_MAX){
                brightness += LED_BRIGHT_STEP;
            }
        }
        status_changed = true;
    }

    else if (gpio == ROT_SW && led_status_changed == false){
        led_status_changed = true;

        //clear release debounce.
        while (debounce_counter < 100000)
        {
            if (gpio_get(ROT_SW) == 1){
                debounce_counter++;
            } else {
                debounce_counter = 0;
            }
        }
    }
}

// #TODO save brightness to eeprom.
// #TODO rewrite code to use ledstate struct.
// #TODO integrate a circular buffer to handle button.
// #TODO refactor code to remove irrelevant shite.