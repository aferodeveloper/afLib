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

#include "Arduino.h"
#include "StatusCommand.h"

StatusCommand::StatusCommand(uint16_t bytesToSend) {
    _cmd = 0x30;
    _bytesToSend = bytesToSend;
    _bytesToRecv = 0;
}

StatusCommand::StatusCommand() {
    _cmd = 0x30;
    _bytesToSend = 0;
    _bytesToRecv = 0;
}

StatusCommand::~StatusCommand() {
}

uint16_t StatusCommand::getSize() {
    return sizeof(_cmd) + sizeof(_bytesToSend) + sizeof(_bytesToRecv);
}

uint16_t StatusCommand::getBytes(int *bytes) {
    int index = 0;

    bytes[index++] = (_cmd);
    bytes[index++] = (_bytesToSend & 0xff);
    bytes[index++] = ((_bytesToSend >> 8) & 0xff);
    bytes[index++] = (_bytesToRecv & 0xff);
    bytes[index++] = ((_bytesToRecv >> 8) & 0xff);

    return index;
}

uint8_t StatusCommand::calcChecksum() {
    uint8_t result = 0;

    result += (_cmd);
    result += (_bytesToSend & 0xff);
    result += ((_bytesToSend >> 8) & 0xff);
    result += (_bytesToRecv & 0xff);
    result += ((_bytesToRecv >> 8) & 0xff);

    return result;
}

void StatusCommand::setChecksum(uint8_t checksum) {
    _checksum = checksum;
}

uint8_t StatusCommand::getChecksum() {
    uint8_t result = 0;

    result += (_cmd);
    result += (_bytesToSend & 0xff);
    result += ((_bytesToSend >> 8) & 0xff);
    result += (_bytesToRecv & 0xff);
    result += ((_bytesToRecv >> 8) & 0xff);

    return result;
}

void StatusCommand::setAck(bool ack) {
    _cmd = ack ? 0x31 : 0x30;
}

void StatusCommand::setBytesToSend(uint16_t bytesToSend) {
    _bytesToSend = bytesToSend;
}

uint16_t StatusCommand::getBytesToSend() {
    return _bytesToSend;
}

void StatusCommand::setBytesToRecv(uint16_t bytesToRecv) {
    _bytesToRecv = bytesToRecv;
}

uint16_t StatusCommand::getBytesToRecv() {
    return _bytesToRecv;
}

bool StatusCommand::equals(StatusCommand *statusCommand) {
    return (_cmd == statusCommand->_cmd && _bytesToSend == statusCommand->_bytesToSend &&
            _bytesToRecv == statusCommand->_bytesToRecv);
}

bool StatusCommand::isValid() {
    return (_checksum == calcChecksum()) && (_cmd == 0x30 || _cmd == 0x31);
}

void StatusCommand::dumpBytes() {
    int len = getSize();
    int bytes[len];
    getBytes(bytes);

    Serial.print("len  : ");
    Serial.println(len);
    Serial.print("data : ");
    for (int i = 0; i < len; i++) {
        if (i > 0) {
            Serial.print(", ");
        }
        int b = bytes[i] & 0xff;
        if (b < 0x10) {
            Serial.print("0x0");
            Serial.print(b, HEX);
        } else {
            Serial.print("0x");
            Serial.print(b, HEX);
        }
    }
    Serial.println("");
}

void StatusCommand::dump() {
    Serial.print("cmd              : ");
    Serial.println(_cmd == 0x30 ? "STATUS" : "STATUS_ACK");
    Serial.print("bytes to send    : ");
    Serial.println(_bytesToSend, DEC);
    Serial.print("bytes to receive : ");
    Serial.println(_bytesToRecv, DEC);
}


