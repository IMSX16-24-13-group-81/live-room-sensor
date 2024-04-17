#ifndef LIVE_ROOM_SENSOR_MINEWSEMI_RADAR_H
#define LIVE_ROOM_SENSOR_MINEWSEMI_RADAR_H

#ifdef USE_NEW_MINEW_RADAR

#include "stdint.h"

void minewsemi_init(void);

int16_t minewsemi_get_current_count(void);

void minewsemi_radar_tick(void)

#endif//USE_NEW_MINEW_RADAR

#endif//LIVE_ROOM_SENSOR_MINEWSEMI_RADAR_H
