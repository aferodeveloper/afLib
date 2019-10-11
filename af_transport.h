/**
 * Copyright 2018 Afero, Inc.
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
#ifndef AF_TRANSPORT_H
#define AF_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include "af_status_command.h"

#ifdef  __cplusplus
extern "C" {
#endif

// The maximum time any transfer operation should take.  If this time elapses then the function should return a AF_ERROR_TIMEOUT result
#define MAX_TRANSFER_TIME_MS                10000

typedef struct af_transport_t af_transport_t; // A specific implementation can fill in what's appropriate for it in their c file

/*
 * checkForInterrupt
 *
 * For interfaces that don't use the interrupt pin to signal they want the state machine to run (like UART).
 */
void af_transport_check_for_interrupt(af_transport_t *af_transport, volatile int *interrupts_pending, bool idle);

/*
 * exchangeStatus
 *
 * Write a status message to the interface and read one back.
 */
int af_transport_exchange_status(af_transport_t *af_transport, af_status_command_t *af_status_command_tx, af_status_command_t *af_status_command_rx);

/*
 * writeStatus
 *
 * Write the specified status message to the interface.
 */
int af_transport_write_status(af_transport_t *af_transport, af_status_command_t *af_status_command);

/*
 * sendBytesOffset
 *
 * Write bytes using an interface specific packet size.
 * It may take multiple calls to this method to write all of the bytes.
 *
 * @param bytes         buffer of bytes to be written.
 * @param bytesToSend   Pointer to count of total number of bytes to be written.
 *                      This value is decremented after each packet is written and will be 0 when all bytes have been sent.
 * @param offset        Pointer to offset into bytes buffer.
 *                      This value is incremented after each packet is written and will equal bytesToSend when all bytes have been sent.
 */
void af_transport_send_bytes_offset(af_transport_t *af_transport, uint8_t *bytes, uint16_t *bytes_to_send, uint16_t *offset);

/*
 * recvBytesOffset
 *
 * Read bytes using an interface specific packet size.
 * It may take multiple calls to this method to read all of the bytes.
 *
 * @param bytes         Pointer to buffer of bytes to read into.
 *                      This buffer is allocated for all bytesToRecv before the first packet is read.
 *                      NOTE: IT IS THE CALLER'S RESPONSIBILITY TO FREE THIS BUFFER.
 * @param bytesToRecv   Pointer to count of total number of bytes to be read.
 *                      This value is decremented after each packet is read and will be 0 when all bytes have been read.
 * @param offset        Pointer to offset into bytes buffer.
 *                      This value is incremented after each packet is read and will equal bytesToRecv when all bytes have been read.
 *
 * @return AF_SUCCESS       - bytes received successfully
 * @return AF_ERROR_TIMEOUT - receiving the bytes took too long, this will effectively make afLib reject the current transfer and go back to the idle state
 */
int af_transport_recv_bytes_offset(af_transport_t *af_transport, uint8_t **bytes, uint16_t *bytes_len, uint16_t *bytes_to_recv, uint16_t *offset);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* AF_TRANSPORT_H */


