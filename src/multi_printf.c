#include "multi_printf.h"
#include "bluetooth_spp.h"

#include <stdarg.h>
#include <stdio.h>

/**
 * @brief Print a formatted string to the console and to (if connected) the Bluetooth SPP client
 * @param format The format string
 * @param ... The arguments to the format string
 */
void multi_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    bluetooth_vprintf(format, args);
    va_end(args);
}
