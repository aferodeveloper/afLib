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

#ifndef AFLIB_H__
#define AFLIB_H__

#include "iafLib.h"
#include "SPI.h"
#include "Command.h"
#include "StatusCommand.h"
#include "afSPI.h"

#define STATE_IDLE                          0
#define STATE_STATUS_SYNC                   1
#define STATE_STATUS_ACK                    3
#define STATE_SEND_BYTES                    4
#define STATE_RECV_BYTES                    5
#define STATE_CMD_COMPLETE                  6

#define REQUEST_QUEUE_SIZE                  10

typedef struct {
    uint8_t     messageType;
    uint16_t    attrId;
    uint8_t     requestId;
    uint16_t    valueLen;
    uint8_t     *p_value;
    uint8_t     status;
    uint8_t     reason;
} request_t;

class afLib : public iafLib {
public:
    afLib(AttrSetHandler attrSet, AttrNotifyHandler attrNotify, Stream *serial, afTransport *theTransport);
    afLib(const int mcuInterrupt, isr isrWrapper, AttrSetHandler attrSet, AttrNotifyHandler attrNotify, Stream *serial, afTransport *theTransport);

    virtual void loop(void);

    virtual int getAttribute(const uint16_t attrId);

    virtual int setAttributeBool(const uint16_t attrId, const bool value);

    virtual int setAttribute8(const uint16_t attrId, const int8_t value);

    virtual int setAttribute16(const uint16_t attrId, const int16_t value);

    virtual int setAttribute32(const uint16_t attrId, const int32_t value);

    virtual int setAttribute64(const uint16_t attrId, const int64_t value);

    virtual int setAttributeStr(const uint16_t attrId, const String &value);

    virtual int setAttributeCStr(const uint16_t attrId, const uint16_t valueLen, const char *value);

    virtual int setAttributeBytes(const uint16_t attrId, const uint16_t valueLen, const uint8_t *value);

    virtual bool isIdle();

    virtual void mcuISR();

private:
    Stream *_theLog;
    afTransport *_theTransport;

    //SPISettings _spiSettings;
    volatile int _interrupts_pending;
    int _state;
    uint16_t _bytesToSend;
    uint16_t _bytesToRecv;
    uint8_t _requestId;
    uint16_t _outstandingSetGetAttrId;

    // Application Callbacks.
    AttrSetHandler _attrSetHandler;
    AttrNotifyHandler _attrNotifyHandler;

    Command *_writeCmd;
    uint16_t _writeBufferLen;
    uint8_t *_writeBuffer;

    Command *_readCmd;
    uint16_t _readBufferLen;
    uint8_t *_readBuffer;

    uint16_t _writeCmdOffset;
    uint16_t _readCmdOffset;

    StatusCommand *_txStatus;
    StatusCommand *_rxStatus;

    request_t _request;

#ifdef ATTRIBUTE_CLI
    int parseCommand(const char *cmd);
#endif

    void sendCommand(void);

    void runStateMachine(void);

    void printState(int state);

    bool inSync(StatusCommand *tx, StatusCommand *rx);

    void sendBytesSPI(char *bytes, int len);
    void sendBytesUART(char *bytes, int len);
    void sendBytes();

    void recvBytesSPI(char *bytes, int len);
    void recvBytesUART(char *bytes, int len);
    void recvBytes();

    void dumpBytes(char *label, int len, uint8_t *bytes);

    void updateIntsPending(int amount);

    void queueInit(void);

    int queuePut(uint8_t messageType, uint8_t requestId, const uint16_t attributeId, uint16_t valueLen, const uint8_t *value, uint8_t status, uint8_t reason);

    int queueGet(uint8_t *messageType, uint8_t *requestId, uint16_t *attributeId, uint16_t *valueLen, uint8_t **value, uint8_t *status, uint8_t *reason);

    int doGetAttribute(uint8_t requestId, uint16_t attrId);

    int doSetAttribute(uint8_t requestId, uint16_t attrId, uint16_t valueLen, uint8_t *value);

    int doUpdateAttribute(uint8_t requestId, uint16_t attrId, uint16_t valueLen, uint8_t *value, uint8_t status, uint8_t reason);

    int setAttributeComplete(uint8_t requestId, const uint16_t attrId, const uint16_t valueLen, const uint8_t *value, uint8_t status, uint8_t reason);

    void onStateIdle(void);
    void onStateSync(void);
    void onStateAck(void);
    void onStateSendBytes(void);
    void onStateRecvBytes(void);
    void onStateCmdComplete(void);
};

#endif // AFLIB_H__
