#include "reset.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "pico/printf.h"
#include <sys/cdefs.h>

_Noreturn void reset_pico() {
    // Deinit Pico W wireless hardware
    cyw43_arch_deinit();
    printf("Resetting\n");
    busy_wait_ms(1000);
    while (true) {
        watchdog_reboot(0, 0, 0);
        busy_wait_ms(1000);
        printf("Failed to reboot\n");
    }
}