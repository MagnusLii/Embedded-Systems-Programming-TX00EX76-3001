#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <stdio.h>

#define BUTTON_ON_OFF 8
#define BUTTON_INC 9
#define BUTTON_DEC 7

#define N_LED 3
#define STARTING_LED 20
#define STARTING_DUTYCYCLE 200
#define LED_DUTYCYCLE_STEP 100
#define LED_DUTYCYCLE_MAX 999
#define LED_DUTYCYCLE_MIN 1

void setup_pwm(uint gpio_pin) {
    uint slice_num = pwm_gpio_to_slice_num(gpio_pin);
    uint channel = pwm_gpio_to_channel(gpio_pin);
    pwm_set_enabled(slice_num, false);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&config, 125);
    pwm_config_set_wrap(&config, 1000); // 1kHz
    pwm_init(slice_num, &config, false);
    pwm_set_chan_level(slice_num, channel, 500); // 50% duty cycle
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    pwm_set_enabled(slice_num, true);
}

void inc_dutycycle(int *dutycycle){
    if (*dutycycle < LED_DUTYCYCLE_MAX){
        *dutycycle = *dutycycle + LED_DUTYCYCLE_STEP;
        if (*dutycycle > LED_DUTYCYCLE_MAX)
        {
            *dutycycle = LED_DUTYCYCLE_MAX;
        }
    }
}

void dec_dutycycle(int *dutycycle){
    if (*dutycycle > LED_DUTYCYCLE_MIN){
        *dutycycle = *dutycycle - LED_DUTYCYCLE_STEP;
        if (*dutycycle < LED_DUTYCYCLE_MIN)
        {
            *dutycycle = 0;
        }
    }
}

void pwm_set_freq_duty(uint slice_num, uint chan, int dutycycle){
    pwm_set_chan_level(slice_num, chan, dutycycle);
}

void turn_on_leds(const int dutycycle){
    printf("turn_on_leds: %d\n", dutycycle);
    for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++){
        gpio_set_function(i, GPIO_FUNC_PWM);
        uint slice_num = pwm_gpio_to_slice_num(i);
        uint chan = pwm_gpio_to_channel(i);
        pwm_set_freq_duty(slice_num, chan, dutycycle);
    }
}

void turn_off_leds(){
    printf("turn_off_leds\n");
    for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++){
        gpio_set_function(i, GPIO_FUNC_PWM);
        uint slice_num = pwm_gpio_to_slice_num(i);
        uint chan = pwm_gpio_to_channel(i);
        pwm_set_freq_duty(slice_num, chan, 0);
    }
}


int main(){
    char OnOff[2][10] = {"OFF", "ON"};
    int dutycycle = STARTING_DUTYCYCLE;

    // setup led(s).
    for (int i = STARTING_LED; i < STARTING_LED + N_LED; i++){
        uint slice_num = pwm_gpio_to_slice_num(i);
        uint chan = pwm_gpio_to_channel(i);
        pwm_set_enabled(slice_num, false);
        pwm_config config = pwm_get_default_config();
        pwm_config_set_clkdiv_int(&config, 125);
        pwm_config_set_wrap(&config, 1000); // 1kHz
        pwm_init(slice_num, &config, false);
        pwm_set_chan_level(slice_num, chan, 500); // 50% duty cycle
        gpio_set_function(i, GPIO_FUNC_PWM);
        pwm_set_enabled(slice_num, true);
        pwm_set_freq_duty(slice_num, chan, dutycycle);
    }
    bool led_state = true;

    // setup button pin for on/off.
    gpio_init(BUTTON_ON_OFF);
    gpio_set_dir(BUTTON_ON_OFF, GPIO_IN);
    gpio_pull_up(BUTTON_ON_OFF);

    // setup button pin for increase.
    gpio_init(BUTTON_INC);
    gpio_set_dir(BUTTON_INC, GPIO_IN);
    gpio_pull_up(BUTTON_INC);

    // setup button pin for decrease.
    gpio_init(BUTTON_DEC);
    gpio_set_dir(BUTTON_DEC, GPIO_IN);
    gpio_pull_up(BUTTON_DEC);

    stdio_init_all();

    while (1){
        if (gpio_get(BUTTON_ON_OFF) == 0){
            sleep_ms(250);
            if (led_state == false){
                led_state = true;
                turn_on_leds(dutycycle);
            }
            else if (led_state == true){
                if (dutycycle == 0){
                    turn_on_leds(500);
                    dutycycle = 500;
                }
                else{
                    led_state = false;
                    turn_off_leds();
                }
            }
            while (gpio_get(BUTTON_ON_OFF) == 0);
            printf("Led state: %s\n", OnOff[led_state]);
            sleep_ms(250);
        }
        if (led_state == true && gpio_get(BUTTON_INC) == 0){
            sleep_ms(250);
            inc_dutycycle(&dutycycle);
            turn_on_leds(dutycycle);
            while (gpio_get(BUTTON_INC) == 0);
            sleep_ms(250);
        }
        if (led_state == true && gpio_get(BUTTON_DEC) == 0){
            sleep_ms(250);
            dec_dutycycle(&dutycycle);
            turn_on_leds(dutycycle);
            while (gpio_get(BUTTON_DEC) == 0);
            sleep_ms(250);
        }
    }
}
