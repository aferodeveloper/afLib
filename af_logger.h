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
#ifndef AF_LOGGER_H
#define AF_LOGGER_H

#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
    AF_LOGGER_BIN = 2,
    AF_LOGGER_OCT = 8,
    AF_LOGGER_DEC = 10,
    AF_LOGGER_HEX = 16
} af_logger_format_t;

void af_logger_print_value(int32_t val);
void af_logger_print_buffer(const char* val);
void af_logger_print_formatted_value(int32_t val, af_logger_format_t format);
void af_logger_println_value(int32_t val);
void af_logger_println_buffer(const char* val);
void af_logger_println_formatted_value(int32_t val, af_logger_format_t format);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* AF_LOGGER_H */

