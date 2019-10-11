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

#include <stdlib.h>

#include "af_status_command.h"
#include "af_logger.h"

void af_status_command_initialize(af_status_command_t *af_status_command) {
    memset(af_status_command, 0, sizeof(af_status_command_t));
    af_status_command->cmd = AF_STATUS_COMMAND_STATUS;
}

void af_status_command_initialize_with_bytes_to_send(af_status_command_t *af_status_command, uint16_t bytes_to_send) {
    af_status_command_initialize(af_status_command);
    af_status_command->bytes_to_send = bytes_to_send;
}

void af_status_command_cleanup(af_status_command_t *af_status_command) {
    memset(af_status_command, 0, sizeof(af_status_command_t));
}

uint16_t af_status_command_get_size(af_status_command_t *af_status_command) {
    return sizeof(af_status_command->cmd) + sizeof(af_status_command->bytes_to_send) + sizeof(af_status_command->bytes_to_recv);
}

uint16_t af_status_command_get_bytes(af_status_command_t *af_status_command, uint8_t *bytes) {
    int index = 0;

    bytes[index++] = (af_status_command->cmd);
    bytes[index++] = (af_status_command->bytes_to_send & 0xff);
    bytes[index++] = ((af_status_command->bytes_to_send >> 8) & 0xff);
    bytes[index++] = (af_status_command->bytes_to_recv & 0xff);
    bytes[index++] = ((af_status_command->bytes_to_recv >> 8) & 0xff);

    return index;
}

void af_status_command_set_checksum(af_status_command_t *af_status_command, uint8_t checksum) {
    af_status_command->checksum = checksum;
}

uint8_t af_status_command_get_checksum(af_status_command_t *af_status_command) {
    uint8_t result = 0;

    result += (af_status_command->cmd);
    result += (af_status_command->bytes_to_send & 0xff);
    result += ((af_status_command->bytes_to_send >> 8) & 0xff);
    result += (af_status_command->bytes_to_recv & 0xff);
    result += ((af_status_command->bytes_to_recv >> 8) & 0xff);

    return result;
}

void af_status_command_set_ack(af_status_command_t *af_status_command, bool ack) {
    af_status_command->cmd = ack ? AF_STATUS_COMMAND_STATUS_ACK : AF_STATUS_COMMAND_STATUS;
}

void af_status_command_set_bytes_to_send(af_status_command_t *af_status_command, uint16_t bytes_to_send) {
    af_status_command->bytes_to_send = bytes_to_send;
}

uint16_t af_status_command_get_bytes_to_send(af_status_command_t *af_status_command) {
    return af_status_command->bytes_to_send;
}

void af_status_command_set_bytes_to_recv(af_status_command_t *af_status_command, uint16_t bytes_to_recv) {
    af_status_command->bytes_to_recv = bytes_to_recv;
}

uint16_t af_status_command_get_bytes_to_recv(af_status_command_t *af_status_command) {
    return af_status_command->bytes_to_recv;
}

bool af_status_command_equals(af_status_command_t *af_status_command_me, af_status_command_t *af_status_command_other) {
    return memcmp(af_status_command_me, af_status_command_other, af_status_command_get_size(af_status_command_me)) == 0;
}

bool af_status_command_is_valid(af_status_command_t *af_status_command) {
    return (af_status_command->checksum == af_status_command_get_checksum(af_status_command)) && (AF_STATUS_COMMAND_STATUS == af_status_command->cmd || AF_STATUS_COMMAND_STATUS_ACK == af_status_command->cmd);
}

void af_status_command_dump(af_status_command_t *af_status_command) {
    af_logger_print_buffer("cmd              : ");
    af_logger_println_buffer(AF_STATUS_COMMAND_STATUS == af_status_command->cmd ? "STATUS" : "STATUS_ACK");
    af_logger_print_buffer("bytes to send    : ");
    af_logger_println_formatted_value(af_status_command->bytes_to_send, AF_LOGGER_DEC);
    af_logger_print_buffer("bytes to receive : ");
    af_logger_println_formatted_value(af_status_command->bytes_to_recv, AF_LOGGER_DEC);
}

void af_status_command_dump_bytes(af_status_command_t *af_status_command) {
    int len = af_status_command_get_size(af_status_command);
    uint8_t *bytes;
    int i = 0;
    int b;

    bytes = (uint8_t *)malloc(len);
    if (bytes == NULL) {
        af_logger_print_buffer("malloc failed!");
        return;
    }

    af_status_command_get_bytes(af_status_command, bytes);

    af_logger_print_buffer("len  : ");
    af_logger_println_value(len);
    af_logger_print_buffer("data : ");
    for (i = 0; i < len; i++) {
        if (i > 0) {
            af_logger_print_buffer(", ");
        }
        b = bytes[i] & 0xff;
        if (b < 0x10) {
            af_logger_print_buffer("0x0");
            af_logger_print_formatted_value(b, AF_LOGGER_HEX);
        } else {
            af_logger_print_buffer("0x");
            af_logger_print_formatted_value(b, AF_LOGGER_HEX);
        }
    }
    af_logger_println_buffer("");

    free(bytes);
}

