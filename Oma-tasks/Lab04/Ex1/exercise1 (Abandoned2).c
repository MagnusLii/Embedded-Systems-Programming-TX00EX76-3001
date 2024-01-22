#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/util/queue.h"
#include <stdio.h>
#include <stbool.h>

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

#define EEPROM_ADDR 0x50  // I2C address of the EEPROM
#define BRIGHTNESS_ADDR 2
#define EEPROM_WRITE_DELAY_MS 5
#define LED_STATE_ADDR 32768 // Address in the EEPROM to store the LED state
#define BUFFER_SIZE 1024


typedef struct ledstate {
    bool led_state[3];  
    uint16_t brightness; 
} ledstate;

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
    char OnOff[2][10] = {"OFF", "ON"};

    stdio_init_all();

    // init the I2C bus
    i2c_init(i2c_default, 100 * 1000); //100khz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    /*
    // setup button pin for on/off.
    gpio_init(ROT_SW);
    gpio_set_dir(ROT_SW, GPIO_IN);
    gpio_pull_up(ROT_SW);
    */

    // setup button pin for increase.
    gpio_init(ROT_A);
    gpio_set_dir(ROT_A, GPIO_IN);

    // setup button pin for decrease.
    gpio_init(ROT_B);
    gpio_set_dir(ROT_B, GPIO_IN);

    ledstate ls;

    // setup buttons
    for (int i = BUTTON1_PIN; i < BUTTON1_PIN + N_LED; i++){
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_pull_up(i);
    }

    read_led_state_from_eeprom(&ls, LED_STATE_ADDR);  // REWORK THIS FUNC
    sleep_ms(EEPROM_WRITE_DELAY_MS);

    // setup led(s).
    for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++){
        uint slice_num = pwm_gpio_to_slice_num(i);
        uint chan = pwm_gpio_to_channel(i);
        pwm_set_enabled(slice_num, false);
        pwm_config config = pwm_get_default_config();
        pwm_config_set_clkdiv_int(&config, 125);
        pwm_config_set_wrap(&config, 1000); // 1kHz
        pwm_init(slice_num, &config, false);
        pwm_set_chan_level(slice_num, chan, ls.brightness); // brightness
        gpio_set_function(i, GPIO_FUNC_PWM);
        pwm_set_enabled(slice_num, true);
    }
    change_bright();

    gpio_set_irq_enabled_with_callback(ROT_A, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(ROT_SW, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);


    while (1) {
        if (gpio_get(20) == 1)
        {
            updateLedState(&ls, 20);
        }
        

        if (status_changed == true){
            if (ls.state != false){
                change_bright();
                printf("Brightness: %d\n", ls.brightness);
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

void change_bright(struct ledstate *ledstate_struct){
    for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++){
        uint slice_num = pwm_gpio_to_slice_num(i);
        uint chan = pwm_gpio_to_channel(i);
        pwm_set_chan_level(slice_num, chan, ledstate_struct.brightness);
    }
}

void toggle_leds(ledstate *ls){
    if (brightness == 0 && ls->state == true){
        brightness = 500;
        change_bright(ls);
        write_led_state_to_eeprom(ls, LED_STATE_ADDR);
        sleep_ms(EEPROM_WRITE_DELAY_MS);
    } else if (ls->state == false){
        set_led_state(ls, true);
        change_bright(ls);
        write_led_state_to_eeprom(ls, LED_STATE_ADDR);
        sleep_ms(EEPROM_WRITE_DELAY_MS);
    } else if (ls->state == true){
        set_led_state(ls, false);
        write_led_state_to_eeprom(ls, LED_STATE_ADDR);
        sleep_ms(EEPROM_WRITE_DELAY_MS);
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

    else if ((gpio == BUTTON1_PIN || gpio == BUTTON2_PIN || gpio == BUTTON3_PIN) && led_status_changed == false){
        led_status_changed = true;

        //clear release debounce.
        while (debounce_counter < 100000)
        {
            if (gpio_get(BUTTON1_PIN) == 1 && gpio_get(BUTTON2_PIN) == 1 && gpio_get(BUTTON3_PIN) == 1){
                debounce_counter++;
            } else {
                debounce_counter = 0;
            }
        }
    }
}

void toggleLED(uint gpioPin, uint32_t events, struct ledstate *ledstate_struct){
    printf("Toggle led pin %d\n", gpioPin);

    int ledNum = gpioPin - BUTTON1_PIN; // Results in 0, 1 or 2.
    ledstate_struct->led_state[ledNum] = !ledstate_struct->led_state[ledNum];

    int ledPin = gpioPin + 13; // 13 is the offset between the button and led pins.
    gpio_set_function(ledPin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(ledPin);
    pwm_set_enabled(slice_num, ledstate_struct->led_state[ledNum]);
}

// #TODO save brightness to eeprom.
// #TODO rewrite code to use ledstate struct.
// #TODO integrate a circular buffer to handle button.
// #TODO refactor code to remove irrelevant shite.