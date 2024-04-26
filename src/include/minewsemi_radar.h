#ifndef LIVE_ROOM_SENSOR_MINEWSEMI_RADAR_H
#define LIVE_ROOM_SENSOR_MINEWSEMI_RADAR_H

#ifdef USE_NEW_MINEW_RADAR

#include <stdbool.h>
#include "stdint.h"

/**
 * Initialize the radar sensor
 */
void minewsemi_init(void);

/**
 * Get the current averaged count of detected objects
 * @return the current count of detected objects or -1 if the count is not valid
 */
int16_t minewsemi_get_current_count(void);

/**
 * Tick function to be called periodically
 */
void minewsemi_radar_tick(void);

/**
 * Starts a new radar study/calibration
 */
void minewsemi_start_studying(void);

/**
 * Reset and configure the radar sensor
 */
void minewsemi_reset_and_configure(void);

/**
 * Request a reset of the radar sensor on the next tick
 */
void minewsemi_request_reset_on_next_tick(void);

/**
 * Request the radar sensor to send a message. Sent on the next tick.
 * @param buf Contains the message to send
 * @param len Length of the message
 * @return True if the message was successfully requested, False otherwise
 */
bool minewsemi_request_send_message(uint8_t *buf, uint16_t len);

#endif//USE_NEW_MINEW_RADAR

#endif//LIVE_ROOM_SENSOR_MINEWSEMI_RADAR_H
