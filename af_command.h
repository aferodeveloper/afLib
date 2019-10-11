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

#ifndef AF_COMMAND_H
#define AF_COMMAND_H

#include <stdbool.h>
#include "af_msg_types.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define MAX_PRINT_BUFFER    256

typedef struct {
    uint16_t    len;
    uint8_t     cmd;
    uint8_t     request_id;
    uint16_t    attr_id;
    uint8_t     status;
    uint8_t     reason;
    uint16_t    value_len;
    uint8_t     *value;
    char        print_buf[MAX_PRINT_BUFFER];
} af_command_t;

void af_command_initialize_from_buffer(af_command_t *af_command, uint16_t len, uint8_t *bytes);
void af_command_initialize_from_string(af_command_t *af_command, uint8_t request_id, const char *str);
void af_command_initialize_with_attr_id(af_command_t *af_command, uint8_t request_id, uint8_t cmd, uint16_t attr_id);
void af_command_initialize_with_value(af_command_t *af_command, uint8_t request_id, uint8_t cmd, uint16_t attr_id, uint16_t value_len, uint8_t *value);
void af_command_initialize_with_status(af_command_t *af_command, uint8_t request_id, uint8_t cmd, uint16_t attr_id, uint8_t status, uint8_t reason, uint16_t value_len, uint8_t *value);
void af_command_initialize(af_command_t *af_command);
void af_command_cleanup(af_command_t *af_command);

uint8_t af_command_get_command(af_command_t *af_command);

uint8_t af_command_get_req_id(af_command_t *af_command);

uint16_t af_command_get_attr_id(af_command_t *af_command);

uint16_t af_command_get_value_len(af_command_t *af_command);

void af_command_get_value(af_command_t *af_command, uint8_t *value);

const uint8_t *af_command_get_value_pointer(af_command_t *af_command);

uint16_t af_command_get_size(af_command_t *af_command);

uint16_t af_command_get_bytes(af_command_t *af_command, uint8_t *bytes);

uint8_t af_command_get_reason(af_command_t *af_command);

bool af_command_is_valid(af_command_t *af_command);

void af_command_dump(af_command_t *af_command);

void af_command_dump_bytes(af_command_t *af_command);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* AF_COMMAND_H */

