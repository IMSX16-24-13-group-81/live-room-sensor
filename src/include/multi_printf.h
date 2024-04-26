#ifndef LIVE_ROOM_SENSOR_DUAL_PRINTF_H
#define LIVE_ROOM_SENSOR_DUAL_PRINTF_H

#include "_ansi.h"

/**
 * @brief Print a formatted string to the console and to (if connected) the Bluetooth SPP client
 * @param format The format string
 * @param ... The arguments to the format string
 */
void multi_printf(const char *format, ...) __attribute__((format(printf, 1, 2)))
_ATTRIBUTE ((__format__(__printf__, 1, 2)));

#endif //LIVE_ROOM_SENSOR_DUAL_PRINTF_H
