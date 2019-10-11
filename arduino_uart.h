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

#ifndef AF_ARDUINO_UART_TRANSPORT_H
#define AF_ARDUINO_UART_TRANSPORT_H

#include "af_transport.h"

// You shouldn't call this directly but instead use the arduino_transport_create_uart call
af_transport_t* arduino_uart_create(uint8_t rxPin, uint8_t txPin);

void af_transport_check_for_interrupt_uart(af_transport_t *af_transport, volatile int *interrupts_pending, bool idle);
int af_transport_exchange_status_uart(af_transport_t *af_transport, af_status_command_t *af_status_command_tx, af_status_command_t *af_status_command_rx);
int af_transport_write_status_uart(af_transport_t *af_transport, af_status_command_t *af_status_command);
void af_transport_send_bytes_uart(af_transport_t *af_transport, uint8_t *bytes, uint16_t len);
void af_transport_recv_bytes_uart(af_transport_t *af_transport, uint8_t *bytes, uint16_t len);
void af_transport_send_bytes_offset_uart(af_transport_t *af_transport, uint8_t *bytes, uint16_t *bytes_to_send, uint16_t *offset);
void af_transport_recv_bytes_offset_uart(af_transport_t *af_transport, uint8_t **bytes, uint16_t *bytes_len, uint16_t *bytes_to_recv, uint16_t *offset);

void arduino_uart_destroy(af_transport_t *af_transport);

#endif /* AF_ARDUINO_UART_TRANSPORT_H */