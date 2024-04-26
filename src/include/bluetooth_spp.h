#ifndef LIVE_ROOM_SENSOR_BLUETOOTH_SPP_H
#define LIVE_ROOM_SENSOR_BLUETOOTH_SPP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "_ansi.h"

int btstack_init();


bool bluetooth_printf(const char *__restrict, ...)
_ATTRIBUTE ((__format__(__printf__, 1, 2)));

bool bluetooth_vprintf(const char *format, va_list args);

/**
 * @brief Mark buffer as ready to send
 * @param buffer buffer to send
 * @param actual_size actual size of the buffer
 */
bool mark_buffer_to_send(const uint8_t *buffer, size_t actual_size);

/**
 * @brief Get a free send buffer
 * @param prelim_size preliminary size of the buffer. The actual size of the buffer may be smaller
 * @return buffer or NULL if no buffer is available or size is too large or no client is connected
 */
uint8_t *get_free_send_buffer(size_t prelim_size);

#endif //LIVE_ROOM_SENSOR_BLUETOOTH_SPP_H
