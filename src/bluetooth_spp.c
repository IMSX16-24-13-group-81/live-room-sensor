#include "bluetooth_spp.h"


/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "multi_printf.h"

#ifdef USE_NEW_MINEW_RADAR

#include "minewsemi_radar.h"
#include "reset.h"

#endif

#define COMMAND_PREFIX "AT+"
#define COMMAND_PREFIX_SIZE (sizeof(COMMAND_PREFIX) - 1)
#define COMMAND_POSTFIX "\r\n"
#define COMMAND_POSTFIX_SIZE (sizeof(COMMAND_POSTFIX) - 1)
#define COMMAND_START_MINEW_RADAR_STUDY "MINEW-STUDY"
#define COMMAND_START_MINEW_RADAR_STUDY_SIZE (sizeof(COMMAND_START_MINEW_RADAR_STUDY) - 1)
#define COMMAND_RESET_MINEW_RADAR "MINEW-RESET"
#define COMMAND_RESET_MINEW_RADAR_SIZE (sizeof(COMMAND_RESET_MINEW_RADAR) - 1)
#define COMMAND_SEND_COMMAND_TO_MINW_RADAR "MINEW-COMMAND="
#define COMMAND_SEND_COMMAND_TO_MINW_RADAR_SIZE (sizeof(COMMAND_SEND_COMMAND_TO_MINW_RADAR) - 1)
#define COMMAND_RESET_PICO "PICO-RESET"
#define COMMAND_RESET_PICO_SIZE (sizeof(COMMAND_RESET_PICO) - 1)

#define RFCOMM_SERVER_CHANNEL 1

#define MAX_SEND_MESSAGE_SIZE 1000
#define SEND_QUEUE_SIZE 10

typedef enum {
    SEND_MESSAGE_FREE,
    SEND_MESSAGE_RESERVED,
    SEND_MESSAGE_SEND
} send_status_t;

typedef struct {
    uint32_t send_id;
    send_status_t status;
    uint8_t buffer[MAX_SEND_MESSAGE_SIZE];
    uint16_t send_len;
} send_queue_entry_t;


static volatile uint32_t current_send_id = 1;
static send_queue_entry_t send_queue[SEND_QUEUE_SIZE];


static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t rfcomm_channel_id;
static bool rfcomm_user_has_authenticated;
static uint8_t spp_service_buffer[150];
static btstack_packet_callback_registration_t hci_event_callback_registration;

bool bluetooth_vprintf(const char *format, va_list args) {
    uint8_t *buffer = get_free_send_buffer(MAX_SEND_MESSAGE_SIZE);
    if (buffer == NULL) return false;
    int size = vsnprintf((char *) buffer, MAX_SEND_MESSAGE_SIZE, format, args);
    return mark_buffer_to_send(buffer, size);
}

bool bluetooth_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    bool result = bluetooth_vprintf(format, args);
    va_end(args);
    return result;
}

/**
 * @brief Get a free send buffer
 * @param prelim_size preliminary size of the buffer. The actual size of the buffer may be smaller
 * @return buffer or NULL if no buffer is available or size is too large or no client is connected
 */
uint8_t *get_free_send_buffer(size_t prelim_size) {
    if (prelim_size > MAX_SEND_MESSAGE_SIZE || !rfcomm_channel_id || !rfcomm_user_has_authenticated) return NULL;

    for (int i = 0; i < SEND_QUEUE_SIZE; i++) {
        if (send_queue[i].status == SEND_MESSAGE_FREE) {
            send_queue[i].status = SEND_MESSAGE_RESERVED;
            send_queue[i].send_id = current_send_id++;
            send_queue[i].send_len = prelim_size;
            return send_queue[i].buffer;
        }
    }
    return NULL;
}

/**
 * @brief Mark buffer as ready to send
 * @param buffer buffer to send
 * @param actual_size actual size of the buffer
 */
bool mark_buffer_to_send(uint8_t *buffer, size_t actual_size) {

    for (int i = 0; i < SEND_QUEUE_SIZE; i++) {
        if (send_queue[i].buffer == buffer) {
            if (!rfcomm_channel_id || actual_size > MAX_SEND_MESSAGE_SIZE) {
                send_queue[i].status = SEND_MESSAGE_FREE;
                return false;
            }
            send_queue[i].status = SEND_MESSAGE_SEND;
            send_queue[i].send_len = actual_size;
            rfcomm_request_can_send_now_event(rfcomm_channel_id);
            return true;
        }
    }

    return false;
}

/**
 * @brief Callback for RFCOMM events.
 * Sends one message from the queue if possible and requests another can send now event if more messages are ready to send
 */
void rfcomm_send_now_event() {
    uint32_t lowest_message_id = 0xffffffff;
    uint32_t lowest_message_index = 0;
    uint32_t messages_ready_to_send = 0;
    for (int i = 0; i < SEND_QUEUE_SIZE; ++i) {
        if (send_queue[i].status == SEND_MESSAGE_SEND) {
            if (send_queue[i].send_id <= lowest_message_id) {
                lowest_message_id = send_queue[i].send_id;
                lowest_message_index = i;
                messages_ready_to_send++;
            }
        }
    }

    if (!messages_ready_to_send) return;

    send_queue_entry_t *entry = &send_queue[lowest_message_index];
    rfcomm_send(rfcomm_channel_id, entry->buffer, entry->send_len);
    entry->status = SEND_MESSAGE_FREE;

    if (messages_ready_to_send > 1) {
        rfcomm_request_can_send_now_event(rfcomm_channel_id);
    }

}

void process_received_message(uint8_t *packet, uint16_t size) {
    if (!rfcomm_user_has_authenticated) {
        if (size == 6 && memcmp(packet, "0000\r\n", 4) == 0) {
            rfcomm_user_has_authenticated = true;
            bluetooth_printf("Authenticated\n");
            bluetooth_printf("Version: %s\n", FIRMWARE_VERSION);
#ifdef USE_NEW_MINEW_RADAR
            bluetooth_printf("With Minew radar support\n");
#else
            bluetooth_printf("With micradar support\n");
#endif

            printf("Bluetooth user authenticated\n");
        } else {
            printf("Bluetooth user failed to authenticate\n");
            rfcomm_disconnect(rfcomm_channel_id);
        }
        return;
    }

    if (size < COMMAND_PREFIX_SIZE + COMMAND_POSTFIX_SIZE ||
        memcmp(packet, COMMAND_PREFIX, COMMAND_PREFIX_SIZE) != 0 ||
        memcmp(packet + size - COMMAND_POSTFIX_SIZE, COMMAND_POSTFIX, COMMAND_POSTFIX_SIZE) != 0) {
        multi_printf("Invalid command\n");
        return;
    }

    uint8_t *command = packet + COMMAND_PREFIX_SIZE;
    uint16_t command_size = size - (COMMAND_PREFIX_SIZE + COMMAND_POSTFIX_SIZE);

#ifdef USE_NEW_MINEW_RADAR

    if (command_size == COMMAND_START_MINEW_RADAR_STUDY_SIZE &&
        memcmp(command, COMMAND_START_MINEW_RADAR_STUDY, COMMAND_START_MINEW_RADAR_STUDY_SIZE) == 0) {
        multi_printf("Starting Minew radar calibration\n");
        minewsemi_start_studying();
        return;
    }

    if (command_size == COMMAND_RESET_MINEW_RADAR_SIZE &&
        memcmp(command, COMMAND_RESET_MINEW_RADAR, COMMAND_RESET_MINEW_RADAR_SIZE) == 0) {
        multi_printf("Resetting Minew radar\n");
        minewsemi_request_reset_on_next_tick();
        return;
    }

    if (command_size > COMMAND_SEND_COMMAND_TO_MINW_RADAR_SIZE &&
        memcmp(command, COMMAND_SEND_COMMAND_TO_MINW_RADAR, COMMAND_SEND_COMMAND_TO_MINW_RADAR_SIZE) == 0) {
        multi_printf("Sending command to Minew radar\n");
        bool success = minewsemi_request_send_message(command + COMMAND_SEND_COMMAND_TO_MINW_RADAR_SIZE,
                                                      command_size - COMMAND_SEND_COMMAND_TO_MINW_RADAR_SIZE);
        if (!success) {
            multi_printf("Failed to send message\n");
        }
        return;
    }

#endif

    if (command_size == COMMAND_RESET_PICO_SIZE && memcmp(command, COMMAND_RESET_PICO, COMMAND_RESET_PICO_SIZE) == 0) {
        multi_printf("Setting reset Pico request flag!\n");
        request_pico_reset();
        return;
    }

    bluetooth_printf("Unknown command\n");
}


/* @section SPP Service Setup
 *s
 * @text To provide an SPP service, the L2CAP, RFCOMM, and SDP protocol layers
 * are required. After setting up an RFCOMM service with channel nubmer
 * RFCOMM_SERVER_CHANNEL, an SDP record is created and registered with the SDP server.
 * Example code for SPP service setup is
 * provided in Listing SPPSetup. The SDP record created by function
 * spp_create_sdp_record consists of a basic SPP definition that uses the provided
 * RFCOMM channel ID and service name. For more details, please have a look at it
 * in \path{src/sdp_util.c}.
 * The SDP record is created on the fly in RAM and is deterministic.
 * To preserve valuable RAM, the result could be stored as constant data inside the ROM.
 */

/* LISTING_START(SPPSetup): SPP service setup */
static void spp_service_setup(void) {

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    rfcomm_init();
    rfcomm_register_service(packet_handler, RFCOMM_SERVER_CHANNEL, 0xffff);  // reserved channel, mtu limited by l2cap

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, 0x10001, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    sdp_register_service(spp_service_buffer);
    printf("SDP service record size: %u\n", de_get_len(spp_service_buffer));
}
/* LISTING_END */


/* @section Bluetooth Logic
 * @text The Bluetooth logic is implemented within the
 * packet handler, see Listing SppServerPacketHandler. In this example,
 * the following events are passed sequentially:
 * - BTSTACK_EVENT_STATE,
 * - HCI_EVENT_PIN_CODE_REQUEST (Standard pairing) or
 * - HCI_EVENT_USER_CONFIRMATION_REQUEST (Secure Simple Pairing),
 * - RFCOMM_EVENT_INCOMING_CONNECTION,
 * - RFCOMM_EVENT_CHANNEL_OPENED,
* - RFCOMM_EVETN_CAN_SEND_NOW, and
 * - RFCOMM_EVENT_CHANNEL_CLOSED
 */

/* @text Upon receiving HCI_EVENT_PIN_CODE_REQUEST event, we need to handle
 * authentication. Here, we use a fixed PIN code "0000".
 *
 * When HCI_EVENT_USER_CONFIRMATION_REQUEST is received, the user will be
 * asked to accept the pairing request. If the IO capability is set to
 * SSP_IO_CAPABILITY_DISPLAY_YES_NO, the request will be automatically accepted.
 *
 * The RFCOMM_EVENT_INCOMING_CONNECTION event indicates an incoming connection.
 * Here, the connection is accepted. More logic is need, if you want to handle connections
 * from multiple clients. The incoming RFCOMM connection event contains the RFCOMM
 * channel number used during the SPP setup phase and the newly assigned RFCOMM
 * channel ID that is used by all BTstack commands and events.
 *
 * If RFCOMM_EVENT_CHANNEL_OPENED event returns status greater then 0,
 * then the channel establishment has failed (rare case, e.g., client crashes).
 * On successful connection, the RFCOMM channel ID and MTU for this
 * channel are made available to the heartbeat counter. After opening the RFCOMM channel,
 * the communication between client and the application
 * takes place. In this example, the timer handler increases the real counter every
 * second.
 *
 * RFCOMM_EVENT_CAN_SEND_NOW indicates that it's possible to send an RFCOMM packet
 * on the rfcomm_cid that is include

 */

/* LISTING_START(SppServerPacketHandler): SPP Server - Heartbeat Counter over RFCOMM */
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);

/* LISTING_PAUSE */
    bd_addr_t event_addr;
    uint8_t rfcomm_channel_nr;
    uint16_t mtu;
    int i;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
/* LISTING_RESUME */
                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // ssp: inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n",
                           little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection(rfcomm_channel_id);
                    break;

                case RFCOMM_EVENT_CHANNEL_OPENED:
                    if (rfcomm_event_channel_opened_get_status(packet)) {
                        printf("RFCOMM channel open failed, status 0x%02x\n",
                               rfcomm_event_channel_opened_get_status(packet));
                    } else {
                        rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n",
                               rfcomm_channel_id, mtu);
                    }
                    break;
                case RFCOMM_EVENT_CAN_SEND_NOW:
                    rfcomm_send_now_event();
                    break;

/* LISTING_PAUSE */
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM channel closed\n");
                    rfcomm_channel_id = 0;
                    rfcomm_user_has_authenticated = false;
                    break;

                default:
                    break;
            }
            break;

        case RFCOMM_DATA_PACKET:
            process_received_message(packet, size);
            break;

        default:
            break;
    }
/* LISTING_RESUME */
}

/* LISTING_END */

int btstack_init() {

    spp_service_setup();

    gap_discoverable_control(1);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_local_name("IMX81 Sensor 00:00:00:00:00:00");

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */

