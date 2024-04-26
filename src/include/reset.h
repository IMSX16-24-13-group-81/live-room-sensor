#include <sys/cdefs.h>

#ifndef LIVE_ROOM_SENSOR_RESET_H
#define LIVE_ROOM_SENSOR_RESET_H

/**
 * Handle any reset requests
 */
void reset_request_tick();

/**
 * Request a reset of the Pico on the next tick
 * This is useful for when you want to reset the Pico but are in a function that is not safe to reset from
 */
void request_pico_reset();

/**
 * Reset the Pico
 */
_Noreturn void reset_pico();

#endif//LIVE_ROOM_SENSOR_RESET_H
