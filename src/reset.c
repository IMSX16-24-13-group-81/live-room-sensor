#include "reset.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "pico/printf.h"
#include "multi_printf.h"

static bool reset_requested = false;

/**
 * Handle any reset requests
 */
void reset_request_tick() {
    if (reset_requested) {
        reset_pico();
    }
}

/**
 * Request a reset of the Pico on the next tick
 * This is useful for when you want to reset the Pico but are in a function that is not safe to reset from
 */
void request_pico_reset() {
    reset_requested = true;
}

/**
 * Reset the Pico
 */
_Noreturn void reset_pico() {
    multi_printf("Resetting pico!\n");
    busy_wait_ms(1000);
    cyw43_arch_deinit();
    while (true) {
        watchdog_reboot(0, 0, 0);
        busy_wait_ms(1000);
        printf("Failed to reboot\n");
    }
}