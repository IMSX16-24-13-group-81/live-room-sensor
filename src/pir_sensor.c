#include "pir_sensor.h"
#include "hardware/timer.h"
#include "pico/printf.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define PIR_SENSOR_GPIO 28
#define PIR_SENSOR_TIMER_MS 1000
#define PIR_SENSOR_MOTION_TIMEOUT_MS 10000

static repeating_timer_t pir_sensor_timer;

static uint64_t pir_sensor_last_motion_time = 0;

void pir_sensor_interrupt_handler(uint gpio, uint32_t events) {

    if (gpio == PIR_SENSOR_GPIO) {
        pir_sensor_last_motion_time = time_us_64();
        printf("Edge motion detected\n");
    }
}

bool pir_sensor_timer_callback() {

    if (gpio_get(PIR_SENSOR_GPIO) == 1) {
        pir_sensor_last_motion_time = time_us_64();
        printf("Motion still detected\n");
    }

    return true; // keep repeating
}

bool pir_sensor_is_motion_detected() {

    return time_us_64() - pir_sensor_last_motion_time < PIR_SENSOR_MOTION_TIMEOUT_MS * 1000;

}

void pir_sensor_init() {

    gpio_set_irq_enabled_with_callback(PIR_SENSOR_GPIO, GPIO_IRQ_EDGE_RISE, true, &pir_sensor_interrupt_handler);
    add_repeating_timer_ms(PIR_SENSOR_TIMER_MS, &pir_sensor_timer_callback, NULL, &pir_sensor_timer);

}