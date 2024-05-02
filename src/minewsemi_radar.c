#include "minewsemi_radar.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "math.h"
#include "multi_printf.h"
#include "pico/time.h"
#include <string.h>

#ifdef USE_NEW_MINEW_RADAR

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define RX_BUF_SIZE 8192

#define COUNT_AVERAGE_BUFFER_SIZE 800

#define TRAJECTORY_INFO_REPORT 0x8202
#define TRAJECTORY_INFO_REPORT_POINT_SIZE 11

#define COUNT_VALIDITY_TIMEOUT_MS 5000
#define RESET_TIMEOUT_MS 60000

#define MAX_AT_RESPONSE_LENGTH 16

#define SEND_REQUEST_BUF_SIZE 256

static const uint8_t STUDYING_DIFFERENT_FROM_FLASH[] = {0x55, 0xAA, 0x06, 0x00, 0xB1, 0xB7};
static const uint8_t STUDYING_DIFFERENT_FROM_4_MINUTES_ABORTING[] = {0x55, 0xAA, 0x06, 0x00, 0xB2, 0xB4};
static const uint8_t STUDYING_SAME_AS_FLASH_SAVING[] = {0x55, 0xAA, 0x06, 0x00, 0xA1, 0xA7};
static const uint8_t STUDYING_SAME_AS_4_MINUTES_AGO[] = {0x55, 0xAA, 0x06, 0x00, 0xA2, 0xA4};
static const uint8_t STUDYING_SAME_FOR_5_PASSES_SAVING[] = {0x55, 0xAA, 0x06, 0x00, 0xA3, 0xA5};

static volatile uint8_t uart_rx_buf[RX_BUF_SIZE];
static volatile uint16_t uart_rx_buf_head = 0;

static volatile uint8_t previous_counts[COUNT_AVERAGE_BUFFER_SIZE];
static volatile uint32_t previous_counts_head = 0;
static volatile uint32_t previous_counts_sum = 0;
static volatile uint64_t last_count_time = 0;

static volatile uint64_t last_reset_time = 0;
static volatile bool studying = false;
static volatile bool reset_requested = false;

static volatile uint8_t send_request_buf[SEND_REQUEST_BUF_SIZE];
static volatile uint16_t send_request_len = 0;

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
        multi_printf("Invalid first TLV\n");
        return;
    }

    uint32_t points_size = uint32_from_buf(&uart_rx_buf[20]);

    if (points_size % sizeof(radar_point_t) != 0) {
        multi_printf("Invalid point count\n");
        return;
    }

    frame.point_count = points_size / sizeof(radar_point_t);
    frame.points = (radar_point_t *) &uart_rx_buf[24];

    uint32_t end_of_points = 24 + points_size;

    uint32_t second_tlv = uint32_from_buf(&uart_rx_buf[end_of_points]);
    if (second_tlv != 2) {
        multi_printf("Invalid second TLV\n");
        return;
    }

    uint32_t persons_size = uint32_from_buf(&uart_rx_buf[end_of_points + 4]);

    if (persons_size % sizeof(radar_person_t) != 0) {
        multi_printf("Invalid person count\n");
        return;
    }

    frame.person_count = persons_size / sizeof(radar_person_t);
    frame.persons = (radar_person_t *) &uart_rx_buf[end_of_points + 8];

    //printf("We have %lu points and %lu persons\n", frame.point_count, frame.person_count);
    update_previous_counts(frame.person_count);
}

void handle_AT_response(void) {
    multi_printf("Received AT response from radar:\n%.*s\n", uart_rx_buf_head, uart_rx_buf);
}

void handle_save_para_failed(void) {
    multi_printf("Save Para Failed received from radar\n");
}

void handle_studying_response(void) {

    if (memcmp((uint8_t *) uart_rx_buf, STUDYING_DIFFERENT_FROM_FLASH, 6) == 0) {
        multi_printf("Studying different from flash, starting new study\n");
    } else if (memcmp((uint8_t *) uart_rx_buf, STUDYING_DIFFERENT_FROM_4_MINUTES_ABORTING, 6) == 0) {
        multi_printf("Studying different from 4 minutes ago, aborting\n");
        studying = false;
        last_reset_time = time_us_64();
    } else if (memcmp((uint8_t *) uart_rx_buf, STUDYING_SAME_AS_FLASH_SAVING, 6) == 0) {
        multi_printf("Studying same as flash, saving\n");
        studying = false;
        last_reset_time = time_us_64();
    } else if (memcmp((uint8_t *) uart_rx_buf, STUDYING_SAME_AS_4_MINUTES_AGO, 6) == 0) {
        multi_printf("Studying same as 4 minutes ago, continuing\n");
    } else if (memcmp((uint8_t *) uart_rx_buf, STUDYING_SAME_FOR_5_PASSES_SAVING, 6) == 0) {
        multi_printf("Studying same for 5 passes, saving\n");
        studying = false;
        last_reset_time = time_us_64();
    } else {
        multi_printf("Unknown studying response\n");
    }
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
            multi_printf("Radar UART receive buffer full where is should not be possible! Clearing and resetting!\n");
            continue;
        }

        append_to_rx_buf(c);

        switch (uart_rx_buf[0]) {

            case 0x01: {
                // Start of a radar frame is 0x01 0x02 ... 0x08
                if (uart_rx_buf_head < 8 &&
                    uart_rx_buf[uart_rx_buf_head - 1] != uart_rx_buf[uart_rx_buf_head - 2] + 1) {
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
                        multi_printf("More than one radar frame length in buffer!\n");
                        continue;
                    } else if (frame_length > RX_BUF_SIZE) {
                        // Frame length is too long
                        uart_rx_buf_head = 0;
                        multi_printf("Radar frame length too long!\n");
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
                    multi_printf("AT command too long!\n");
                    continue;
                }
                break;
            }
                //Studying responses from radar
            case 0x55: {
                if (uart_rx_buf_head == 6) {
                    handle_studying_response();
                    uart_rx_buf_head = 0;
                    continue;
                } else if (uart_rx_buf_head > 6) {
                    // Too long, should not happen
                    uart_rx_buf_head = 0;
                    continue;
                }
                break;
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
                break;
            }
        }

        if (uart_rx_buf_head >= RX_BUF_SIZE - 1) {
            uart_rx_buf_head = 0;
            multi_printf("Radar UART receive buffer full without a complete frame found. Clearing and resetting!\n");
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

    return (int16_t) roundf(average);
}

/**
 * Request a reset of the radar sensor on the next tick
 */
void minewsemi_request_reset_on_next_tick(void) {
    reset_requested = true;
}

/**
 * Request the radar sensor to send a message. Sent on the next tick.
 * @param buf Contains the message to send
 * @param len Length of the message
 * @return True if the message was successfully requested, False otherwise
 */
bool minewsemi_request_send_message(uint8_t *buf, uint16_t len) {
    if (send_request_len > 0) {
        multi_printf("Send request already in progress\n");
        return false;
    }

    if (len > SEND_REQUEST_BUF_SIZE) {
        multi_printf("Send request too long\n");
        return false;
    }

    memcpy((void *) send_request_buf, buf, len);
    if (send_request_buf[len - 1] != '\n') {
        if (len == SEND_REQUEST_BUF_SIZE) {
            multi_printf("Send request too long\n");
            return false;
        }
        send_request_buf[len] = '\n';
        len++;
    }
    send_request_len = len;

    return true;
}

/**
 * Reset and configure the radar sensor
 */
void minewsemi_reset_and_configure(void) {
    uart_puts(UART_ID, "AT+RESET\n");
    watchdog_update();
    sleep_ms(1000);
    uart_puts(UART_ID, "AT+START\n");
    watchdog_update();
    last_reset_time = time_us_64();
}

/**
 * Starts a new radar study/calibration
 */
void minewsemi_start_studying(void) {
    uart_puts(UART_ID, "AT+STUDY\n");
}

/**
 * Tick function to be called periodically
 */
void minewsemi_radar_tick(void) {
    uint64_t now = time_us_64();
    bool radar_timeout = now > ((COUNT_VALIDITY_TIMEOUT_MS * 10 * 1000) + last_count_time);
    bool reset_cooldown = now < ((RESET_TIMEOUT_MS * 1000) + last_reset_time);

    // If we have not received a valid count in a while, reset the radar
    if (reset_requested || (radar_timeout && !reset_cooldown && !studying)) {

        multi_printf("Last count time %llu, Last reset time %llu, Current time %llu, Count limit %llu, Reset limit %llu\n",
                     last_count_time, last_reset_time, now, ((COUNT_VALIDITY_TIMEOUT_MS * 10 * 1000) + last_count_time),
                     ((RESET_TIMEOUT_MS * 1000) + last_reset_time));

        reset_requested = false;
        multi_printf("Resetting radar\n");
        minewsemi_reset_and_configure();
    }

    if (send_request_len > 0) {
        multi_printf("Sending requested message to radar\n");
        uart_write_blocking(UART_ID, (uint8_t *) send_request_buf, send_request_len);
        send_request_len = 0;
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

    minewsemi_reset_and_configure();
}

#endif
