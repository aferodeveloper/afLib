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
#include "afLib.h"
#include "afErrors.h"
#include "msg_types.h"

#define IS_MCU_ATTR(x) (x >= 0 && x < 1024)

static iafLib *_iaflib = NULL;

iafLib *iafLib::create(const int mcuInterrupt, isr isrWrapper,
                       onAttributeSet attrSet, onAttributeSetComplete attrSetComplete, Stream *theLog ,afSPI *theSPI)
{
    if (_iaflib == NULL) {
        _iaflib = new afLib( mcuInterrupt, isrWrapper, attrSet, attrSetComplete, theLog,theSPI);
    }

    return _iaflib;
}

afLib::afLib(const int mcuInterrupt, isr isrWrapper,
             onAttributeSet attrSet, onAttributeSetComplete attrSetComplete, Stream *theLog, afSPI *theSPI)
{
    queueInit();
    _theLog= theLog;
    _theSPI= theSPI;
    _request.p_value = NULL;

    //_spiSettings = SPISettings(1000000, LSBFIRST, SPI_MODE0);
    _interrupts_pending = 0;
    _state = STATE_IDLE;

    _writeCmd = NULL;
    _writeCmdOffset = 0;

    _outstandingSetGetAttrId = 0;

    _readCmd = NULL;
    _readCmdOffset = 0;
    _readBufferLen = 0;

    _txStatus = new StatusCommand(_theLog);
    _rxStatus = new StatusCommand(_theLog);

    _onAttrSet = attrSet;
    _onAttrSetComplete = attrSetComplete;
    _theSPI->begin();

    // AJS where does this get moved to??
    #ifdef ARDUINO
    pinMode(mcuInterrupt, INPUT);
    attachInterrupt(mcuInterrupt, isrWrapper, FALLING);
    #endif


}

#ifndef ARDUINO
// AJS - better place to move this to?
    void noInterrupts(){}
    void interrupts(){}
#endif

void afLib::loop(void) {
    if (isIdle() && (queueGet(&_request.messageType, &_request.requestId, &_request.attrId, &_request.valueLen,
                              &_request.p_value) == afSUCCESS)) {
        switch (_request.messageType) {
            case MSG_TYPE_GET:
                doGetAttribute(_request.requestId, _request.attrId);
                break;

            case MSG_TYPE_SET:
                doSetAttribute(_request.requestId, _request.attrId, _request.valueLen, _request.p_value);
                break;

            case MSG_TYPE_UPDATE:
                doUpdateAttribute(_request.requestId, _request.attrId, 0, _request.valueLen, _request.p_value);
                break;

            default:
                _theLog->println("loop: request type!");
        }
    }

    if (_request.p_value != NULL) {
        delete (_request.p_value);
        _request.p_value = NULL;
    }
    checkInterrupt();
}

void afLib::updateIntsPending(int amount) {
    noInterrupts();
    _interrupts_pending += amount;
    interrupts();
}

void afLib::sendCommand(void) {
    noInterrupts();
    if (_interrupts_pending == 0 && _state == STATE_IDLE) {
        _interrupts_pending++;
    }
    interrupts();
}

int afLib::getAttribute(const uint16_t attrId) {
    _requestId++;
    uint8_t dummy; // This value isn't actually used.
    return queuePut(MSG_TYPE_GET, _requestId++, attrId, 0, &dummy);
}

/**
 * The many moods of setAttribute
 */
int afLib::setAttributeBool(const uint16_t attrId, const bool value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, sizeof(value),
                    (uint8_t *)&value);
}

int afLib::setAttribute8(const uint16_t attrId, const int8_t value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, sizeof(value),
                    (uint8_t *)&value);
}

int afLib::setAttribute16(const uint16_t attrId, const int16_t value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, sizeof(value),
                    (uint8_t *) &value);
}

int afLib::setAttribute32(const uint16_t attrId, const int32_t value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, sizeof(value),
                    (uint8_t *) &value);
}

int afLib::setAttribute64(const uint16_t attrId, const int64_t value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, sizeof(value),
                    (uint8_t *) &value);
}

int afLib::setAttribute(const uint16_t attrId, const String &value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, value.length(),
                    (uint8_t *) value.c_str());
}

int afLib::setAttribute(const uint16_t attrId, const uint16_t valueLen, const char *value) {
    if (valueLen > MAX_ATTRIBUTE_SIZE) {
        return afERROR_INVALID_PARAM;
    }

    if (value == NULL) {
        return afERROR_INVALID_PARAM;
    }

    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, valueLen,
                    (const uint8_t *) value);
}

int afLib::setAttribute(const uint16_t attrId, const uint16_t valueLen, const uint8_t *value) {
    if (valueLen > MAX_ATTRIBUTE_SIZE) {
        return afERROR_INVALID_PARAM;
    }

    if (value == NULL) {
        return afERROR_INVALID_PARAM;
    }

    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, valueLen, value);
}

int afLib::setAttributeComplete(uint8_t requestId, const uint16_t attrId, const uint16_t valueLen, const uint8_t *value) {
    if (valueLen > MAX_ATTRIBUTE_SIZE) {
        return afERROR_INVALID_PARAM;
    }

    if (value == NULL) {
        return afERROR_INVALID_PARAM;
    }

    return queuePut(MSG_TYPE_UPDATE, requestId, attrId, valueLen, value);
}

int afLib::doGetAttribute(uint8_t requestId, uint16_t attrId) {
    if (_interrupts_pending > 0 || _writeCmd != NULL) {
        return afERROR_BUSY;
    }

    _writeCmd = new Command(_theLog,requestId, MSG_TYPE_GET, attrId);
    if (!_writeCmd->isValid()) {
        _theLog->print("getAttribute invalid command:");
        _writeCmd->dumpBytes();
        _writeCmd->dump();
        delete (_writeCmd);
        _writeCmd = NULL;
        return afERROR_INVALID_COMMAND;
    }

    _outstandingSetGetAttrId = attrId;

    // Start the transmission.
    sendCommand();

    return afSUCCESS;
}

int afLib::doSetAttribute(uint8_t requestId, uint16_t attrId, uint16_t valueLen, uint8_t *value) {
    if (_interrupts_pending > 0 || _writeCmd != NULL) {
        return afERROR_BUSY;
    }

    _writeCmd = new Command(_theLog,requestId, MSG_TYPE_SET, attrId, valueLen, value);
    if (!_writeCmd->isValid()) {
        _theLog->print("setAttributeComplete invalid command:");
        _writeCmd->dumpBytes();
        _writeCmd->dump();
        delete (_writeCmd);
        _writeCmd = NULL;
        return afERROR_INVALID_COMMAND;
    }

    _outstandingSetGetAttrId = attrId;

    // Start the transmission.
    sendCommand();

    return afSUCCESS;
}

int afLib::doUpdateAttribute(uint8_t requestId, uint16_t attrId, uint8_t status, uint16_t valueLen, uint8_t *value) {
    if (_interrupts_pending > 0 || _writeCmd != NULL) {
        return afERROR_BUSY;
    }

    _writeCmd = new Command(_theLog,requestId, MSG_TYPE_UPDATE, attrId, status, 3 /* MCU Set it */, valueLen, value);
    if (!_writeCmd->isValid()) {
        _theLog->print("updateAttribute invalid command:");
        _writeCmd->dumpBytes();
        _writeCmd->dump();
        delete (_writeCmd);
        return afERROR_INVALID_COMMAND;
    }

    // Start the transmission.
    sendCommand();

    return afSUCCESS;
}

int afLib::parseCommand(const char *cmd) {
    if (_interrupts_pending > 0 || _writeCmd != NULL) {
        _theLog->print("Busy: ");
        _theLog->print(_interrupts_pending);
        _theLog->print(", ");
        _theLog->println(_writeCmd != NULL);
        return afERROR_BUSY;
    }

    int reqId = _requestId++;
    _writeCmd = new Command(_theLog,reqId, cmd);
    if (!_writeCmd->isValid()) {
        _theLog->print("BAD: ");
        _theLog->println(cmd);
        _writeCmd->dumpBytes();
        _writeCmd->dump();
        delete (_writeCmd);
        _writeCmd = NULL;
        return afERROR_INVALID_COMMAND;
    }

    // Start the transmission.
    sendCommand();

    return afSUCCESS;
}

void afLib::checkInterrupt(void) {
    int result;

    if (_interrupts_pending > 0) {
        //_theLog->print("_interrupts_pending: "); _theLog->println(_interrupts_pending);

        switch (_state) {
            case STATE_IDLE:
                if (_writeCmd != NULL) {
                    // Include 2 bytes for length
                    _bytesToSend = _writeCmd->getSize() + 2;
                } else {
                    _bytesToSend = 0;
                }
                _state = STATE_STATUS_SYNC;
                printState(_state);
                return;

            case STATE_STATUS_SYNC:
                _txStatus->setAck(false);
                _txStatus->setBytesToSend(_bytesToSend);
                _txStatus->setBytesToRecv(0);

                result = exchangeStatus(_txStatus, _rxStatus);

                if (result == 0 && _rxStatus->isValid() && inSync(_txStatus, _rxStatus)) {
                    _state = STATE_STATUS_ACK;
                    if (_txStatus->getBytesToSend() == 0 && _rxStatus->getBytesToRecv() > 0) {
                        _bytesToRecv = _rxStatus->getBytesToRecv();
                    }
                } else {
                    // Try resending the preamble
                    _state = STATE_STATUS_SYNC;
                    _theLog->println("Collision");
//          _txStatus->dumpBytes();
//          _rxStatus->dumpBytes();
                }
                printState(_state);
                break;

            case STATE_STATUS_ACK:
                _txStatus->setAck(true);
                _txStatus->setBytesToRecv(_rxStatus->getBytesToRecv());
                _bytesToRecv = _rxStatus->getBytesToRecv();
                result = writeStatus(_txStatus);
                if (result != 0) {
                    _state = STATE_STATUS_SYNC;
                    printState(_state);
                    break;
                }
                if (_bytesToSend > 0) {
                    _writeBufferLen = (uint16_t) _writeCmd->getSize();
                    _writeBuffer = new uint8_t[_bytesToSend];
                    memcpy(_writeBuffer, (uint8_t * ) & _writeBufferLen, 2);
                    _writeCmd->getBytes(&_writeBuffer[2]);
                    _state = STATE_SEND_BYTES;
                } else if (_bytesToRecv > 0) {
                    _state = STATE_RECV_BYTES;
                } else {
                    _state = STATE_CMD_COMPLETE;
                }
                printState(_state);
                break;

            case STATE_SEND_BYTES:
//        _theLog->print("send bytes: "); _theLog->println(_bytesToSend);		
                sendBytes();

                if (_bytesToSend == 0) {
                    _writeBufferLen = 0;
                    delete (_writeBuffer);
                    _writeBuffer = NULL;
                    _state = STATE_CMD_COMPLETE;
                    printState(_state);
                }
                break;

            case STATE_RECV_BYTES:
//        _theLog->print("receive bytes: "); _theLog->println(_bytesToRecv);
                recvBytes();
                if (_bytesToRecv == 0) {
                    _state = STATE_CMD_COMPLETE;
                    printState(_state);
                    _readCmd = new Command(_theLog,_readBufferLen, &_readBuffer[2]);
                    delete (_readBuffer);
                    _readBuffer = NULL;
                }
                break;

            case STATE_CMD_COMPLETE:
                _state = STATE_IDLE;
                printState(_state);
                if (_readCmd != NULL) {
                    byte *val = new byte[_readCmd->getValueLen()];
                    _readCmd->getValue(val);

                    switch (_readCmd->getCommand()) {
                        case MSG_TYPE_SET:
                            _onAttrSet(_readCmd->getReqId(), _readCmd->getAttrId(), _readCmd->getValueLen(), val);
                            break;

                        case MSG_TYPE_UPDATE:
                            if (_readCmd->getAttrId() == _outstandingSetGetAttrId) {
                                _outstandingSetGetAttrId = 0;
                            }
                            _onAttrSetComplete(_readCmd->getReqId(), _readCmd->getAttrId(), _readCmd->getValueLen(), val);
                            break;

                        default:
                            break;
                    }
                    delete (val);
                    delete (_readCmd);
                    _readCmdOffset = 0;
                    _readCmd = NULL;
                }

                if (_writeCmd != NULL) {
                    // Fake a callback here for MCU attributes as we don't get one from the module.
                    if (_writeCmd->getCommand() == MSG_TYPE_UPDATE && IS_MCU_ATTR(_writeCmd->getAttrId())) {
                        _onAttrSetComplete(_writeCmd->getReqId(), _writeCmd->getAttrId(), _writeCmd->getValueLen(), _writeCmd->getValueP());
                    }
                    delete (_writeCmd);
                    _writeCmdOffset = 0;
                    _writeCmd = NULL;
                }
                break;
        }

        updateIntsPending(-1);
    }
}

/*
void afLib::beginSPI() {
    SPI.beginTransaction(_spiSettings);
    digitalWrite(_chipSelect, LOW);
    delayMicroseconds(8);
}

void afLib::endSPI() {
    //delayMicroseconds(1);
    digitalWrite(_chipSelect, HIGH);
    SPI.endTransaction();
}
*/
int afLib::exchangeStatus(StatusCommand *tx, StatusCommand *rx) {
    int result = 0;
    uint16_t len = tx->getSize();
    int bytes[len];
    char rbytes[len+1];
    int index = 0;
    tx->getBytes(bytes);

    _theSPI->beginSPI();

    for (int i=0;i<len;i++)
    {
      rbytes[i]=bytes[i];
    }
    rbytes[len]=tx->getChecksum();
    _theSPI->transfer(rbytes,len+1);

    byte cmd = bytes[index++];
    if (cmd != 0x30 && cmd != 0x31) {
        _theLog->print("exchangeStatus bad cmd: ");
        _theLog->println(cmd, HEX);
        result = -afERROR_INVALID_COMMAND;
    }

    rx->setBytesToSend(rbytes[index + 0] | (rbytes[index + 1] << 8));
    rx->setBytesToRecv(rbytes[index + 2] | (rbytes[index + 3] << 8));
    rx->setChecksum(rbytes[index+4]);

    _theSPI->endSPI();

    return result;
}

bool afLib::inSync(StatusCommand *tx, StatusCommand *rx) {
    return (tx->getBytesToSend() == 0 && rx->getBytesToRecv() == 0) ||
           (tx->getBytesToSend() > 0 && rx->getBytesToRecv() == 0) ||
           (tx->getBytesToSend() == 0 && rx->getBytesToRecv() > 0);
}

int afLib::writeStatus(StatusCommand *c) {
    int result = 0;
    uint16_t len = c->getSize();
    int bytes[len];
    char rbytes[len+1];
    int index = 0;
    c->getBytes(bytes);

    _theSPI->beginSPI();

    for (int i=0;i<len;i++)
    {
      rbytes[i]=bytes[i];
    }
    rbytes[len]=c->getChecksum();
    _theSPI->transfer(rbytes,len+1);

    byte cmd = rbytes[index++];
    if (cmd != 0x30 && cmd != 0x31) {
        _theLog->print("writeStatus bad cmd: ");
        _theLog->println(cmd, HEX);
        result = afERROR_INVALID_COMMAND;
    }


    _theSPI->endSPI();

//  c->dump();
//  c->dumpBytes();

    return result;
}

void afLib::sendBytes() {
    uint16_t len = _bytesToSend > SPI_FRAME_LEN ? SPI_FRAME_LEN : _bytesToSend;
    uint8_t bytes[SPI_FRAME_LEN];
    memset(bytes, 0xff, sizeof(bytes));

    memcpy(bytes, &_writeBuffer[_writeCmdOffset], len);

    _theSPI->beginSPI();

    _theSPI->transfer((char *)bytes,len);

    _theSPI->endSPI();

//  dumpBytes("Sending:", len, bytes);

    _writeCmdOffset += len;
    _bytesToSend -= len;
}

void afLib::recvBytes() {
    uint16_t len = _bytesToRecv > SPI_FRAME_LEN ? SPI_FRAME_LEN : _bytesToRecv;

    if (_readCmdOffset == 0) {
        _readBufferLen = _bytesToRecv;
        _readBuffer = new uint8_t[_readBufferLen];
    }

    _theSPI->beginSPI();


    char * start =(char*)_readBuffer + _readCmdOffset;
    _theSPI->transfer(start,len);

    _theSPI->endSPI();

//  dumpBytes("Receiving:", len, _readBuffer);

    _readCmdOffset += len;
    _bytesToRecv -= len;
}

void afLib::onIdle() {
}

bool afLib::isIdle() {
    return _interrupts_pending == 0 && _state == STATE_IDLE && _outstandingSetGetAttrId == 0;
}

void afLib::dumpBytes(char *label, int len, uint8_t *bytes) {
    _theLog->println(label);
    for (int i = 0; i < len; i++) {
        if (i > 0) {
            _theLog->print(", ");
        }
        uint8_t b = bytes[i] & 0xff;

        if (b < 0x10) {
            _theLog->print("0x0");
            _theLog->print(b, HEX);
        } else {
            _theLog->print("0x");
            _theLog->print(b, HEX);
        }
    }
    _theLog->println("");
}

void afLib::mcuISR() {
//  _theLog->println("mcu");
    _interrupts_pending++;
}

void afLib::printState(int state) {
    return;
    switch (state) {
        case STATE_IDLE:
            _theLog->println("STATE_IDLE");
            break;
        case STATE_STATUS_SYNC:
            _theLog->println("STATE_STATUS_SYNC");
            break;
        case STATE_STATUS_ACK:
            _theLog->println("STATE_STATUS_ACK");
            break;
        case STATE_SEND_BYTES:
            _theLog->println("STATE_SEND_BYTES");
            break;
        case STATE_RECV_BYTES:
            _theLog->println("STATE_RECV_BYTES");
            break;
        case STATE_CMD_COMPLETE:
            _theLog->println("STATE_CMD_COMPLETE");
            break;
        default:
            _theLog->println("Unknown State!");
            break;
    }
}

void afLib::queueInit() {
    for (int i = 0; i < REQUEST_QUEUE_SIZE; i++) {
        _requestQueue[i].p_value = NULL;
    }
}

int afLib::queuePut(uint8_t messageType, uint8_t requestId, const uint16_t attributeId, uint16_t valueLen,
                    const uint8_t *value) {
    for (int i = 0; i < REQUEST_QUEUE_SIZE; i++) {
        if (_requestQueue[i].p_value == NULL) {
            _requestQueue[i].messageType = messageType;
            _requestQueue[i].attrId = attributeId;
            _requestQueue[i].requestId = requestId;
            _requestQueue[i].valueLen = valueLen;
            _requestQueue[i].p_value = new uint8_t[valueLen];
            memcpy(_requestQueue[i].p_value, value, valueLen);
            return afSUCCESS;
        }
    }

    return afERROR_QUEUE_OVERFLOW;
}

int afLib::queueGet(uint8_t *messageType, uint8_t *requestId, uint16_t *attributeId, uint16_t *valueLen,
                    uint8_t **value) {
    for (int i = 0; i < REQUEST_QUEUE_SIZE; i++) {
        if (_requestQueue[i].p_value != NULL) {
            *messageType = _requestQueue[i].messageType;
            *attributeId = _requestQueue[i].attrId;
            *requestId = _requestQueue[i].requestId;
            *valueLen = _requestQueue[i].valueLen;
            *value = new uint8_t[*valueLen];
            memcpy(*value, _requestQueue[i].p_value, *valueLen);
            delete (_requestQueue[i].p_value);
            _requestQueue[i].p_value = NULL;
            return afSUCCESS;
        }
    }

    return afERROR_QUEUE_UNDERFLOW;
}
