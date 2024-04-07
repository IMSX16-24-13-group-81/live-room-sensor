#ifndef LIVE_ROOM_SENSOR_MICRADAR_H
#define LIVE_ROOM_SENSOR_MICRADAR_H
#include <stdint.h>

/**
 * Get the current averaged count of detected objects
 * @return
 */
int16_t micradar_get_current_count();


/**
 * Initialize the radar sensor
 */
void micradar_init();

#endif //LIVE_ROOM_SENSOR_MICRADAR_H
