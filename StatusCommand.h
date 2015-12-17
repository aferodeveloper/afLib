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

#ifndef STATUS_COMMAND_H__
#define STATUS_COMMAND_H__

#include "Arduino.h"

class StatusCommand {
public:
    StatusCommand();

    StatusCommand(uint16_t bytesToSend);

    ~StatusCommand();

    uint16_t getSize();

    uint16_t getBytes(int *bytes);

    uint8_t calcChecksum();

    void setChecksum(uint8_t checksum);

    uint8_t getChecksum();

    void setAck(bool ack);

    void setBytesToSend(uint16_t bytesToSend);

    uint16_t getBytesToSend();

    void setBytesToRecv(uint16_t bytesToRecv);

    uint16_t getBytesToRecv();

    bool equals(StatusCommand *statusCommand);

    bool isValid();

    void dump();

    void dumpBytes();

private:
    uint8_t     _cmd;
    uint16_t    _bytesToSend;
    uint16_t    _bytesToRecv;
    uint8_t     _checksum;
};

#endif // STATUS_COMMAND_H__

