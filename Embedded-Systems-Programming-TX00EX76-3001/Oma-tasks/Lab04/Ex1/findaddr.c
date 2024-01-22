#include "pico/stdlib.h"
#include "hardware/i2c.h"

int main() {
    stdio_init_all();
    i2c_init(i2c_default, 100 * 1000);

    for (int addr = 0; addr < 128; addr++) {
        int result = i2c_write_blocking(i2c_default, addr, NULL, 0, false);
        if (result == PICO_ERROR_NO_DATA) {
            printf("No device at address 0x%02x\n", addr);
        } else {
            printf("Found device at address 0x%02x\n", addr);
        }
    }

    return 0;
}
