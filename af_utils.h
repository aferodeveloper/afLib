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
#ifndef AF_UTILS_H
#define AF_UTILS_H

#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * Utility function to return the millisecond time since EPOCH
 *
 * @return
 */
long af_utils_millis();


/**
 * Various functions to read/write Little Endian values
 */
void af_utils_write_little_endian_16(uint16_t n, uint8_t* d);
uint16_t af_utils_read_little_endian_16(const uint8_t* d);

void af_utils_write_little_endian_32(uint32_t n, uint8_t* d);
uint32_t af_utils_read_little_endian_32(const uint8_t* d);

void af_utils_write_little_endian_64(uint64_t n, uint8_t* d);
uint64_t af_utils_read_little_endian_64(const uint8_t* d);


#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* AF_UTILS_H */

