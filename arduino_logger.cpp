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

#include "Arduino.h"

#include "af_logger.h"
#include "arduino_logger.h"

void arduino_logger_start(int baud_rate) {
    Serial.begin(baud_rate);
    while (!Serial) {
        ;
    }
}

void arduino_logger_stop() {
    Serial.end();
}

void af_logger_print_value(int32_t val) {
    Serial.print(val);
    Serial.flush();
}

void af_logger_print_buffer(const char* val) {
    Serial.print(val);
    Serial.flush();
}

void af_logger_print_formatted_value(int32_t val, af_logger_format_t format) {
    Serial.print(val, format);
    Serial.flush();
}

void af_logger_println_value(int32_t val) {
    Serial.println(val);
    Serial.flush();
}

void af_logger_println_buffer(const char* val) {
    Serial.println(val);
    Serial.flush();
}

void af_logger_println_formatted_value(int32_t val, af_logger_format_t format) {
    Serial.println(val, format);
    Serial.flush();
}
