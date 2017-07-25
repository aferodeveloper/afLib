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
#ifndef AFLIB_ARDUINOUART_H
#define AFLIB_ARDUINOUART_H

#include "afTransport.h"
#include <SoftwareSerial.h>

#define INT_CHAR                            0x32

class ArduinoUART : public afTransport {
public:

    ArduinoUART(uint8_t rxPin, uint8_t txPin, Stream *theLog);

    virtual void checkForInterrupt(volatile int *interrupts_pending, bool idle);
    virtual int exchangeStatus(StatusCommand *tx, StatusCommand *rx);
    virtual int writeStatus(StatusCommand *c);
    virtual void sendBytes(char *bytes, int len);
    virtual void recvBytes(char *bytes, int len);
    virtual void sendBytesOffset(char *bytes, uint16_t *bytesToSend, uint16_t *offset);
    virtual void recvBytesOffset(char **bytes, uint16_t *bytesLen, uint16_t *bytesToRecv, uint16_t *offset);

private:
    SoftwareSerial *_uart;
    Stream *_theLog;

    int available();
    char peek();
    void read(uint8_t *buffer, int len);
    char read();
    void write(uint8_t *buffer, int len);
};

#endif


