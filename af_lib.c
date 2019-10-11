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
 * Prevent the MCU from spamming us with too many setAttribute requests.
 * We do this by waiting a small amount of time in between transactions.
 * This prevents sync retries and allows the module to get it's work done.
 */
#define MIN_TIME_BETWEEN_UPDATES_MILLIS (50)

#define IS_MCU_ATTR(x) (x >= 0 && x < 1024)

#define STATE_IDLE                          0
#define STATE_STATUS_SYNC                   1
#define STATE_STATUS_ACK                    3
#define STATE_SEND_BYTES                    4
#define STATE_RECV_BYTES                    5
#define STATE_CMD_COMPLETE                  6

#define MAX_SYNC_RETRIES    10
static long last_sync = 0;
static int sync_retries = 0;
static long last_complete = 0;

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
};

AF_QUEUE_DECLARE(s_request_queue, sizeof(request_t), REQUEST_QUEUE_SIZE);

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
    AF_QUEUE_INIT(s_request_queue, sizeof(request_t), REQUEST_QUEUE_SIZE);
}

/**
 * queue_put
 *
 * Add an item to the end of the queue. Return an error if we're out of space in the queue.
 */
static int queue_put(af_lib_t *af_lib, uint8_t message_type, uint8_t request_id, const uint16_t attribute_id, uint16_t value_len, const uint8_t *value, const uint8_t status, const uint8_t reason) {
    queue_t volatile *p_q = &s_request_queue;
    request_t *p_event = (request_t *)AF_QUEUE_ELEM_ALLOC_FROM_INTERRUPT(p_q);
    if (p_event != NULL) {
        p_event->message_type = message_type;
        p_event->attr_id = attribute_id;
        p_event->request_id = request_id;
        p_event->value_len = value_len;
        p_event->value = (uint8_t*)malloc(value_len);
        memcpy(p_event->value, value, value_len);
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
}

static int af_lib_set_attribute_complete(af_lib_t *af_lib, uint8_t request_id, const uint16_t attr_id, const uint16_t value_len, const uint8_t *value, uint8_t status, uint8_t reason) {
    if (value_len > MAX_ATTRIBUTE_SIZE) {
        return AF_ERROR_INVALID_PARAM;
    }

    if (value == NULL) {
        return AF_ERROR_INVALID_PARAM;
    }

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
    af_command_initialize_with_status(af_lib->write_cmd, request_id, MSG_TYPE_UPDATE, attr_id, status, reason, value_len, value);
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
    af_status_command_set_bytes_to_recv(&af_lib->rx_status, 0);

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
    af_transport_recv_bytes_offset(af_lib->the_transport, &af_lib->read_buffer, &af_lib->read_buffer_len, &af_lib->bytes_to_recv, &af_lib->read_cmd_offset);
    if (0 == af_lib->bytes_to_recv) {
        af_lib->state = STATE_CMD_COMPLETE;
        print_state(af_lib->state);
        af_lib->read_cmd = (af_command_t*)malloc(sizeof(af_command_t));
        af_command_initialize_from_buffer(af_lib->read_cmd, af_lib->read_buffer_len, &af_lib->read_buffer[2]);
        //_readCmd->dumpBytes();
        free(af_lib->read_buffer);
        af_lib->read_buffer = NULL;
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

    af_lib->state = STATE_IDLE;
    print_state(af_lib->state);
    if (af_lib->read_cmd != NULL) {
        uint8_t *val = (uint8_t*)malloc(af_command_get_value_len(af_lib->read_cmd));
        af_command_get_value(af_lib->read_cmd, val);

        uint8_t state;
        uint8_t reason;

        switch (af_command_get_command(af_lib->read_cmd)) {
            case MSG_TYPE_SET:
                if (af_lib->attr_set_handler(af_command_get_req_id(af_lib->read_cmd), af_command_get_attr_id(af_lib->read_cmd), af_command_get_value_len(af_lib->read_cmd), val)) {
                    state = UPDATE_STATE_UPDATED;
                    reason = UPDATE_REASON_SERVICE_SET;
                } else {
                    state = UPDATE_STATE_FAILED;
                    reason = UPDATE_REASON_INTERNAL_SET_FAIL;
                }
                result = af_lib_set_attribute_complete(af_lib, af_command_get_req_id(af_lib->read_cmd), af_command_get_attr_id(af_lib->read_cmd), af_command_get_value_len(af_lib->read_cmd), val, state, reason);
                if (result != AF_SUCCESS) {
                    af_logger_println_buffer("Can't reply to SET! This is FATAL!");
                }

                break;

            case MSG_TYPE_UPDATE:
                // If the attr update is a "fake" update, don't send it to the MCU
                if (af_command_get_reason(af_lib->read_cmd) != UPDATE_REASON_FAKE_UPDATE) {
                    if (af_command_get_attr_id(af_lib->read_cmd) == af_lib->outstanding_set_get_attr_id) {
                        af_lib->outstanding_set_get_attr_id = 0;
                    }
                    static bool inNotifyHandler;
                    if (!inNotifyHandler) {
                        inNotifyHandler = true;
                        af_lib->attr_notify_handler(af_command_get_req_id(af_lib->read_cmd), af_command_get_attr_id(af_lib->read_cmd), af_command_get_value_len(af_lib->read_cmd), val);
                        inNotifyHandler = false;
                    }
                    last_complete = af_utils_millis();
                }
                break;

            default:
                break;
        }
        free(val);
        af_command_cleanup(af_lib->read_cmd);
        free(af_lib->read_cmd);
        af_lib->read_cmd_offset = 0;
        af_lib->read_cmd = NULL;
    }

    if (af_lib->write_cmd != NULL) {
        // Fake a callback here for MCU attributes as we don't get one from the module.
        if (af_command_get_command(af_lib->write_cmd) == MSG_TYPE_UPDATE && IS_MCU_ATTR(af_command_get_attr_id(af_lib->write_cmd))) {
            af_lib->attr_notify_handler(af_command_get_req_id(af_lib->write_cmd), af_command_get_attr_id(af_lib->write_cmd), af_command_get_value_len(af_lib->write_cmd), af_command_get_value_pointer(af_lib->write_cmd));
            last_complete = af_utils_millis();
        }
        af_command_cleanup(af_lib->write_cmd);
        free(af_lib->write_cmd);
        af_lib->write_cmd = NULL;
        af_lib->write_cmd_offset = 0;
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
        //af_logger_print_buffer("interrupts_pending: "); af_logger_println_value(af_lib->interrupts_pending);

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
        }
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

    return af_lib;
}

void af_lib_destroy(af_lib_t* af_lib) {
    af_status_command_cleanup(&af_lib->tx_status);
    af_status_command_cleanup(&af_lib->rx_status);
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
int af_lib_get_attribute(af_lib_t *af_lib, const uint16_t attr_id) {
    af_lib->request_id++;
    uint8_t dummy; // This value isn't actually used.
    return queue_put(af_lib, MSG_TYPE_GET, af_lib->request_id, attr_id, 0, &dummy, 0, 0);
}

/**
 * The many moods of setAttribute
 *
 * These are the public versions of the setAttribute method.
 * These methods queue the operation and return immediately. Applications must call loop() for the operation to complete.
 */
int af_lib_set_attribute_bool(af_lib_t *af_lib, const uint16_t attr_id, const bool value) {
    af_lib->request_id++;
    uint8_t val = value ? 1 : 0;
    return queue_put(af_lib, IS_MCU_ATTR(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, sizeof(val), (uint8_t *)&val, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int af_lib_set_attribute_8(af_lib_t *af_lib, const uint16_t attr_id, const int8_t value) {
    af_lib->request_id++;
    return queue_put(af_lib, IS_MCU_ATTR(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, sizeof(value), (uint8_t *)&value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int af_lib_set_attribute_16(af_lib_t *af_lib, const uint16_t attr_id, const int16_t value) {
    af_lib->request_id++;
    return queue_put(af_lib, IS_MCU_ATTR(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, sizeof(value), (uint8_t *) &value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int af_lib_set_attribute_32(af_lib_t *af_lib, const uint16_t attr_id, const int32_t value) {
    af_lib->request_id++;
    return queue_put(af_lib, IS_MCU_ATTR(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, sizeof(value), (uint8_t *) &value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int af_lib_set_attribute_64(af_lib_t *af_lib, const uint16_t attr_id, const int64_t value) {
    af_lib->request_id++;
    return queue_put(af_lib, IS_MCU_ATTR(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, sizeof(value), (uint8_t *) &value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int af_lib_set_attribute_str(af_lib_t *af_lib, const uint16_t attr_id, const uint16_t value_len, const char *value) {
    if (value_len > MAX_ATTRIBUTE_SIZE) {
        return AF_ERROR_INVALID_PARAM;
    }

    if (value == NULL) {
        return AF_ERROR_INVALID_PARAM;
    }

    af_lib->request_id++;
    return queue_put(af_lib, IS_MCU_ATTR(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, value_len, (const uint8_t *) value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int af_lib_set_attribute_bytes(af_lib_t *af_lib, const uint16_t attr_id, const uint16_t value_len, const uint8_t *value) {
    if (value_len > MAX_ATTRIBUTE_SIZE) {
        return AF_ERROR_INVALID_PARAM;
    }

    if (value == NULL) {
        return AF_ERROR_INVALID_PARAM;
    }

    af_lib->request_id++;
    return queue_put(af_lib, IS_MCU_ATTR(attr_id) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, af_lib->request_id, attr_id, value_len, value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
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
    last_complete = 0;
    return 0 == af_lib->interrupts_pending && STATE_IDLE == af_lib->state && 0 == af_lib->outstanding_set_get_attr_id;
}

void af_lib_mcu_isr(af_lib_t *af_lib) {
#if (defined(DEBUG_TRANSPORT) && DEBUG_TRANSPORT > 0)
    af_logger_println_buffer("mcuISR");
#endif
    af_lib_update_ints_pending(af_lib, 1);
}












