#ifndef LIVE_ROOM_SENSOR_PIR_SENSOR_H
#define LIVE_ROOM_SENSOR_PIR_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

bool pir_sensor_is_motion_detected();

void pir_sensor_init();

#endif //LIVE_ROOM_SENSOR_PIR_SENSOR_H
