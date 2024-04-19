#include "minewsemi_radar.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "math.h"
#include "pico/printf.h"
#include "pico/time.h"
#include <string.h>

#ifdef USE_NEW_MINEW_RADAR

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define RX_BUF_SIZE 8192

#define COUNT_AVERAGE_BUFFER_SIZE 16

#define TRAJECTORY_INFO_REPORT 0x8202
#define TRAJECTORY_INFO_REPORT_POINT_SIZE 11

#define COUNT_VALIDITY_TIMEOUT_MS 5000
#define RESET_TIMEOUT_MS 60000

#define MAX_AT_RESPONSE_LENGTH 16

static volatile uint8_t uart_rx_buf[RX_BUF_SIZE];
static volatile uint16_t uart_rx_buf_head = 0;

static volatile uint8_t previous_counts[COUNT_AVERAGE_BUFFER_SIZE];
static volatile uint32_t previous_counts_head = 0;
static volatile uint32_t previous_counts_sum = 0;
static volatile uint64_t last_count_time = 0;

static volatile uint64_t last_reset_time = 0;

typedef struct {
    float x;
    float z;
    float y;
    int8_t v;
    float snr;
    float pow;
    float dpk;

} __attribute__((packed, aligned(1))) radar_point_t;

typedef struct {
    uint32_t id;
    uint32_t q;
    float x;
    float z;
    float y;
    float vx;
    float vz;
    float vy;

} __attribute__((packed, aligned(1))) radar_person_t;

typedef struct {
    uint32_t frame_number;
    uint32_t point_count;
    radar_point_t *points;
    uint32_t person_count;
    radar_person_t *persons;
} radar_frame_t;

void append_to_rx_buf(uint8_t c) {
    uart_rx_buf[uart_rx_buf_head] = c;
    uart_rx_buf_head = (uart_rx_buf_head + 1) % RX_BUF_SIZE;
}

uint32_t uint32_from_buf(const volatile uint8_t *buf) {
    return buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0];
}

void update_previous_counts(uint8_t count) {
    previous_counts_sum -= previous_counts[previous_counts_head];
    previous_counts[previous_counts_head] = count;
    previous_counts_sum += count;
    previous_counts_head = (previous_counts_head + 1) % COUNT_AVERAGE_BUFFER_SIZE;
    last_count_time = time_us_64();
}

void parse_radar_frame(void) {
    radar_frame_t frame;

    frame.frame_number = uint32_from_buf(&uart_rx_buf[12]);

    uint32_t first_tlv = uint32_from_buf(&uart_rx_buf[16]);
    if (first_tlv != 1) {
        printf("Invalid first TLV\n");
        return;
    }

    uint32_t points_size = uint32_from_buf(&uart_rx_buf[20]);

    if (points_size % sizeof(radar_point_t) != 0) {
        printf("Invalid point count\n");
        return;
    }

    frame.point_count = points_size / sizeof(radar_point_t);
    frame.points = (radar_point_t *) &uart_rx_buf[24];

    uint32_t end_of_points = 24 + points_size;

    uint32_t second_tlv = uint32_from_buf(&uart_rx_buf[end_of_points]);
    if (second_tlv != 2) {
        printf("Invalid second TLV\n");
        return;
    }

    uint32_t persons_size = uint32_from_buf(&uart_rx_buf[end_of_points + 4]);

    if (persons_size % sizeof(radar_person_t) != 0) {
        printf("Invalid person count\n");
        return;
    }

    frame.person_count = persons_size / sizeof(radar_person_t);
    frame.persons = (radar_person_t *) &uart_rx_buf[end_of_points + 8];

    //printf("We have %lu points and %lu persons\n", frame.point_count, frame.person_count);
    update_previous_counts(frame.person_count);
}

void handle_AT_response(void) {
    printf("AT response received from radar\n");
    for (int i = 0; i < uart_rx_buf_head; i++) {
        printf("%c", uart_rx_buf[i]);
    }
    printf("\n");
}

void handle_save_para_failed(void) {
    printf("Save Para Failed received from radar\n");
}

void on_uart_rx() {
    uint8_t c;
    while (uart_is_readable(UART_ID)) {
        c = uart_getc(UART_ID);
        // Ether a message is a radar frame, an AT+xxxx response or "Save Para Failed"

        if (uart_rx_buf_head == 0) {
            // Check for the start of the message
            if (c != 0x01 && c != 'A' && c != 'S') {
                continue;
            }
            append_to_rx_buf(c);
            continue;
        } else if (uart_rx_buf_head >= RX_BUF_SIZE - 1) {
            uart_rx_buf_head = 0;
            printf("Radar UART receive buffer full where is should not be possible! Clearing and resetting!\n");
            continue;
        }

        append_to_rx_buf(c);

        switch (uart_rx_buf[0]) {

            case 0x01: {
                // Start of a radar frame is 0x01 0x02 ... 0x08
                if (uart_rx_buf_head < 8 && uart_rx_buf[uart_rx_buf_head - 1] != uart_rx_buf[uart_rx_buf_head - 2] + 1) {
                    // Not a valid frame
                    uart_rx_buf_head = 0;
                    continue;
                }

                if (uart_rx_buf_head > 12) {
                    // We have the frame length
                    uint32_t frame_length = uint32_from_buf(&uart_rx_buf[8]);
                    if (uart_rx_buf_head == frame_length + 1) {
                        // We have a complete frame
                        parse_radar_frame();
                        uart_rx_buf_head = 0;
                        continue;
                    } else if (uart_rx_buf_head > frame_length + 1) {
                        // We have more than one complete frame in the buffer. Should not happen
                        uart_rx_buf_head = 0;
                        printf("More than one radar frame length in buffer!\n");
                        continue;
                    } else if (frame_length > RX_BUF_SIZE) {
                        // Frame length is too long
                        uart_rx_buf_head = 0;
                        printf("Radar frame length too long!\n");
                        continue;
                    }
                }
                break;
            }
            case 'A': {
                if (uart_rx_buf_head == 2 && c != 'T') {
                    // Not an AT response
                    uart_rx_buf_head = 0;
                    continue;
                } else if (uart_rx_buf_head == 3 && c != '+') {
                    // Not an AT response
                    uart_rx_buf_head = 0;
                    continue;
                } else if (c == '\n') {
                    // AT response complete
                    handle_AT_response();
                    uart_rx_buf_head = 0;
                    continue;
                } else if (uart_rx_buf_head >= MAX_AT_RESPONSE_LENGTH) {
                    // AT command too long
                    uart_rx_buf_head = 0;
                    printf("AT command too long!\n");
                    continue;
                }
            }
                //Save Para Failed
            case 'S': {
                if (uart_rx_buf_head > 17) {
                    // Too long, should not happen
                    uart_rx_buf_head = 0;
                    continue;
                } else if (c != "Save Para Failed\n"[uart_rx_buf_head - 1]) {
                    // Not the expected string
                    uart_rx_buf_head = 0;
                    continue;
                } else {
                    // Expected string complete
                    handle_save_para_failed();
                    uart_rx_buf_head = 0;
                    continue;
                }
            }
        }

        if (uart_rx_buf_head >= RX_BUF_SIZE - 1) {
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
int16_t minewsemi_get_current_count(void) {
    if (time_us_64() - last_count_time > COUNT_VALIDITY_TIMEOUT_MS * 1000) {
        return -1;
    }

    float average = ((float) previous_counts_sum) / (float) COUNT_AVERAGE_BUFFER_SIZE;
    //printf("Average: %f\n", average);

    return roundf(average);
}

/**
 * Reset and start the radar sensor
 */
void minewsemi_reset_and_start(void) {
    last_reset_time = time_us_64();
    uart_puts(UART_ID, "AT+RESET\n");
    watchdog_update();
    sleep_ms(2000);
    uart_puts(UART_ID, "AT+START\n");
    watchdog_update();AT+STOP
    sleep_ms(1000);
    uart_puts(UART_ID, "AT+RANGE=1000\n");
    watchdog_update();
    sleep_ms(1000);
    uart_puts(UART_ID, "AT+SENS=10\n");

}AT+SETTING

/**
 * Tick function to be called periodically
 */
void minewsemi_radar_tick(void) {
    // If we have not received a valid count in a while, reset the radar
    if ((time_us_64() - last_reset_time > RESET_TIMEOUT_MS * 1000) &&
        (time_us_64() - last_count_time > COUNT_VALIDITY_TIMEOUT_MS * 10 * 1000)) {
        printf("Resetting radar\n");
        minewsemi_reset_and_start();
    }
}

/**
 * Initialize the radar sensor
 */
void minewsemi_init(void) {

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

    minewsemi_reset_and_start();
}

#endif
