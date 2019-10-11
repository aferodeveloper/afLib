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

#include "af_utils.h"

void af_utils_write_little_endian_16(uint16_t n, uint8_t* d) {
    d[0] = (uint8_t)n;
    d[1] = (uint8_t)(n >> 8);
}

uint16_t af_utils_read_little_endian_16(const uint8_t* d) {
    return d[0] | (d[1] << 8);
}

void af_utils_write_little_endian_32(uint32_t n, uint8_t* d) {
    d[0] = (uint8_t)(n & 0xff);
    d[1] = (uint8_t)((n >> 8) & 0xff);
    d[2] = (uint8_t)((n >> 16) & 0xff);
    d[3] = (uint8_t)(n >> 24);
}

uint32_t af_utils_read_little_endian_32(const uint8_t* d) {
    uint32_t res = d[0];
    res |= (d[1] << 8);
    res |= (d[2] << 16);
    res |= (d[3] << 24);
    return res;
}

void af_utils_write_little_endian_64(uint64_t n, uint8_t* d) {
    d[0] = (uint8_t)(n & 0xff);
    d[1] = (uint8_t)((n >> 8) & 0xff);
    d[2] = (uint8_t)((n >> 16) & 0xff);
    d[3] = (uint8_t)((n >> 24) & 0xff);
    d[4] = (uint8_t)((n >> 32) & 0xff);
    d[5] = (uint8_t)((n >> 40) & 0xff);
    d[6] = (uint8_t)((n >> 48) & 0xff);
    d[7] = (uint8_t)((n >> 56));
}

uint64_t af_utils_read_little_endian_64(const uint8_t* d) {
    uint64_t res = d[0];
    res |= ((uint64_t)d[1] << 8);
    res |= ((uint64_t)d[2] << 16);
    res |= ((uint64_t)d[3] << 24);
    res |= ((uint64_t)d[4] << 32);
    res |= ((uint64_t)d[5] << 40);
    res |= ((uint64_t)d[6] << 48);
    res |= ((uint64_t)d[7] << 56);
    return res;
}