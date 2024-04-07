#include "sensor_controller.h"
#include "micradar.h"
#include "pico/printf.h"
#include "pico/time.h"
#include "pir_sensor.h"
#include "reporting.h"

#define SENSOR_REPORT_INTERVAL_MS 60000

static uint64_t last_report_time = 0;

void sensor_controller_init() {
    pir_sensor_init();
    micradar_init();
}

void sensor_controller_update() {
    if (time_us_64() - last_report_time > SENSOR_REPORT_INTERVAL_MS * 1000) {
        printf("Time to report\n");
        last_report_time = time_us_64();
        int16_t radar_count = micradar_get_current_count();
        bool motion_detected = pir_sensor_is_motion_detected();
        printf("Radar count: %d, Motion detected: %d\n", radar_count, motion_detected);

        int16_t occupants = radar_count < 0 ? 0 : radar_count;
        if (!occupants && motion_detected) {
            occupants++;
        }

        //send_sensor_report(occupants, radar_count, motion_detected);
    }
}