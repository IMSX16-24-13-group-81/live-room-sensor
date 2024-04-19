#include "micradar.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "math.h"
#include "pico/printf.h"
#include <string.h>

#ifndef USE_NEW_MINEW_RADAR

#define UART_ID uart1
#define BAUD_RATE 9600
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define RX_BUF_SIZE 256

#define COUNT_AVERAGE_BUFFER_SIZE 256

#define TRAJECTORY_INFO_REPORT 0x8202
#define TRAJECTORY_INFO_REPORT_POINT_SIZE 11

#define COUNT_VALIDITY_TIMEOUT_MS 5000

static volatile uint8_t uart_rx_buf[RX_BUF_SIZE];
static volatile uint8_t uart_rx_buf_head = 0;

static volatile uint8_t previous_counts[COUNT_AVERAGE_BUFFER_SIZE];
static volatile uint32_t previous_counts_head = 0;
static volatile uint32_t previous_counts_sum = 0;
static volatile uint64_t last_count_time = 0;

void append_to_rx_buf(uint8_t c) {
    uart_rx_buf[uart_rx_buf_head] = c;
    uart_rx_buf_head = (uart_rx_buf_head + 1) % RX_BUF_SIZE;
}

bool checksum_is_valid(const uint8_t *buf, uint8_t len) {
    if (len < 5) return false;

    // Checksum is the sum of all bytes except the last 3 bytes of the frame
    // where the third last byte is the checksum itself and the last two bytes are the end of frame bytes
    uint8_t checksum = 0;
    for (int i = 0; i < len - 4; i++) {
        checksum += buf[i];
    }
    return checksum == buf[len - 4];
}

void update_previous_counts(uint8_t count) {
    previous_counts_sum -= previous_counts[previous_counts_head];
    previous_counts[previous_counts_head] = count;
    previous_counts_sum += count;
    previous_counts_head = (previous_counts_head + 1) % COUNT_AVERAGE_BUFFER_SIZE;
    last_count_time = time_us_64();
}

void parse_trajectory_info(const uint8_t *buf, uint8_t len) {
    if (len < 9) return;

    uint16_t message_content_len = buf[4] << 8 | buf[5];

    update_previous_counts(message_content_len / TRAJECTORY_INFO_REPORT_POINT_SIZE);
}

void handle_received_frame() {
    if (uart_rx_buf_head < 5) return;

    if (!checksum_is_valid((uint8_t *) uart_rx_buf, uart_rx_buf_head)) {
        printf("Invalid checksum\n");
        uart_rx_buf_head = 0;
        return;
    }

    uint16_t control_command = uart_rx_buf[2] << 8 | uart_rx_buf[3];
    switch (control_command) {
        case TRAJECTORY_INFO_REPORT:
            parse_trajectory_info((uint8_t *) uart_rx_buf, uart_rx_buf_head);
            break;
        default:
            printf("Unknown radar message received\n");
            break;
    }

    uart_rx_buf_head = 0;
}

void on_uart_rx() {
    uint8_t c;
    while (uart_is_readable(UART_ID)) {
        c = uart_getc(UART_ID);
        if (uart_rx_buf_head == 0 && c != 0x53) {
            // Check for the start of the message frame
            continue;
        } else if (uart_rx_buf_head == 1 && c != 0x59) {
            // Check for the start of the message frame
            uart_rx_buf_head = 0;
            continue;

        } else if (uart_rx_buf_head >= RX_BUF_SIZE - 1) {
            uart_rx_buf_head = 0;
            printf("Radar UART receive buffer full where is should not be possible! Clearing and resetting!\n");
            continue;
        }

        append_to_rx_buf(c);

        if (uart_rx_buf_head > 5 && uart_rx_buf[uart_rx_buf_head - 3] == 0x54 && uart_rx_buf[uart_rx_buf_head - 2] == 0x43) {
            // We have a complete message frame
            handle_received_frame();
            uart_rx_buf_head = 0;
        } else if (uart_rx_buf_head >= RX_BUF_SIZE - 1) {
            uart_rx_buf_head = 0;
            printf("Radar UART receive buffer full without a complete frame found. Clearing and resetting!\n");
            continue;
        }
    }
}

/**
 * Get the current averaged count of detected objects
 * @return the current count of detected objects or -1 if the count is not valid
 */
int16_t micradar_get_current_count() {
    if (time_us_64() - last_count_time > COUNT_VALIDITY_TIMEOUT_MS * 1000) {
        return -1;
    }

    float average = ((float) previous_counts_sum) / (float) COUNT_AVERAGE_BUFFER_SIZE;
    //printf("Average: %f\n", average);

    return roundf(average);
}


/**
 * Initialize the radar sensor
 */
void micradar_init() {
    memset((void *) previous_counts, 0, COUNT_AVERAGE_BUFFER_SIZE);

    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);

    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    uart_set_irq_enables(UART_ID, true, false);
}

#endif//USE_NEW_MINEW_RADAR