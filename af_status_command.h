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

#ifndef AF_STATUS_COMMAND_H
#define AF_STATUS_COMMAND_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef  __cplusplus
extern "C" {
#endif

#define AF_STATUS_COMMAND_STATUS        0x30
#define AF_STATUS_COMMAND_STATUS_ACK    0x31

typedef struct {
    uint8_t     cmd;
    uint16_t    bytes_to_send;
    uint16_t    bytes_to_recv;
    uint8_t     checksum;
} af_status_command_t;

void af_status_command_initialize(af_status_command_t *af_status_command);
void af_status_command_initialize_with_bytes_to_send(af_status_command_t *af_status_command, uint16_t bytes_to_send);
void af_status_command_cleanup(af_status_command_t *af_status_command);

uint16_t af_status_command_get_size(af_status_command_t *af_status_command);

uint16_t af_status_command_get_bytes(af_status_command_t *af_status_command, uint8_t *bytes);

uint8_t af_status_command_get_checksum(af_status_command_t *af_status_command);

void af_status_command_set_checksum(af_status_command_t *af_status_command, uint8_t checksum);

void af_status_command_set_ack(af_status_command_t *af_status_command, bool ack);

void af_status_command_set_bytes_to_send(af_status_command_t *af_status_command, uint16_t bytes_to_send);

uint16_t af_status_command_get_bytes_to_send(af_status_command_t *af_status_command);

void af_status_command_set_bytes_to_recv(af_status_command_t *af_status_command, uint16_t bytes_to_recv);

uint16_t af_status_command_get_bytes_to_recv(af_status_command_t *af_status_command);

bool af_status_command_equals(af_status_command_t *af_status_command_me, af_status_command_t *af_status_command_other);

bool af_status_command_is_valid(af_status_command_t *af_status_command);

void af_status_command_dump(af_status_command_t *af_status_command);

void af_status_command_dump_bytes(af_status_command_t *af_status_command);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* AF_STATUS_COMMAND_H */

