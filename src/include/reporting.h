#ifndef LIVE_ROOM_SENSOR_REPORTING_H
#define LIVE_ROOM_SENSOR_REPORTING_H

#include <stdbool.h>
#include <stdint.h>

void reporting_init();

void send_sensor_report(int16_t occupants, int16_t radar_state, bool pir_state);

#endif//LIVE_ROOM_SENSOR_REPORTING_H
