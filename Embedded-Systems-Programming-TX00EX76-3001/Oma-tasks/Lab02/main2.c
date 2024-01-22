#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#define LED_PIN 20
#define ROTARY_ENCODER_A_PIN 10
#define ROTARY_ENCODER_B_PIN 11
#define ROTARY_ENCODER_BUTTON_PIN 12

bool led_state = false;
uint brightness = 128; // Start at 50% brightness
uint max_brightness = 255;

void gpio_callback(uint gpio, uint32_t events) {
    // Handle rotary encoder
    if (gpio == ROTARY_ENCODER_A_PIN || gpio == ROTARY_ENCODER_B_PIN) {
        if (led_state) {
            int a_state = gpio_get(ROTARY_ENCODER_A_PIN);
            int b_state = gpio_get(ROTARY_ENCODER_B_PIN);

            if (a_state != b_state) {
                brightness++;
            } else {
                brightness--;
            }

            if (brightness > max_brightness) {
                brightness = max_brightness;
            } else if (brightness < 0) {
                brightness = 0;
            }

            pwm_set_gpio_level(LED_PIN, brightness);
        }
    }
    // Handle button press
    else if (gpio == ROTARY_ENCODER_BUTTON_PIN) {
        led_state = !led_state;

        if (led_state && brightness == 0) {
            brightness = max_brightness / 2; // Set to 50% brightness
        }

        pwm_set_gpio_level(LED_PIN, led_state ? brightness : 0);
    }
}

int main() {
    stdio_init_all();

    // Set up LED PWM
    gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(LED_PIN);
    pwm_set_wrap(slice_num, max_brightness);
    pwm_set_clkdiv(slice_num, 1.0f); // Set PWM frequency to 1 MHz
    pwm_set_phase_correct(slice_num, true); // Set PWM frequency to 1 kHz

    // Set up rotary encoder inputs
    gpio_set_irq_enabled_with_callback(ROTARY_ENCODER_A_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(ROTARY_ENCODER_B_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // Set up button input
    gpio_set_irq_enabled_with_callback(ROTARY_ENCODER_BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    while (true) {
        tight_loop_contents();
    }

    return 0;
}
