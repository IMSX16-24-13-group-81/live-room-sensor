#ifndef LIVE_ROOM_SENSOR_HTTPS_H
#define LIVE_ROOM_SENSOR_HTTPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool send_https_request(const uint8_t *cert, size_t cert_len, const char *server, const char *request, size_t request_len, int timeout);

#endif//LIVE_ROOM_SENSOR_HTTPS_H
