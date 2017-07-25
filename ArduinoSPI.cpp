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
#include "ArduinoSPI.h"
#include "afErrors.h"
#include "msg_types.h"


ArduinoSPI::ArduinoSPI(int chipSelect, Stream *theLog)
{
    _theLog = theLog;
    _chipSelect = chipSelect;
    _spiSettings = SPISettings(1000000, LSBFIRST, SPI_MODE0);
    begin();
}

void ArduinoSPI::begin()
{
    pinMode(_chipSelect, OUTPUT);
    digitalWrite(_chipSelect, HIGH);
    SPI.begin();
}
void ArduinoSPI::beginSPI() /* settings are in this class */
{
    SPI.beginTransaction(_spiSettings);
    digitalWrite(_chipSelect, LOW);
    delayMicroseconds(8);
}

void ArduinoSPI::endSPI()
{
    //delayMicroseconds(1);
    digitalWrite(_chipSelect, HIGH);
    SPI.endTransaction();
}
void ArduinoSPI::transfer(char *bytes, int len)
{
    SPI.transfer(bytes, len);
}

void ArduinoSPI::checkForInterrupt(volatile int *interrupts_pending, bool idle)
{
    // Nothing required here.
}

int ArduinoSPI::exchangeStatus(StatusCommand *tx, StatusCommand *rx)
{
    int result = afSUCCESS;
    uint16_t len = tx->getSize();
    int bytes[len];
    char rbytes[len + 1];
    int index = 0;
    tx->getBytes(bytes);

    beginSPI();

    for (int i=0; i < len; i++)
    {
        rbytes[i]=bytes[i];
    }
    rbytes[len]=tx->getChecksum();
    transfer(rbytes, len + 1);

    byte cmd = bytes[index++];
    if (cmd != SYNC_REQUEST && cmd != SYNC_ACK) {
        _theLog->print("exchangeStatus bad cmd: ");
        _theLog->println(cmd, HEX);
        result = afERROR_INVALID_COMMAND;
    }

    rx->setBytesToSend(rbytes[index + 0] | (rbytes[index + 1] << 8));
    rx->setBytesToRecv(rbytes[index + 2] | (rbytes[index + 3] << 8));
    rx->setChecksum(rbytes[index+4]);

    endSPI();

    return result;
}

int ArduinoSPI::writeStatus(StatusCommand *c)
{
    int result = afSUCCESS;
    uint16_t len = c->getSize();
    int bytes[len];
    char rbytes[len+1];
    int index = 0;
    c->getBytes(bytes);

    beginSPI();

    for (int i=0;i<len;i++)
    {
        rbytes[i]=bytes[i];
    }
    rbytes[len]=c->getChecksum();
    transfer(rbytes, len + 1);

    byte cmd = rbytes[index++];
    if (cmd != SYNC_REQUEST && cmd != SYNC_ACK) {
        _theLog->print("writeStatus bad cmd: ");
        _theLog->println(cmd, HEX);
        result = afERROR_INVALID_COMMAND;
    }

    endSPI();

    return result;
}

void ArduinoSPI::sendBytes(char *bytes, int len)
{
    beginSPI();

    transfer(bytes, len);

    endSPI();
}

void ArduinoSPI::recvBytes(char *bytes, int len)
{
    beginSPI();

    transfer(bytes, len);

    endSPI();
}

void ArduinoSPI::sendBytesOffset(char *bytes, uint16_t *bytesToSend, uint16_t *offset)
{
    uint16_t len = 0;

    len = *bytesToSend > SPI_FRAME_LEN ? SPI_FRAME_LEN : *bytesToSend;

    char buffer[len];
    memset(buffer, 0xff, sizeof(buffer));

    memcpy(buffer, &bytes[*offset], len);

    sendBytes(buffer, len);

    *offset += len;
    *bytesToSend -= len;
}

void ArduinoSPI::recvBytesOffset(char **bytes, uint16_t *bytesLen, uint16_t *bytesToRecv, uint16_t *offset)
{
    uint16_t len = 0;

    len = *bytesToRecv > SPI_FRAME_LEN ? SPI_FRAME_LEN : *bytesToRecv;

    if (*offset == 0) {
        *bytesLen = *bytesToRecv;
        *bytes = new char[*bytesLen];
    }

    char *start = *bytes + *offset;

    recvBytes(start, len);

    *offset += len;
    *bytesToRecv -= len;
}