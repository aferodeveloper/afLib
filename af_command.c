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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "af_command.h"
#include "af_msg_types.h"
#include "af_logger.h"

#define CMD_HDR_LEN  4    // 4 byte header on all commands
#define CMD_VAL_LEN  2    // 2 byte value length for commands that have a value

const char *CMD_NAMES[] = {"SET   ", "GET   ", "UPDATE"};

static uint8_t str_to_cmd(char *cmdStr) {
    char c = cmdStr[0];
    if (c == 'g' || c == 'G') {
        return MSG_TYPE_GET;
    } else if (c == 's' || c == 'S') {
        return MSG_TYPE_SET;
    } else if (c == 'u' || c == 'U') {
        return MSG_TYPE_UPDATE;
    }

    return -1;
}

static uint16_t str_to_attr_id(char *attrIdStr) {
    return atoi(attrIdStr);
}

static uint8_t get_val(char c) {
    if (c >= '0' && c <= '9')
        return (uint8_t)(c - '0');
    else if (c >= 'A' && c <= 'F')
        return (uint8_t)(c - 'A' + 10);
    else if (c >= 'a' && c <= 'f')
        return (uint8_t)(c - 'a' + 10);

    af_logger_print_buffer("bad hex char: ");
    af_logger_println_value(c);

    return 0;
}

static int str_to_value(char *valueStr, uint8_t *value) {
    int i = 0;
    for (i = 0; i < (int) (strlen(valueStr) / 2); i++) {
        int j = i * 2;
        value[i] = get_val(valueStr[j + 1]) + (get_val(valueStr[j]) << 4);
    }

    return 0;
}


void af_command_initialize_from_buffer(af_command_t *af_command, uint16_t len, uint8_t *bytes) {
    int index = 0;
    memset(af_command, 0, sizeof(af_command_t)); 

    af_command->cmd = bytes[index++];
    af_command->request_id = bytes[index++];
    af_command->attr_id = bytes[index + 0] | bytes[index + 1] << 8;
    index += 2;

    if (MSG_TYPE_GET == af_command->cmd) {
        return;
    }
    if (MSG_TYPE_UPDATE == af_command->cmd) {
        af_command->state = bytes[index++];
        af_command->reason = bytes[index++];
    }
    if (MSG_TYPE_UPDATE_REJECTED == af_command->cmd) {
        af_command->state = bytes[index++];
        af_command->reason = bytes[index++];
        return;
    }

    af_command->value_len = bytes[index + 0] | bytes[index + 1] << 8;
    index += 2;
    af_command->value = (uint8_t*)malloc(af_command->value_len);
    memcpy(af_command->value, bytes + index, af_command->value_len);
}

void af_command_initialize_from_string(af_command_t *af_command, uint8_t request_id, const char *str) {
    char *cp; 
    char *tok;

    cp = strdup(str); 
    tok = strtok(cp, " ");

    memset(af_command, 0, sizeof(af_command_t));

    af_command->request_id = request_id;

    af_command->cmd = str_to_cmd(tok);

    tok = strtok(NULL, " ");
    af_command->attr_id = str_to_attr_id(tok);

    if (af_command->cmd != MSG_TYPE_GET) {
        tok = strtok(NULL, " ");
        af_command->value_len = strlen(tok) / 2;
        af_command->value = (uint8_t*)malloc(af_command->value_len);
        str_to_value(tok, af_command->value);
    }

    free(cp);
}

void af_command_initialize_with_attr_id(af_command_t *af_command, uint8_t request_id, uint8_t cmd, uint16_t attr_id) {
    memset(af_command, 0, sizeof(af_command_t));
    af_command->request_id = request_id;
    af_command->cmd = cmd;
    af_command->attr_id = attr_id;
}

void af_command_initialize_with_value(af_command_t *af_command, uint8_t request_id, uint8_t cmd, uint16_t attr_id, uint16_t value_len, uint8_t *value) {
    memset(af_command, 0, sizeof(af_command_t));
    af_command->request_id = request_id;
    af_command->cmd = cmd;
    af_command->attr_id = attr_id;
    af_command->value_len = value_len;
    af_command->value = (uint8_t*)malloc(af_command->value_len);
    memcpy(af_command->value, value, af_command->value_len);
}

void af_command_initialize_with_status(af_command_t *af_command, uint8_t request_id, uint8_t cmd, uint16_t attr_id, uint8_t state, uint8_t reason, uint16_t value_len, uint8_t *value) {
    memset(af_command, 0, sizeof(af_command_t));
    af_command->request_id = request_id;
    af_command->cmd = cmd;
    af_command->attr_id = attr_id;
    af_command->state = state;
    af_command->reason = reason;
    af_command->value_len = value_len;
    af_command->value = (uint8_t*)malloc(af_command->value_len);
    memcpy(af_command->value, value, af_command->value_len);
}

void af_command_initialize(af_command_t *af_command) {
    memset(af_command, 0, sizeof(af_command_t));
}

void af_command_cleanup(af_command_t *af_command) {
    if (af_command->value != NULL) {
        free(af_command->value);
    }
    memset(af_command, 0, sizeof(af_command_t));
}

uint8_t af_command_get_command(af_command_t *af_command) {
    return af_command->cmd;
}

uint8_t af_command_get_req_id(af_command_t *af_command) {
    return af_command->request_id;
}

uint16_t af_command_get_attr_id(af_command_t *af_command) {
    return af_command->attr_id;
}

uint16_t af_command_get_value_len(af_command_t *af_command) {
    return af_command->value_len;
}

void af_command_get_value(af_command_t *af_command, uint8_t *value) {
    memcpy(value, af_command->value, af_command->value_len);
}

const uint8_t *af_command_get_value_pointer(af_command_t *af_command) {
    return af_command->value;
}

uint16_t af_command_get_size(af_command_t *af_command) {
    uint16_t len = CMD_HDR_LEN;

    if (af_command->cmd != MSG_TYPE_GET) {
        len += CMD_VAL_LEN + af_command->value_len;
    }

    if (af_command->cmd == MSG_TYPE_UPDATE) {
        len += 2; // status byte + reason byte
    }

    return len;
}

uint16_t af_command_get_bytes(af_command_t *af_command, uint8_t *bytes) {
    uint16_t len = af_command_get_size(af_command);

    int index = 0;

    bytes[index++] = (af_command->cmd);
    bytes[index++] = (af_command->request_id);
    bytes[index++] = (af_command->attr_id & 0xff);
    bytes[index++] = ((af_command->attr_id >> 8) & 0xff);

    if (MSG_TYPE_GET == af_command->cmd) {
        return len;
    }

    if (MSG_TYPE_UPDATE == af_command->cmd) {
        bytes[index++] = (af_command->state);
        bytes[index++] = (af_command->reason);
    }

    bytes[index++] = (af_command->value_len & 0xff);
    bytes[index++] = ((af_command->value_len >> 8) & 0xff);

    memcpy(bytes + index, af_command->value, af_command->value_len);

    return len;
}

uint8_t af_command_get_reason(af_command_t *af_command) {
    return af_command->reason;
}

uint8_t af_command_get_state(af_command_t *af_command) {
    return af_command->state;
}

bool af_command_is_valid(af_command_t *af_command) {
    return (MSG_TYPE_SET == af_command->cmd) || (MSG_TYPE_GET == af_command->cmd) || (MSG_TYPE_UPDATE == af_command->cmd);
}

void af_command_dump(af_command_t *af_command) {
    memset(af_command->print_buf, 0, MAX_PRINT_BUFFER);
    snprintf(af_command->print_buf, MAX_PRINT_BUFFER, "cmd: %s attr: %d value: ", CMD_NAMES[af_command->cmd - MESSAGE_CHANNEL_BASE - 1], af_command->attr_id);
    if (af_command->cmd != MSG_TYPE_GET) {
        uint16_t size_so_far = strlen(af_command->print_buf);
        int i = 0;
        for (i = 0; i < af_command->value_len; i++) {
            sprintf(af_command->print_buf + size_so_far + i, "%02x", af_command->value[i]);
        }
    }

    af_logger_println_buffer(af_command->print_buf);
}

void af_command_dump_bytes(af_command_t *af_command) {
    uint16_t len = af_command_get_size(af_command);
    uint8_t *bytes = malloc(len); 
    uint16_t size_so_far;
    int i = 0;
    
    if (bytes == NULL) {
    	af_logger_println_buffer("af_command_dump_bytes: malloc failed");
    	return;
    }
    
    af_command_get_bytes(af_command, bytes);

    memset(af_command->print_buf, 0, MAX_PRINT_BUFFER);
    snprintf(af_command->print_buf, MAX_PRINT_BUFFER, "len: %d value: ", len);
    size_so_far = strlen(af_command->print_buf);
    for (i = 0; i < len; i++) {
        sprintf(af_command->print_buf + size_so_far + i, "%02x", af_command->value[i]);
    }
    af_logger_println_buffer(af_command->print_buf);
    
    free(bytes);
}


