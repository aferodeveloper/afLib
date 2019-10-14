/**
 * Copyright 2018 AF_Ero, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include "af_lib.h"
#include "af_logger.h"
#include "af_queue.h"
#include "af_utils.h"
#include "af_command.h"
#include "af_module_states.h"

/**
 * Define this to debug your selected transport (ie SPI or UART).
 * This will cause a println each time an interrupt or ready byte is received from the ASR.
 * You will also get states printed whenever a SYNC transaction is performed by the afLib.
 */
//#define DEBUG_TRANSPORT     1

/**
 * These are required to be able to recognize the MCU trying to reboot the ASR by setting the command
 * attribute. We used local defines with the aflib prefix to make sure they are always defined and don't
 * clash with anything the app is using.
 */
#define AFLIB_SYSTEM_COMMAND_ATTR_ID    (65012)
#define AFLIB_SYSTEM_COMMAND_REBOOT     (1)

/**
 * Some afLib features are only supported on newer firmware and newer ASR devices, so check
 * for older firmware so afLib can cope with different features
 */
#define AFLIB_SYSTEM_APPLICATION_VERSION             (2003)
// ASR_STATE extensions were introduced in this version
#define AFLIB_SYSTEM_APPLICATION_VERSION_EXTENSIONS  0x5062 // 20578

/**
 * Prevent the MCU from spamming us with too many setAttribute requests.
 * We do this by waiting a small amount of time in between transactions.
 * This prevents sync retries and allows the module to get it's work done.
 */
#define MIN_TIME_BETWEEN_UPDATES_MILLIS (0)

#define MAX_COMMAND_RESULT_TIME_MILLIS          10000

#define ATTRIBUTE_ID_MCU_START                  0x0001   // 1
#define ATTRIBUTE_ID_MCU_END                    0x03ff   // 1023

#define ATTRIBUTE_ID_DEVICE_TO_MCU_CHANNEL      0x044a   // 1098
#define ATTRIBUTE_ID_MCU_TO_DEVICE_CHANNEL      0x044b   // 1099

#define ATTRIBUTE_ID_DEVICE_MCU_START           0x04b1   // 1201
#define ATTRIBUTE_ID_DEVICE_MCU_END             0x0514   // 1300

// afLib needs to tell the ASR what capabilities it supports via the below attribute when it starts up
#define ATTRIBUTE_ID_DEVICE_MCU_AFLIB_CAPABILITIES      (ATTRIBUTE_ID_DEVICE_MCU_START + 6) // 1207
#define ATTRIBUTE_ID_DEVICE_MCU_DEVICE_PROTOCOL_VERSION (ATTRIBUTE_ID_DEVICE_MCU_START + 7) // 1208
#define ATTRIBUTE_ID_DEVICE_MCU_AFLIB_PROTOCOL_VERSION  (ATTRIBUTE_ID_DEVICE_MCU_START + 8) // 1209

#define ATTRIBUTE_ID_TUNNELED_DEVICE_MCU_START  0x0515   // 1301
#define ATTRIBUTE_ID_TUNNELED_DEVICE_MCU_END    0x0578   // 1400

#define IS_ATTRIBUTE_MCU(uuid)                 ((uuid) >= ATTRIBUTE_ID_MCU_START && (uuid) <= ATTRIBUTE_ID_MCU_END)
#define IS_ATTRIBUTE_DEVICE_MCU(uuid)          ((uuid) >= ATTRIBUTE_ID_DEVICE_MCU_START && (uuid) <= ATTRIBUTE_ID_DEVICE_MCU_END)
#define IS_ATTRIBUTE_TUNNELED_DEVICE_MCU(uuid) ((uuid) >= ATTRIBUTE_ID_TUNNELED_DEVICE_MCU_START && (uuid) <= ATTRIBUTE_ID_TUNNELED_DEVICE_MCU_END)
#define IS_ATTRIBUTE_MCU_CHANNEL(uuid)         ((uuid) == ATTRIBUTE_ID_DEVICE_TO_MCU_CHANNEL || (uuid) == ATTRIBUTE_ID_MCU_TO_DEVICE_CHANNEL)

#define STATE_IDLE                          0
#define STATE_STATUS_SYNC                   1
#define STATE_STATUS_ACK                    3
#define STATE_SEND_BYTES                    4
#define STATE_RECV_BYTES                    5
#define STATE_CMD_COMPLETE                  6
#define STATE_WAITING_FOR_SET_RESPONSE      7

#define AFLIB_MCU_PROCOCOL_VERSION          2

#define MAX_SYNC_RETRIES    10
static long last_sync = 0;
static int sync_retries = 0;
static long last_complete = 0;
static uint64_t s_asr_version = 0;
static uint8_t s_asr_states = 0;

typedef struct {
    uint8_t     message_type;
    uint16_t    attr_id;
    uint8_t     request_id;
    uint16_t    value_len;
    uint8_t     *value;
    uint8_t     status;
    uint8_t     reason;
} request_t;

struct af_lib_t {
    af_transport_t *the_transport;

    volatile int interrupts_pending;
    int state;
    uint16_t bytes_to_send;
    uint16_t bytes_to_recv;
    uint8_t request_id;
    uint16_t outstanding_set_get_attr_id;

    // Application Callbacks.
    attr_set_handler_t attr_set_handler;
    attr_notify_handler_t attr_notify_handler;

    af_lib_event_callback_t event_handler;

    af_command_t *write_cmd;
    uint16_t write_buffer_len;
    uint8_t *write_buffer;

    af_command_t *read_cmd;
    uint16_t read_buffer_len;
    uint8_t *read_buffer;

    uint16_t write_cmd_offset;
    uint16_t read_cmd_offset;

    af_status_command_t tx_status;
    af_status_command_t rx_status;

    request_t request;

    uint8_t *asr_capability;
    uint8_t asr_capability_length;

    bool asr_rebooting;
    long attr_set_request_time;

    long last_command_send_time;

    uint16_t asr_protocol_version;
};

AF_QUEUE_DECLARE(s_request_queue, sizeof(request_t), AF_LIB_REQUEST_QUEUE_SIZE);

/****************************************************************************
 *                              Queue Methods                               *
 ****************************************************************************/

static uint8_t af_queue_preemption_disable(void) {
    return 0;
}

static void af_queue_preemption_enable(uint8_t is_nested) {
}

/**
 * queue_init
 *
 * Create a small queue to prevent flooding the ASR-1 with attribute operations.
 * The initial size is small to allow running on small boards like UNO.
 * Size can be increased on larger boards.
 */
static void queue_init(af_lib_t *af_lib) {
    af_queue_init_system(af_queue_preemption_disable, af_queue_preemption_enable);
    AF_QUEUE_INIT(s_request_queue, sizeof(request_t), AF_LIB_REQUEST_QUEUE_SIZE);
}

/**
 * queue_put
 *
 * Add an item to the end of the queue. Return an error if we're out of space in the queue.
 */
static af_lib_error_t queue_put(af_lib_t *af_lib, uint8_t message_type, uint8_t request_id, uint16_t attribute_id, uint16_t value_len, const uint8_t *value, const uint8_t status, const uint8_t reason) {
    queue_t volatile *p_q = &s_request_queue;

    // We need to make sure we leave at least one spot in our queue to handle the response from a server set
    if (af_lib->state != STATE_WAITING_FOR_SET_RESPONSE && AF_QUEUE_GET_NUM_AVAILABLE(p_q) <= 1) {
        return AF_ERROR_QUEUE_OVERFLOW; // We're basically "full" now
    }

    request_t *p_event = (request_t *)AF_QUEUE_ELEM_ALLOC_FROM_INTERRUPT(p_q);

    if (p_event != NULL) {
        uint16_t orig_attribute_id = attribute_id;
        if (IS_ATTRIBUTE_TUNNELED_DEVICE_MCU(orig_attribute_id)) {
            // For tunneled attributes we only support UPDATE messages so if the message type was a get we have to return an error since that's not allowed
            if (MSG_TYPE_GET == message_type) {
                AF_QUEUE_ELEM_FREE_FROM_INTERRUPT(&s_request_queue, p_event);
                return AF_ERROR_INVALID_PARAM;
            }
            message_type = MSG_TYPE_UPDATE;
            attribute_id = ATTRIBUTE_ID_MCU_TO_DEVICE_CHANNEL;
            value_len += sizeof(attribute_id);
        }
        p_event->message_type = message_type;
        p_event->attr_id = attribute_id;
        p_event->request_id = request_id;
        p_event->value_len = value_len;
        p_event->value = (uint8_t*)malloc(value_len);
        if (IS_ATTRIBUTE_TUNNELED_DEVICE_MCU(orig_attribute_id)) {
            af_utils_write_little_endian_16(orig_attribute_id, p_event->value);
            memcpy(p_event->value + sizeof(attribute_id), value, value_len - sizeof(attribute_id));
        } else {
            memcpy(p_event->value, value, value_len);
        }
        p_event->status = status;
        p_event->reason = reason;

        AF_QUEUE_PUT_FROM_INTERRUPT(p_q, p_event);
        return AF_SUCCESS;
    }

    return AF_ERROR_QUEUE_OVERFLOW;
}

/**
 * queue_get
 *
 * Pull and return the oldest item from the queue. Return an error if the queue is empty.
 */
static int queue_get(af_lib_t *af_lib, uint8_t *message_type, uint8_t *request_id, uint16_t *attribute_id, uint16_t *value_len, uint8_t **value, uint8_t *status, uint8_t *reason) {
    // If the ASR is rebooting then we can't be picking things off our queue as we have to wait for the ASR to come back
    if (af_lib->asr_rebooting) {
        return AF_ERROR_ASR_REBOOTING;
    }

    if (AF_QUEUE_PEEK_FROM_INTERRUPT(&s_request_queue)) {
        request_t *p_event = (request_t *)AF_QUEUE_GET_FROM_INTERRUPT(&s_request_queue);
        *message_type = p_event->message_type;
        *attribute_id = p_event->attr_id;
        *request_id = p_event->request_id;
        *value_len = p_event->value_len;
        *value = (uint8_t*)malloc(*value_len);
        memcpy(*value, p_event->value, *value_len);
        free(p_event->value);
        p_event->value = NULL;
        *status = p_event->status;
        *reason = p_event->reason;

        AF_QUEUE_ELEM_FREE_FROM_INTERRUPT(&s_request_queue, p_event);
        return AF_SUCCESS;
    }

    return AF_ERROR_QUEUE_UNDERFLOW;
}

static void dump_queue_element(void* elem) {
    uint16_t i = 0;
    request_t *p_event = (request_t*)elem;
    af_logger_print_buffer("q_elem: attr id: ");
    af_logger_print_value(p_event->attr_id);
    af_logger_print_buffer(" msg type: ");
    af_logger_print_value(p_event->message_type);
    af_logger_print_buffer(" value len: ");
    af_logger_print_value(p_event->value_len);
    af_logger_print_buffer(" val: ");
    for (i = 0; i < p_event->value_len; i++) {
        af_logger_print_formatted_value(p_event->value[i], AF_LOGGER_HEX);
    }
    af_logger_println_buffer("");
}

/**
 * af_lib_update_ints_pending
 *
 * Interrupt-sAF_E method for updating the interrupt count. This is called to increment and decrement the interrupt count
 * as interrupts are received and handled.
 */
static void af_lib_update_ints_pending(af_lib_t *af_lib, int amount) {
    af_lib->interrupts_pending += amount;
}

/**
 * af_lib_send_command
 *
 * This increments the interrupt count to kick off the state machine in the next call to loop().
 */
static void af_lib_send_command(af_lib_t *af_lib) {
    if (0 == af_lib->interrupts_pending && STATE_IDLE == af_lib->state) {
        af_lib_update_ints_pending(af_lib, 1);
    }
    af_lib->last_command_send_time = af_utils_millis();
}

static int af_lib_set_attribute_complete(af_lib_t *af_lib, uint8_t request_id, const uint16_t attr_id, const uint16_t value_len, const uint8_t *value, uint8_t status, uint8_t reason) {
    return queue_put(af_lib, MSG_TYPE_UPDATE, request_id, attr_id, value_len, value, status, reason);
}

/**
 * af_lib_do_get_attribute
 *
 * The private version of getAttribute. This version actually calls af_lib_send_command() to kick off the state machine and
 * execute the operation.
 */
static int af_lib_do_get_attribute(af_lib_t *af_lib, uint8_t request_id, uint16_t attr_id) {
    if (af_lib->interrupts_pending > 0 || af_lib->write_cmd != NULL) {
        return AF_ERROR_BUSY;
    }

    af_lib->write_cmd = (af_command_t*)malloc(sizeof(af_command_t));
    af_command_initialize_with_attr_id(af_lib->write_cmd, request_id, MSG_TYPE_GET, attr_id);
    if (!af_command_is_valid(af_lib->write_cmd)) {
        af_logger_print_buffer("af_lib_do_get_attribute invalid command:");
        af_command_dump_bytes(af_lib->write_cmd);
        af_command_dump(af_lib->write_cmd);
        af_command_cleanup(af_lib->write_cmd);
        free(af_lib->write_cmd);
        af_lib->write_cmd = NULL;
        return AF_ERROR_INVALID_COMMAND;
    }

    af_lib->outstanding_set_get_attr_id = attr_id;

    // Start the transmission.
    af_lib_send_command(af_lib);

    return AF_SUCCESS;
}

/**
 * af_lib_do_set_attribute
 *
 * The private version of setAttribute. This version actually calls af_lib_send_command() to kick off the state machine and
 * execute the operation.
 */
static int af_lib_do_set_attribute(af_lib_t *af_lib, uint8_t request_id, uint16_t attr_id, uint16_t value_len, uint8_t *value) {
    if (af_lib->interrupts_pending > 0 || af_lib->write_cmd != NULL) {
        return AF_ERROR_BUSY;
    }

    af_lib->write_cmd = (af_command_t*)malloc(sizeof(af_command_t));
    af_command_initialize_with_value(af_lib->write_cmd, request_id, MSG_TYPE_SET, attr_id, value_len, value);
    if (!af_command_is_valid(af_lib->write_cmd)) {
        af_logger_print_buffer("af_lib_do_set_attribute invalid command:");
        af_command_dump_bytes(af_lib->write_cmd);
        af_command_dump(af_lib->write_cmd);
        af_command_cleanup(af_lib->write_cmd);
        free(af_lib->write_cmd);
        af_lib->write_cmd = NULL;
        return AF_ERROR_INVALID_COMMAND;
    }

    /**
    * Recognize when the MCU is trying to reboot the ASR. When this is the case, the ASR will reboot before
    * the SPI transaction completes and the _outstandingSetGetAttrId will be left set. Instead, just don't
    * set it for this case.
    */
    if (attr_id != AFLIB_SYSTEM_COMMAND_ATTR_ID || *value != AFLIB_SYSTEM_COMMAND_REBOOT) {
        af_lib->outstanding_set_get_attr_id = attr_id;
    }

    // Start the transmission.
    af_lib_send_command(af_lib);

    return AF_SUCCESS;
}

/**
 * af_lib_do_update_attribute
 *
 * setAttribute calls on MCU attributes turn into updateAttribute calls. See documentation on the SPI protocol for
 * more information. This method calls af_lib_send_command() to kick off the state machine and execute the operation.
 */
static int af_lib_do_update_attribute(af_lib_t *af_lib, uint8_t request_id, uint16_t attr_id, uint16_t value_len, uint8_t *value, uint8_t status, uint8_t reason) {
    if (af_lib->interrupts_pending > 0 || af_lib->write_cmd != NULL) {
        return AF_ERROR_BUSY;
    }

    af_lib->write_cmd = (af_command_t*)malloc(sizeof(af_command_t));
    af_command_initialize_with_status(af_lib->write_cmd, request_id, MSG_TYPE_UPDATE, attr_id, status, reason, value_len, value, true);
    if (!af_command_is_valid(af_lib->write_cmd)) {
        af_logger_print_buffer("af_lib_do_update_attribute invalid command:");
        af_command_dump_bytes(af_lib->write_cmd);
        af_command_dump(af_lib->write_cmd);
        af_command_cleanup(af_lib->write_cmd);
        free(af_lib->write_cmd);
        af_lib->write_cmd = NULL;
        return AF_ERROR_INVALID_COMMAND;
    }

    // Start the transmission.
    af_lib_send_command(af_lib);

    return AF_SUCCESS;
}

/**
 * af_lib_parse_command
 *
 * A debug method for parsing a string into a command. This is not required for library operation and is only supplied
 * as an example of how to execute attribute operations from a command line interface.
 */
#ifdef ATTRIBUTE_CLI
static int af_lib_parse_command(af_lib_t *af_lib, const char *cmd) {
    if (af_lib->interrupts_pending > 0 || af_lib->write_cmd != NULL) {
        af_logger_print_buffer("Busy: ");
        af_logger_print_value(af_lib->interrupts_pending);
        af_logger_print_buffer(", ");
        af_logger_println_value(af_lib->write_cmd != NULL);
        return AF_ERROR_BUSY;
    }

    int req_id = af_lib->request_id++;
    af_lib->write_cmd = (af_command_t*)malloc(sizeof(af_command_t));
    af_command_create_from_string(af_lib->write_cmd, req_id, cmd);
    if (!af_command_is_valid(af_lib->write_cmd)) {
        af_logger_print_buffer("BAD: ");
        af_logger_println_value(cmd);
        af_command_dump_bytes(af_lib->write_cmd);
        af_command_dump(af_lib->write_cmd);
        af_command_cleanup(af_lib->write_cmd);
        free(af_lib->write_cmd);
        af_lib->write_cmd = NULL;
        return AF_ERROR_INVALID_COMMAND;
    }

    // Start the transmission.
    af_lib_send_command(af_lib);

    return AF_SUCCESS;
}
#endif

/**
 * print_state
 *
 * Print the current state of the afLib state machine.
 */
static void print_state(int state) {
#if (defined(DEBUG_TRANSPORT) && DEBUG_TRANSPORT > 0)
    switch (state) {
        case STATE_IDLE:
            af_logger_println_buffer("STATE_IDLE");
            break;
        case STATE_STATUS_SYNC:
            af_logger_println_buffer("STATE_STATUS_SYNC");
            break;
        case STATE_STATUS_ACK:
            af_logger_println_buffer("STATE_STATUS_ACK");
            break;
        case STATE_SEND_BYTES:
            af_logger_println_buffer("STATE_SEND_BYTES");
            break;
        case STATE_RECV_BYTES:
            af_logger_println_buffer("STATE_RECV_BYTES");
            break;
        case STATE_CMD_COMPLETE:
            af_logger_println_buffer("STATE_CMD_COMPLETE");
            break;
        default:
            af_logger_println_buffer("Unknown State!");
            break;
    }
#endif
}

/**
 * in_sync
 *
 * Check to make sure the MCU and the ASR-1 aren't trying to send data at the same time.
 * Return true only if there is no collision.
 */
static bool in_sync(af_status_command_t *tx, af_status_command_t *rx) {
    return (af_status_command_get_bytes_to_send(tx) == 0 && af_status_command_get_bytes_to_recv(rx) == 0) ||
           (af_status_command_get_bytes_to_send(tx) > 0 && af_status_command_get_bytes_to_recv(rx) == 0) ||
           (af_status_command_get_bytes_to_send(tx) == 0 && af_status_command_get_bytes_to_recv(rx) > 0);
}

/**
 * af_lib_on_state_idle
 *
 * If there is a command to be written, update the bytes to send. Otherwise we're sending a zero-sync message.
 * Either way advance the state to send a sync message.
 */
static void af_lib_on_state_idle(af_lib_t *af_lib) {
    if (af_lib->write_cmd != NULL) {
        // Include 2 bytes for length
        af_lib->bytes_to_send = af_command_get_size(af_lib->write_cmd) + 2;
    } else {
        af_lib->bytes_to_send = 0;
    }
    af_lib->state = STATE_STATUS_SYNC;
    print_state(af_lib->state);
}

/**
 * af_lib_on_state_sync
 *
 * Write a sync message over SPI to let the ASR-1 know that we want to send some data.
 * Check for a "collision" which occurs if the ASR-1 is trying to send us data at the same time.
 */
static void af_lib_on_state_sync(af_lib_t *af_lib) {
    int result;

    af_status_command_set_ack(&af_lib->tx_status, false);
    af_status_command_set_bytes_to_send(&af_lib->tx_status, af_lib->bytes_to_send);
    af_status_command_set_bytes_to_recv(&af_lib->tx_status, 0);

    result = af_transport_exchange_status(af_lib->the_transport, &af_lib->tx_status, &af_lib->rx_status);

    if (AF_SUCCESS == result && af_status_command_is_valid(&af_lib->rx_status) && in_sync(&af_lib->tx_status, &af_lib->rx_status)) {
        sync_retries = 0;   // Flag that sync completed.
        af_lib->state = STATE_STATUS_ACK;
        if (af_status_command_get_bytes_to_send(&af_lib->tx_status) == 0 && af_status_command_get_bytes_to_recv(&af_lib->rx_status) > 0) {
            af_lib->bytes_to_recv = af_status_command_get_bytes_to_recv(&af_lib->rx_status);
        }
    } else {
        // Try resending the preamble
        af_lib->state = STATE_STATUS_SYNC;
        last_sync = af_utils_millis();
        sync_retries++;
#if (defined(DEBUG_TRANSPORT) && DEBUG_TRANSPORT > 0)
        af_logger_println_buffer("tx_status");
        af_status_command_dump(&af_lib->tx_status);
        af_logger_println_buffer("rx_status");
        af_status_command_dump(&af_lib->rx_status);
#endif
    }
    print_state(af_lib->state);
}

/**
 * af_lib_on_state_ack
 *
 * Acknowledge the previous sync message and advance the state.
 * If there are bytes to send, advance to send bytes state.
 * If there are bytes to receive, advance to receive bytes state.
 * Otherwise it was a zero-sync so advance to command complete.
 */
static void af_lib_on_state_ack(af_lib_t *af_lib) {
    int result;

    af_status_command_set_ack(&af_lib->tx_status, true);
    af_status_command_set_bytes_to_recv(&af_lib->tx_status, af_status_command_get_bytes_to_recv(&af_lib->rx_status));
    af_lib->bytes_to_recv = af_status_command_get_bytes_to_recv(&af_lib->rx_status);
    result = af_transport_write_status(af_lib->the_transport, &af_lib->tx_status);
    if (result != AF_SUCCESS) {
        af_lib->state = STATE_STATUS_SYNC;
        print_state(af_lib->state);
        return;
    }
    if (af_lib->bytes_to_send > 0) {
        af_lib->write_buffer_len = af_command_get_size(af_lib->write_cmd);
        af_lib->write_buffer = (uint8_t*)malloc(af_lib->bytes_to_send);
        memcpy(af_lib->write_buffer, &af_lib->write_buffer_len, 2);
        af_command_get_bytes(af_lib->write_cmd, &af_lib->write_buffer[2]);
        af_lib->state = STATE_SEND_BYTES;
    } else if (af_lib->bytes_to_recv > 0) {
        af_lib->state = STATE_RECV_BYTES;
    } else {
        af_lib->state = STATE_CMD_COMPLETE;
    }
    print_state(af_lib->state);
}

/**
 * af_lib_on_state_send_bytes
 *
 * Send the required number of bytes to the ASR-1 and then advance to command complete.
 */
static void af_lib_on_state_send_bytes(af_lib_t *af_lib) {
    //af_logger_print_buffer("send bytes: "); af_logger_println_value(af_lib->bytes_to_send);
    af_transport_send_bytes_offset(af_lib->the_transport, af_lib->write_buffer, &af_lib->bytes_to_send, &af_lib->write_cmd_offset);

    if (0 == af_lib->bytes_to_send) {
        af_lib->write_buffer_len = 0;
        free(af_lib->write_buffer);
        af_lib->write_buffer = NULL;
        af_lib->state = STATE_CMD_COMPLETE;
        print_state(af_lib->state);
    }
}

/**
 * af_lib_on_state_recv_bytes
 *
 * Receive the required number of bytes from the ASR-1 and then advance to command complete.
 */
static void af_lib_on_state_recv_bytes(af_lib_t *af_lib) {
    int result = af_transport_recv_bytes_offset(af_lib->the_transport, &af_lib->read_buffer, &af_lib->read_buffer_len, &af_lib->bytes_to_recv, &af_lib->read_cmd_offset);
    if (result != AF_SUCCESS) {
        af_lib->state = STATE_IDLE;
        print_state(af_lib->state);
        free(af_lib->read_buffer);
        af_lib->read_buffer = NULL;
        return;
    }
    if (0 == af_lib->bytes_to_recv) {
        af_lib->state = STATE_CMD_COMPLETE;
        print_state(af_lib->state);
        af_lib->read_cmd = (af_command_t*)malloc(sizeof(af_command_t));
        af_command_initialize_from_buffer(af_lib->read_cmd, af_lib->read_buffer_len, &af_lib->read_buffer[2], af_lib->asr_protocol_version);
        //_readCmd->dumpBytes();
        free(af_lib->read_buffer);
        af_lib->read_buffer = NULL;
    }
}

static af_lib_error_t af_lib_convert_state_to_error(uint8_t state) {
    af_lib_error_t error = AF_SUCCESS;

    switch (state) {
        case UPDATE_STATE_UPDATED:
            error = AF_SUCCESS;
            break;
        case UPDATE_STATE_UNKNOWN_UUID:
            error = AF_ERROR_NO_SUCH_ATTRIBUTE;
            break;
        case UPDATE_STATE_LENGTH_EXCEEDED:
            error = AF_ERROR_INVALID_DATA;
            break;
        case UPDATE_STATE_FORBIDDEN:
            error = AF_ERROR_FORBIDDEN;
            break;
        default:
            error = AF_ERROR_UNKNOWN;
            break;
    }

    return error;
}

static void af_lib_handle_attr_notify(af_lib_t *af_lib, af_command_t *command) {
    uint16_t attribute_id = af_command_get_attr_id(command);
    uint16_t value_len = af_command_get_value_len(command);
    uint8_t *value = (uint8_t*)af_command_get_value_pointer(command);

    if (ATTRIBUTE_ID_DEVICE_TO_MCU_CHANNEL == attribute_id) {
        attribute_id = af_utils_read_little_endian_16(value);
        value_len -= sizeof(attribute_id);
        value += sizeof(attribute_id);
    }

    if (af_lib->event_handler != NULL) {
        af_lib_event_type_t event = AF_LIB_EVENT_UNKNOWN;
        af_lib_error_t error = af_lib_convert_state_to_error(af_command_get_state(command));
        bool old_default_msg = false;

        if (af_command_get_mcu_started(command) && IS_ATTRIBUTE_MCU(attribute_id)) {
            event = AF_LIB_EVENT_MCU_SET_REQ_SENT;
        } else if (af_command_get_reason(command) == UPDATE_REASON_MCU_SET) {
            event = AF_LIB_EVENT_ASR_SET_RESPONSE;
        } else if (af_command_get_reason(command) == UPDATE_REASON_GET_RESPONSE ) {
            event = AF_LIB_EVENT_GET_RESPONSE;
        } else if (af_command_get_reason(command) == UPDATE_REASON_REBOOTED && IS_ATTRIBUTE_MCU(attribute_id)) {
            // Old ASRs that don't support the MCU v2 protocol will still send this message to indicate a default value.  Starting in aflib4 we only send this message
            // for attributes with actual non-empty default values - so we need to check that here.  Also, the contract with aflib4 is that the ASR will ask (via the get msg)
            // for any attribute that it wants a value for - clearly the old ASR won't be doing that so we need to create one of those events here as well.
            if (af_lib->asr_protocol_version < 2) {
                if (value_len != 0) {
                    event = AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION;
                    old_default_msg = true;
                } else {
                    event = AF_LIB_EVENT_MCU_GET_REQUEST;
                    error = AF_SUCCESS;
                }
            } else {
                af_logger_println_buffer("Unexpected msg from ASR supporting MCU protocol v2!!!");
            }
        } else {
            event = AF_LIB_EVENT_ASR_NOTIFICATION;
        }

        af_lib->event_handler(event, error, attribute_id, value_len, value);

        // After we've sent off this message if it's an old default msg then we need to also ask for the current value
        if (old_default_msg) {
            af_lib->event_handler(AF_LIB_EVENT_MCU_GET_REQUEST, AF_SUCCESS, attribute_id, 0, NULL);
        }
    } else {
        af_lib->attr_notify_handler(af_command_get_req_id(command), attribute_id, value_len, value);
    }
}

static void af_lib_asr_initialization_complete(af_lib_t *af_lib) {
    bool asr_state_extensions = AFLIB_SYSTEM_APPLICATION_VERSION_EXTENSIONS < s_asr_version;
    uint8_t desired_state = 1 << (asr_state_extensions ? AF_MODULE_STATE_INITIALIZED : AF_MODULE_STATE_LINKED);

    if (s_asr_states & desired_state) {
        af_logger_println_buffer("ASR finished rebooting");
        af_lib->asr_rebooting = false;

        // When we start up we need to tell the ASR our capabilities
        uint8_t our_capability = 0;
        af_lib_set_attribute_bytes(af_lib, ATTRIBUTE_ID_DEVICE_MCU_AFLIB_CAPABILITIES, sizeof(our_capability), &our_capability, AF_LIB_SET_REASON_LOCAL_CHANGE);

        // When we start up we need to get the ASR capabilities and cache them internally
        af_lib_get_attribute(af_lib, AF_ATTRIBUTE_ID_ASR_CAPABILITIES);

        // Clear the variables after we've gotten what we wanted
        s_asr_version = s_asr_states = 0;
    }
}

/**
 * af_lib_on_state_cmd_complete
 *
 * Call the appropriate callback to report the result of the command.
 * Clear the command object and go back to waiting for the next interrupt or command.
 */
static void af_lib_on_state_cmd_complete(af_lib_t *af_lib) {
    int result;
    uint8_t state;
    uint8_t reason;
    uint8_t command;

    af_lib->state = STATE_IDLE;
    print_state(af_lib->state);
    if (af_lib->read_cmd != NULL) {
        uint8_t *val = (uint8_t*)malloc(af_command_get_value_len(af_lib->read_cmd));
        af_command_get_value(af_lib->read_cmd, val);

        command = af_command_get_command(af_lib->read_cmd);

        switch (command) {
            case MSG_TYPE_SET:
                if (af_lib->event_handler != NULL) {
                    af_lib->state = STATE_WAITING_FOR_SET_RESPONSE;
                    af_lib->attr_set_request_time = af_utils_millis();
                    af_lib->event_handler(AF_LIB_EVENT_MCU_SET_REQUEST, AF_SUCCESS, af_command_get_attr_id(af_lib->read_cmd), af_command_get_value_len(af_lib->read_cmd), val);
                } else {
                    if (af_lib->attr_set_handler(af_command_get_req_id(af_lib->read_cmd), af_command_get_attr_id(af_lib->read_cmd), af_command_get_value_len(af_lib->read_cmd), val)) {
                        state = UPDATE_STATE_UPDATED;
                        reason = UPDATE_REASON_SERVICE_SET;
                    } else {
                        state = UPDATE_STATE_FAILED;
                        reason = UPDATE_REASON_INTERNAL_SET_REJECTED;
                    }
                    af_lib->state = STATE_WAITING_FOR_SET_RESPONSE;
                    result = af_lib_set_attribute_complete(af_lib, af_command_get_req_id(af_lib->read_cmd), af_command_get_attr_id(af_lib->read_cmd), af_command_get_value_len(af_lib->read_cmd), val, state, reason);
                    if (result != AF_SUCCESS) {
                        af_logger_print_buffer("Can't reply to SET in on_state_cmd_complete! This is FATAL! rc=");
                        af_logger_println_value(result);
                    }
                    af_lib->state = STATE_IDLE;
                }
                break;

            case MSG_TYPE_UPDATE:
                // If the attr update is a "fake" update, don't send it to the MCU
                if (af_command_get_reason(af_lib->read_cmd) != UPDATE_REASON_FAKE_UPDATE) {
                    // If this is the AF_ATTRIBUTE_ID_ASR_CAPABILITIES then cache the value internally so we can use it later on
                    uint16_t attr_id = af_command_get_attr_id(af_lib->read_cmd);
                    if (AF_ATTRIBUTE_ID_ASR_CAPABILITIES == attr_id && NULL == af_lib->asr_capability) {
                        af_lib->asr_capability_length = af_command_get_value_len(af_lib->read_cmd);
                        af_lib->asr_capability = (uint8_t*)malloc(af_lib->asr_capability_length);
                        memcpy(af_lib->asr_capability, val, af_lib->asr_capability_length);
                    }
                    if (ATTRIBUTE_ID_DEVICE_MCU_DEVICE_PROTOCOL_VERSION == attr_id) {
                        af_lib->asr_protocol_version = af_utils_read_little_endian_16(val);
                        // Now we need to send up our protocol version
                        uint16_t our_protocol_version = AFLIB_MCU_PROCOCOL_VERSION;
                        af_logger_print_buffer("ASR protocol version: ");
                        af_logger_println_value(af_lib->asr_protocol_version);
                        af_lib->asr_rebooting = false; // This will actually let the message out, we'll revert back to the the "true" value once we get the update message from the ASR
                        af_lib_set_attribute_16(af_lib, ATTRIBUTE_ID_DEVICE_MCU_AFLIB_PROTOCOL_VERSION, our_protocol_version, AF_LIB_SET_REASON_LOCAL_CHANGE);
                    }
                    if (ATTRIBUTE_ID_DEVICE_MCU_AFLIB_PROTOCOL_VERSION == attr_id) {
                        af_lib->asr_rebooting = true; // Now we're back to our normal selves and have to wait for the ASR state
                    }

                    if (attr_id == af_lib->outstanding_set_get_attr_id) {
                        af_lib->outstanding_set_get_attr_id = 0;
                    }

                    if (AFLIB_SYSTEM_APPLICATION_VERSION == attr_id) {
                        s_asr_version = af_utils_read_little_endian_64(af_command_get_value_pointer(af_lib->read_cmd));
                        if (s_asr_states != 0 && s_asr_version != 0) {
                            af_lib_asr_initialization_complete(af_lib);
                        }
                    }

                    if (AF_SYSTEM_ASR_STATE_ATTR_ID == attr_id) {
                        uint8_t *value = (uint8_t*)af_command_get_value_pointer(af_lib->read_cmd);
                        if (value != NULL) {
                            s_asr_states |= (1 << value[0]);
                            if (s_asr_states != 0 && s_asr_version != 0) {
                                af_lib_asr_initialization_complete(af_lib);
                            }
                        }
                    }

                    bool hide_from_mcu = false;
                    switch (attr_id) {
                        // If this is the internal ATTRIBUTE_ID_DEVICE_MCU_AFLIB_CAPABILITIES or the protocol versions then don't tell the MCU since this is used for internal book keeping between afLib and the ASR
                        case ATTRIBUTE_ID_DEVICE_MCU_AFLIB_CAPABILITIES:
                        case ATTRIBUTE_ID_DEVICE_MCU_AFLIB_PROTOCOL_VERSION:
                        case ATTRIBUTE_ID_DEVICE_MCU_DEVICE_PROTOCOL_VERSION:
                            hide_from_mcu = true;
                            break;
                        default:
                            hide_from_mcu = false;
                            break;
                    }

                    if (!hide_from_mcu) {
                        static bool inNotifyHandler;
                        if (!inNotifyHandler) {
                            inNotifyHandler = true;
                            af_lib_handle_attr_notify(af_lib, af_lib->read_cmd);
                            inNotifyHandler = false;
                        }
                    }
                    last_complete = af_utils_millis();
                }
                break;

            case MSG_TYPE_UPDATE_REJECTED:
                if (af_lib->event_handler != NULL) {
                    af_lib->event_handler(AF_LIB_EVENT_MCU_SET_REQ_REJECTION, af_lib_convert_state_to_error(af_command_get_state(af_lib->read_cmd)), af_command_get_attr_id(af_lib->read_cmd), af_command_get_value_len(af_lib->read_cmd), val);
                }
                break;

            case MSG_TYPE_GET:
                if (af_lib->event_handler != NULL) {
                    af_lib->event_handler(AF_LIB_EVENT_MCU_GET_REQUEST, AF_SUCCESS, af_command_get_attr_id(af_lib->read_cmd), 0, NULL);
                }
                break;

            case MSG_TYPE_SET_DEFAULT:
                if (af_lib->event_handler != NULL) {
                    af_lib->event_handler(AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION, AF_SUCCESS, af_command_get_attr_id(af_lib->read_cmd), af_command_get_value_len(af_lib->read_cmd), val);
                }
                break;

            default:
                if (af_lib->asr_protocol_version < 2) {
                    // We changed some of the values for the msg types in 2, so let's see if it's one of the old ones...
                    if (MSG_TYPE_UPDATE_REJECTED_V1 == command) {
                        if (af_lib->event_handler != NULL) {
                            af_lib->event_handler(AF_LIB_EVENT_MCU_SET_REQ_REJECTION, af_lib_convert_state_to_error(af_command_get_state(af_lib->read_cmd)), af_command_get_attr_id(af_lib->read_cmd), af_command_get_value_len(af_lib->read_cmd), val);
                        }
                        break;
                    }
                }
                af_logger_print_buffer("Unhandled msg type: ");
                af_logger_println_value(command);
                break;
        }
        free(val);
        if (af_lib->state != STATE_WAITING_FOR_SET_RESPONSE) {
            if (af_lib->read_cmd != NULL) {
                af_command_cleanup(af_lib->read_cmd);
                free(af_lib->read_cmd);
                af_lib->read_cmd_offset = 0;
                af_lib->read_cmd = NULL;
            }
        }
    }

    if (af_lib->write_cmd != NULL) {
        // If we just wrote the command attribute with the value of reboot then put our foot down and don't allow any other attributes to be get/set until we get the ASR state update from the ASR indicating that it has rebooted
        if (af_command_get_command(af_lib->write_cmd) == MSG_TYPE_SET && af_command_get_attr_id(af_lib->write_cmd) == AFLIB_SYSTEM_COMMAND_ATTR_ID) {
            const uint8_t* data = af_command_get_value_pointer(af_lib->write_cmd);
            if (data != NULL && AFLIB_SYSTEM_COMMAND_REBOOT == *data) {
                af_logger_println_buffer("ASR rebooting...");
                af_lib->asr_rebooting = true;
            }
        }

        // Fake a callback here for MCU attributes as we don't get one from the module - but only if the it was started by the MCU calling one of the af_lib_set_attribute* calls
        if (af_command_get_command(af_lib->write_cmd) == MSG_TYPE_UPDATE && IS_ATTRIBUTE_MCU(af_command_get_attr_id(af_lib->write_cmd)) && af_command_get_mcu_started(af_lib->write_cmd)) {
            af_lib_handle_attr_notify(af_lib, af_lib->write_cmd);
            last_complete = af_utils_millis();
        }
        af_command_cleanup(af_lib->write_cmd);
        free(af_lib->write_cmd);
        af_lib->write_cmd = NULL;
        af_lib->write_cmd_offset = 0;
    }
}

/**
 * af_lib_on_state_waiting_for_set_response
 *
 * See if we've detected a possible MCU logic bug wherein they've yet to call the af_lib_send_set_response() to a server set request in the allotted time.  If so tell them and unclear the blockage so we can proceed.
 */
static void af_lib_on_state_waiting_for_set_response(af_lib_t *af_lib) {
    if (af_utils_millis() - af_lib->attr_set_request_time > AF_LIB_SET_RESPONSE_TIMEOUT_SECONDS*1000) {
        af_logger_print_buffer("Response timeout for attribute ");
        af_logger_print_value(af_command_get_attr_id(af_lib->read_cmd));
        af_logger_print_buffer(", timeout ");
        af_logger_print_value(AF_LIB_SET_RESPONSE_TIMEOUT_SECONDS);
        af_logger_println_buffer(" seconds");

        // We've detected a possible error in the MCU code and to keep us from doing nothing forever we'll respond on the MCU's behalf and also tell them that this situation occurred
        af_lib->event_handler(AF_LIB_EVENT_MCU_SET_REQUEST_RESPONSE_TIMEOUT, AF_ERROR_TIMEOUT, af_command_get_attr_id(af_lib->read_cmd), af_command_get_value_len(af_lib->read_cmd), af_command_get_value_pointer(af_lib->read_cmd));

        // The MCU code might have responded to our above "kick" and called the appropriate function - so we gotta double check to make sure we still need to
        if (af_lib->read_cmd != NULL) {
            af_lib_send_set_response(af_lib, af_command_get_attr_id(af_lib->read_cmd), false, af_command_get_value_len(af_lib->read_cmd), af_command_get_value_pointer(af_lib->read_cmd));
        }
    }
}


/**
 * af_lib_run_state_machine
 *
 * The state machine for afLib. This state machine is responsible for implementing the KSP SPI protocol and executing
 * attribute operations.
 * This method is run:
 *      1. In response to receiving an interrupt from the ASR-1.
 *      2. When an attribute operation is pulled out of the queue and executed.
 */
static void af_lib_run_state_machine(af_lib_t *af_lib) {
    if (af_lib->interrupts_pending > 0) {
#if (defined(DEBUG_TRANSPORT) && DEBUG_TRANSPORT > 0)
        af_logger_print_buffer("interrupts_pending: "); af_logger_print_value(af_lib->interrupts_pending); af_logger_print_buffer(" state: "); af_logger_println_value(af_lib->state);
#endif

        switch (af_lib->state) {
            case STATE_IDLE:
                af_lib_on_state_idle(af_lib);
                return;

            case STATE_STATUS_SYNC:
                af_lib_on_state_sync(af_lib);
                break;

            case STATE_STATUS_ACK:
                af_lib_on_state_ack(af_lib);
                break;

            case STATE_SEND_BYTES:
                af_lib_on_state_send_bytes(af_lib);
                break;

            case STATE_RECV_BYTES:
                af_lib_on_state_recv_bytes(af_lib);
                break;

            case STATE_CMD_COMPLETE:
                af_lib_on_state_cmd_complete(af_lib);
                break;

            case STATE_WAITING_FOR_SET_RESPONSE:
                af_lib_on_state_waiting_for_set_response(af_lib);
                break;
        }

        af_lib_update_ints_pending(af_lib, -1);
    } else {
        if (sync_retries > 0 && sync_retries < MAX_SYNC_RETRIES && af_utils_millis() - last_sync > 1000) {
            af_logger_println_buffer("Sync Retry");
            af_lib_update_ints_pending(af_lib, 1);
        } else if (sync_retries >= MAX_SYNC_RETRIES) {
            af_logger_println_buffer("No response from ASR - does profile have MCU enabled?");
            sync_retries = 0;
            af_lib->state = STATE_IDLE;
            if (af_lib->event_handler != NULL) {
                af_lib->event_handler(AF_LIB_EVENT_COMMUNICATION_BREAKDOWN, AF_ERROR_UNKNOWN, 0, 0, NULL);

            }
        }
    }
}

static uint8_t af_lib_set_reason_converter(af_lib_t *af_lib, af_lib_set_reason_t reason) {
    switch (reason) {
        case AF_LIB_SET_REASON_GET_RESPONSE:
            return af_lib->asr_protocol_version < 2 ? UPDATE_REASON_LOCAL_OR_MCU_UPDATE : UPDATE_REASON_GET_RESPONSE;
        case AF_LIB_SET_REASON_LOCAL_CHANGE:
            return UPDATE_REASON_LOCAL_OR_MCU_UPDATE;
        default:
            return UPDATE_REASON_LOCAL_OR_MCU_UPDATE;
    }
}

/****************************************************************************
 *                              Public Methods                              *
 ****************************************************************************/

af_lib_t* af_lib_create(attr_set_handler_t attr_set, attr_notify_handler_t attr_notify, af_transport_t *the_transport) {
    af_lib_t *af_lib = (af_lib_t*)malloc(sizeof(af_lib_t));
    memset(af_lib, 0, sizeof(af_lib_t));

    queue_init(af_lib);
    af_lib->the_transport = the_transport;
    af_lib->request.value = NULL;

    af_lib->interrupts_pending = 0;
    af_lib->state = STATE_IDLE;

    af_status_command_initialize(&af_lib->tx_status);
    af_status_command_initialize(&af_lib->rx_status);

    af_lib->attr_set_handler = attr_set;
    af_lib->attr_notify_handler = attr_notify;
    af_lib->asr_capability = NULL;
    af_lib->asr_capability_length = 0;
    af_lib->asr_rebooting = true;
    af_lib->asr_protocol_version = 1; // Till we know otherwise...

    return af_lib;
}

void af_lib_destroy(af_lib_t* af_lib) {
    af_status_command_cleanup(&af_lib->tx_status);
    af_status_command_cleanup(&af_lib->rx_status);
    free(af_lib->asr_capability);
    free(af_lib);
}

/**
 * af_lib_loop
 *
 * This is how the afLib gets time to run its state machine. This method should be called periodically.
 * This function pulls pending attribute operations from the queue. It takes approximately 4 calls to loop() to
 * complete one attribute operation.
 */
void af_lib_loop(af_lib_t *af_lib) {
    // For UART, we need to look for a magic character on the line as our interrupt.
    // We call this method to handle that. For other interfaces, the interrupt pin is used and this method does nothing.
    af_transport_check_for_interrupt(af_lib->the_transport, &af_lib->interrupts_pending, af_lib_is_idle(af_lib));

    if (af_lib_is_idle(af_lib) && (queue_get(af_lib, &af_lib->request.message_type, &af_lib->request.request_id, &af_lib->request.attr_id, &af_lib->request.value_len,
                              &af_lib->request.value, &af_lib->request.status, &af_lib->request.reason) == AF_SUCCESS)) {
        switch (af_lib->request.message_type) {
            case MSG_TYPE_GET:
                af_lib_do_get_attribute(af_lib, af_lib->request.request_id, af_lib->request.attr_id);
                break;

            case MSG_TYPE_SET:
                af_lib_do_set_attribute(af_lib, af_lib->request.request_id, af_lib->request.attr_id, af_lib->request.value_len, af_lib->request.value);
                break;

            case MSG_TYPE_UPDATE:
                af_lib_do_update_attribute(af_lib, af_lib->request.request_id, af_lib->request.attr_id, af_lib->request.value_len, af_lib->request.value, af_lib->request.status, af_lib->request.reason);
                break;

            default:
                af_logger_println_buffer("loop: INVALID request type!");
        }
    }

    if (af_lib->request.value != NULL) {
        free(af_lib->request.value);
        af_lib->request.value = NULL;
    }
    af_lib_run_state_machine(af_lib);
}

/**
 * af_lib_get_attribute
 *
 * The public getAttribute method. This method queues the operation and returns immediately. Applications must call
 * loop() for the operation to complete.
 */
af_lib_error_t af_lib_get_attribute(af_lib_t *af_lib, const uint16_t attr_id) {
    uint8_t dummy; // This value isn't actually used.
    af_lib->request_id++;
    return queue_put(af_lib, MSG_TYPE_GET, af_lib->request_id, attr_id, 0, &dummy, 0, 0);
}

/**
 * The many moods of setAttribute
 *
 * These are the public versions of the setAttribute method.
 * These methods queue the operation and return immediately. Applications must call loop() for the operation to complete.
 */
af_lib_error_t af_lib_set_attribute_bool(af_lib_t *af_lib, const uint16_t attr_id, const bool value, af_lib_set_reason_t reason) {
    uint8_t val = value ? 1 : 0;
    af_lib->request_id++;
    return queue_put(af_lib, IS_ATTRIBUTE_MCU(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, sizeof(val), (uint8_t *)&val, UPDATE_STATE_UPDATED, af_lib_set_reason_converter(af_lib, reason));
}

af_lib_error_t af_lib_set_attribute_8(af_lib_t *af_lib, const uint16_t attr_id, const int8_t value, af_lib_set_reason_t reason) {
    af_lib->request_id++;
    return queue_put(af_lib, IS_ATTRIBUTE_MCU(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, sizeof(value), (uint8_t *)&value, UPDATE_STATE_UPDATED, af_lib_set_reason_converter(af_lib, reason));
}

af_lib_error_t af_lib_set_attribute_16(af_lib_t *af_lib, const uint16_t attr_id, const int16_t value, af_lib_set_reason_t reason) {
    uint8_t temp[sizeof(value)];
    af_lib->request_id++;
    af_utils_write_little_endian_16(value, temp);
    return queue_put(af_lib, IS_ATTRIBUTE_MCU(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, sizeof(temp), temp, UPDATE_STATE_UPDATED, af_lib_set_reason_converter(af_lib, reason));
}

af_lib_error_t af_lib_set_attribute_32(af_lib_t *af_lib, const uint16_t attr_id, const int32_t value, af_lib_set_reason_t reason) {
    uint8_t temp[sizeof(value)];
    af_lib->request_id++;
    af_utils_write_little_endian_32(value, temp);
    return queue_put(af_lib, IS_ATTRIBUTE_MCU(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, sizeof(temp), temp, UPDATE_STATE_UPDATED, af_lib_set_reason_converter(af_lib, reason));
}

af_lib_error_t af_lib_set_attribute_64(af_lib_t *af_lib, const uint16_t attr_id, const int64_t value, af_lib_set_reason_t reason) {
    uint8_t temp[sizeof(value)];
    af_lib->request_id++;
    af_utils_write_little_endian_64(value, temp);
    return queue_put(af_lib, IS_ATTRIBUTE_MCU(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, sizeof(temp), temp, UPDATE_STATE_UPDATED, af_lib_set_reason_converter(af_lib, reason));
}

af_lib_error_t af_lib_set_attribute_str(af_lib_t *af_lib, const uint16_t attr_id, const uint16_t value_len, const char *value, af_lib_set_reason_t reason) {
    af_lib->request_id++;
    return queue_put(af_lib, IS_ATTRIBUTE_MCU(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, value_len, (const uint8_t *) value, UPDATE_STATE_UPDATED, af_lib_set_reason_converter(af_lib, reason));
}

af_lib_error_t af_lib_set_attribute_bytes(af_lib_t *af_lib, const uint16_t attr_id, const uint16_t value_len, const uint8_t *value, af_lib_set_reason_t reason) {
    af_lib->request_id++;
    return queue_put(af_lib, IS_ATTRIBUTE_MCU(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, value_len, value, UPDATE_STATE_UPDATED, af_lib_set_reason_converter(af_lib, reason));
}

/**
 * af_lib_is_idle
 *
 * Provide a way to know if we're idle. Returns true if there are no attribute operations in progress.
 */
bool af_lib_is_idle(af_lib_t *af_lib) {
    if (last_complete != 0 && (af_utils_millis() - last_complete) < MIN_TIME_BETWEEN_UPDATES_MILLIS) {
        return false;
    }
    // See if we've waited long enough for the last command to complete
    if (af_lib->outstanding_set_get_attr_id != 0 && af_utils_millis() - af_lib->last_command_send_time > MAX_COMMAND_RESULT_TIME_MILLIS) {
        af_logger_print_buffer("af_lib(): last attr command ");
        af_logger_print_value(af_lib->outstanding_set_get_attr_id);
        af_logger_println_buffer(" took too long to complete, moving on...");
        af_lib->outstanding_set_get_attr_id = 0;
    }

    last_complete = 0;
    return 0 == af_lib->interrupts_pending && STATE_IDLE == af_lib->state && 0 == af_lib->outstanding_set_get_attr_id;
}

void af_lib_sync(af_lib_t *af_lib) {
    while (!af_lib_is_idle(af_lib)) {
        af_lib_loop(af_lib);
    }
}

void af_lib_mcu_isr(af_lib_t *af_lib) {
#if (defined(DEBUG_TRANSPORT) && DEBUG_TRANSPORT > 0)
    af_logger_println_buffer("mcuISR");
#endif
    af_lib_update_ints_pending(af_lib, 1);
}

af_lib_error_t af_lib_asr_has_capability(af_lib_t *af_lib, uint32_t af_asr_capability) {
    uint8_t byte_index = af_asr_capability / 8;
    uint8_t byte_bit_index = af_asr_capability % 8;
    bool supported;

    if (NULL == af_lib) {
        return AF_ERROR_NOT_CREATED;
    }

    if (NULL == af_lib->asr_capability) {
        return AF_ERROR_BUSY;
    }

    if (byte_index > af_lib->asr_capability_length) {
        return AF_ERROR_NOT_SUPPORTED;
    }

    supported = af_lib->asr_capability[byte_index] & (1 << (7 - byte_bit_index));
    return supported ? AF_SUCCESS : AF_ERROR_NOT_SUPPORTED;
}

af_lib_t *af_lib_create_with_unified_callback(af_lib_event_callback_t event_cb, af_transport_t *transport) {
    af_lib_t *af_lib = (af_lib_t*)malloc(sizeof(af_lib_t));
    memset(af_lib, 0, sizeof(af_lib_t));

    queue_init(af_lib);
    af_lib->the_transport = transport;
    af_lib->request.value = NULL;

    af_lib->interrupts_pending = 0;
    af_lib->state = STATE_IDLE;

    af_status_command_initialize(&af_lib->tx_status);
    af_status_command_initialize(&af_lib->rx_status);

    af_lib->event_handler = event_cb;
    af_lib->asr_capability = NULL;
    af_lib->asr_capability_length = 0;
    af_lib->asr_rebooting = true;
    af_lib->asr_protocol_version = 1; // Till we know otherwise...

    return af_lib;
}

af_lib_error_t af_lib_send_set_response(af_lib_t *af_lib, const uint16_t attribute_id, bool set_succeeded, const uint16_t value_len, const uint8_t *value) {
    uint8_t state;
    uint8_t reason;
    af_lib_error_t result;

    if (NULL == af_lib->read_cmd || af_command_get_attr_id(af_lib->read_cmd) != attribute_id) {
        return AF_ERROR_INVALID_PARAM;
    }

    if (set_succeeded) {
        state = UPDATE_STATE_UPDATED;
        reason = UPDATE_REASON_SERVICE_SET;
    } else {
        state = UPDATE_STATE_FAILED;
        reason = UPDATE_REASON_INTERNAL_SET_REJECTED;
    }
    result = af_lib_set_attribute_complete(af_lib, af_command_get_req_id(af_lib->read_cmd), af_command_get_attr_id(af_lib->read_cmd), value_len, value, state, reason);
    if (result != AF_SUCCESS) {
        af_logger_print_buffer("Can't reply to SET in send_set_response! This is FATAL! rc=");
        af_logger_println_value(result);
        return result;
    }

    af_command_cleanup(af_lib->read_cmd);
    free(af_lib->read_cmd);
    af_lib->read_cmd_offset = 0;
    af_lib->read_cmd = NULL;

    af_lib->state = STATE_IDLE;

    return result;
}

void af_lib_dump_queue() {
    queue_t *p_q = (queue_t *)&s_request_queue;
    af_queue_dump(p_q, dump_queue_element);
}













