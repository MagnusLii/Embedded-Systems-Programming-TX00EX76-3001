#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <stdio.h>

#define ROT_A 10
#define ROT_B 11
#define ROT_SW 12

#define N_LED 3
#define STARTING_LED 20
#define LED_BRIGHT_MAX 999
#define LED_BRIGHT_MIN 0
#define LED_BRIGHT_STEP 10

volatile bool led_state = true;
volatile uint brightness = 500;
volatile bool status_changed = false;
volatile bool led_status_changed = false;

void change_bright(){
    for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++){
        uint slice_num = pwm_gpio_to_slice_num(i);
        uint chan = pwm_gpio_to_channel(i);
        pwm_set_chan_level(slice_num, chan, brightness);
    }
}

void toggle_leds(){
    if (brightness == 0 && led_state == true){
        brightness = 500;
        change_bright();
    } else if (led_state == false){
        led_state = true;
        change_bright();
    } else if (led_state == true){
        led_state = false;
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

int main(){
    char OnOff[2][10] = {"OFF", "ON"};

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
    change_bright();

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

    stdio_init_all();

    while (1) {
        if (status_changed == true){
            if (led_state != false){
                change_bright();
                printf("Brightness: %d\n", brightness);
            }
            status_changed = false;
        }
        if (led_status_changed == true){
            toggle_leds();
            printf("LEDs: %s\n", OnOff[led_state]);
            led_status_changed = false;
        }
    }
    return 0;
}