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

#ifndef AF_ARDUINO_TRANSPORT_H
#define AF_ARDUINO_TRANSPORT_H

#include "af_transport.h"

af_transport_t* arduino_transport_create_spi(int chipSelect);
af_transport_t* arduino_transport_create_uart(uint8_t rxPin, uint8_t txPin);

void arduino_transport_destroy(af_transport_t *af_transport);

#endif /* AF_ARDUINO_TRANSPORT_H */