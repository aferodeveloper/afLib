/**
 * Copyright 2015 Afero, Inc.
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
#ifndef AFLIB_AFUART_H
#define AFLIB_AFUART_H

#include <stdint.h>

class afUART {
	public:
	virtual int available() = 0;
	virtual char peek() = 0;
	virtual void read(uint8_t *buffer, int len) = 0;
	virtual char read() = 0;
	virtual void write(uint8_t *buffer, int len) = 0;
};

#endif


