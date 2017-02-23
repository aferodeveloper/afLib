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

#ifndef COMMAND_H__
#define COMMAND_H__

#include "Arduino.h"
#include "msg_types.h"

#define SPI_CMD_MAX_LEN  256

class Command {
public:
    Command(Stream *,uint16_t len, uint8_t *bytes);

    Command(Stream *,uint8_t requestId, const char *str);

    Command(Stream *,uint8_t requestId, uint8_t cmd, uint16_t attrId);

    Command(Stream *,uint8_t requestId, uint8_t cmd, uint16_t attrId, uint16_t valueLen, uint8_t *value);

    Command(Stream *,uint8_t requestId, uint8_t cmd, uint16_t attrId, uint8_t status, uint8_t reason, uint16_t valueLen,
            uint8_t *value);

    Command(Stream *);

    ~Command();

    uint8_t getCommand();

    uint8_t getReqId();

    uint16_t getAttrId();

    uint16_t getValueLen();

    void getValue(uint8_t *value);

    uint8_t *getValueP();

    uint16_t getSize();

    uint16_t getBytes(uint8_t *bytes);

    bool isValid();

    void dump();

    void dumpBytes();

private:
    byte getVal(char c);
    int strToValue(char *valueStr, uint8_t *value);

    uint8_t strToCmd(char *cmdStr);

    uint16_t strToAttrId(char *attrIdStr);

    Stream *_serial;

    uint16_t _len;
    uint8_t _cmd;
    uint8_t _requestId;
    uint16_t _attrId;
    uint8_t _status;
    uint8_t _reason;
    uint16_t _valueLen;
    uint8_t *_value;

    char _printBuf[256];

};

#endif // COMMAND_H__

