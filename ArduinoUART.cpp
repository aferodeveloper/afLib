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
#include "ArduinoUART.h"
#include "afErrors.h"
#include "msg_types.h"

#define MAX_WAIT_TIME 1000

ArduinoUART::ArduinoUART(uint8_t rxPin, uint8_t txPin, Stream *theLog)
{
    pinMode(rxPin, INPUT);
    pinMode(txPin, OUTPUT);

    _theLog = theLog;
    _uart = new SoftwareSerial(rxPin, txPin);
    _uart->begin(9600);
}

int ArduinoUART::available()
{
    return _uart->available();
}

char ArduinoUART::peek()
{
    return _uart->peek();
}

char ArduinoUART::read()
{
    return _uart->read();
}

void ArduinoUART::read(uint8_t *buffer, int len)
{
    memset(buffer, 0, len);
    for (int i = 0; i < len; i++) {
        int b;
        unsigned long time = millis();
        while (((b = _uart->read()) == -1)) {
            if (millis() - time > MAX_WAIT_TIME) {
                return;
            }
        }
        buffer[i] = (uint8_t )b;
//        _theLog->print("<"); _theLog->println(buffer[i], HEX);
    }
}

void ArduinoUART::write(uint8_t *buffer, int len)
{
    for (int i = 0; i < len; i++) {
//        _theLog->print(">"); _theLog->println(buffer[i], HEX);
        _uart->write(buffer[i]);
    }
}

void ArduinoUART::checkForInterrupt(volatile int *interrupts_pending, bool idle) {
    if (available()) {
        if (peek() == INT_CHAR) {
            if (*interrupts_pending == 0) {
                //_theLog->println("INT");
                read();
                *interrupts_pending += 1;
            } else if (idle) {
                read();
            } else {
                //_theLog->println("INT(Pending)");
            }
        } else {
            if (*interrupts_pending == 0) {
                //_theLog->print("Skipping: "); _theLog->println(peek(), HEX);
                read();
            }
        }
    }
}

int ArduinoUART::exchangeStatus(StatusCommand *tx, StatusCommand *rx) {
    int result = afSUCCESS;
    uint16_t len = tx->getSize();
    int bytes[len];
    char rbytes[len + 1];
    int index = 0;
    tx->getBytes(bytes);

    for (int i=0; i < len; i++)
    {
        rbytes[i]=bytes[i];
    }
    rbytes[len]=tx->getChecksum();
    sendBytes(rbytes, len + 1);

    // Skip any interrupts that may have come in.
    recvBytes(rbytes, 1);
    while (rbytes[0] == INT_CHAR) {
        recvBytes(rbytes, 1);
    }

    // Okay, we have a good first char, now read the rest.
    recvBytes(&rbytes[1], len);

    byte cmd = bytes[index++];
    if (cmd != SYNC_REQUEST && cmd != SYNC_ACK) {
        _theLog->print("exchangeStatus bad cmd: ");
        _theLog->println(cmd, HEX);
        result = afERROR_INVALID_COMMAND;
    }

    rx->setBytesToSend(rbytes[index + 0] | (rbytes[index + 1] << 8));
    rx->setBytesToRecv(rbytes[index + 2] | (rbytes[index + 3] << 8));
    rx->setChecksum(rbytes[index+4]);

    return result;
}

int ArduinoUART::writeStatus(StatusCommand *c) {
    int result = afSUCCESS;
    uint16_t len = c->getSize();
    int bytes[len];
    char rbytes[len+1];
    int index = 0;
    c->getBytes(bytes);

    for (int i=0;i<len;i++)
    {
        rbytes[i]=bytes[i];
    }
    rbytes[len]=c->getChecksum();

    sendBytes(rbytes, len + 1);

    byte cmd = rbytes[index++];
    if (cmd != SYNC_REQUEST && cmd != SYNC_ACK) {
        _theLog->print("writeStatus bad cmd: ");
        _theLog->println(cmd, HEX);
        result = afERROR_INVALID_COMMAND;
    }

//  c->dump();
//  c->dumpBytes();

    return result;
}

void ArduinoUART::sendBytes(char *bytes, int len) {
    write((uint8_t *)bytes, len);
}

void ArduinoUART::recvBytes(char *bytes, int len) {
    read((uint8_t *)bytes, len);
}

void ArduinoUART::sendBytesOffset(char *bytes, uint16_t *bytesToSend, uint16_t *offset)
{
    uint16_t len = 0;

    len = *bytesToSend;

    sendBytes(bytes, len);

//  dumpBytes("Sending:", len, bytes);

    *offset += len;
    *bytesToSend -= len;
}

void ArduinoUART::recvBytesOffset(char **bytes, uint16_t *bytesLen, uint16_t *bytesToRecv, uint16_t *offset)
{
    uint16_t len = 0;

    len = *bytesToRecv;

    if (*offset == 0) {
        *bytesLen = *bytesToRecv;
        *bytes = new char[*bytesLen];
    }

    char * start = *bytes + *offset;

    recvBytes(start, len);

//  dumpBytes("Receiving:", len, _readBuffer);

    *offset += len;
    *bytesToRecv -= len;
}