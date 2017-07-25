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

/**
 * Define this to debug your selected transport (ie SPI or UART).
 * This will cause a println each time an interrupt or ready byte is received from the ASR.
 * You will also get states printed whenever a SYNC transaction is performed by the afLib.
 */
#define DEBUG_TRANSPORT     0

/**
 * These are required to be able to recognize the MCU trying to reboot the ASR by setting the command
 * attribute. We used local defines with the aflib prefix to make sure they are always defined and don't
 * clash with anything the app is using.
 */
#define AFLIB_SYSTEM_COMMAND_ATTR_ID    (65012)
#define AFLIB_SYSTEM_COMMAND_REBOOT     (1)

#define IS_MCU_ATTR(x) (x >= 0 && x < 1024)

static iafLib *_iaflib = NULL;

#define MAX_SYNC_RETRIES    10
static long lastSync = 0;
static int syncRetries = 0;

/**
* Required for the Linux version of afLib.
*/
#ifndef ARDUINO

#include <sys/time.h>

struct timeval start;

long millis() {
    gettimeofday(&start, NULL);
    return start.tv_sec;
}

/**
* These methods are required for the Arduino version of afLib.
* They are no-ops on linux.
*/
void noInterrupts() {}
void interrupts() {}

#endif

/**
 * create
 *
 * The public constructor for the afLib. This allows us to create the afLib object once and hold a reference to it.
 */
iafLib *iafLib::create(const int mcuInterrupt, isr isrWrapper,
                       AttrSetHandler attrSet, AttrNotifyHandler attrNotify, Stream *theLog , afTransport *theTransport)
{
    if (_iaflib == NULL) {
        _iaflib = new afLib( mcuInterrupt, isrWrapper, attrSet, attrNotify, theLog, theTransport);
    }

    return _iaflib;
}

/**
 * create
 *
 * The public constructor for the afLib. This allows us to create the afLib object once and hold a reference to it.
 */
iafLib *iafLib::create(AttrSetHandler attrSet, AttrNotifyHandler attrNotify, Stream *theLog, afTransport *theTransport)
{
    if (_iaflib == NULL) {
        _iaflib = new afLib(-1, NULL, attrSet, attrNotify, theLog, theTransport);
    }

    return _iaflib;
}

/**
 * afLib
 *
 * The private constructor for the afLib. This one actually initializes the afLib and prepares it for use.
 */
afLib::afLib(const int mcuInterrupt, isr isrWrapper,
             AttrSetHandler attrSet, AttrNotifyHandler attrNotify, Stream *theLog, afTransport *theTransport)
{
    queueInit();
    _theLog= theLog;
    _theTransport= theTransport;
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

    _attrSetHandler = attrSet;
    _attrNotifyHandler = attrNotify;


#ifdef ARDUINO
    if (mcuInterrupt != -1) {
        pinMode(mcuInterrupt, INPUT);
        attachInterrupt(mcuInterrupt, isrWrapper, FALLING);
    }
#endif

    _interrupts_pending = 0;
}

/**
 * loop
 *
 * This is how the afLib gets time to run its state machine. This method should be called periodically from the
 * loop() function of the Arduino sketch.
 * This function pulls pending attribute operations from the queue. It takes approximately 4 calls to loop() to
 * complete one attribute operation.
 */
void afLib::loop(void) {
    // For UART, we need to look for a magic character on the line as our interrupt.
    // We call this method to handle that. For other interfaces, the interrupt pin is used and this method does nothing.
    _theTransport->checkForInterrupt(&_interrupts_pending, isIdle());

    if (isIdle() && (queueGet(&_request.messageType, &_request.requestId, &_request.attrId, &_request.valueLen,
                              &_request.p_value, &_request.status, &_request.reason) == afSUCCESS)) {
        switch (_request.messageType) {
            case MSG_TYPE_GET:
                doGetAttribute(_request.requestId, _request.attrId);
                break;

            case MSG_TYPE_SET:
                doSetAttribute(_request.requestId, _request.attrId, _request.valueLen, _request.p_value);
                break;

            case MSG_TYPE_UPDATE:
                doUpdateAttribute(_request.requestId, _request.attrId, _request.valueLen, _request.p_value, _request.status, _request.reason);
                break;

            default:
                _theLog->println("loop: request type!");
        }
    }

    if (_request.p_value != NULL) {
        delete (_request.p_value);
        _request.p_value = NULL;
    }
    runStateMachine();
}

/**
 * updateIntsPending
 *
 * Interrupt-safe method for updating the interrupt count. This is called to increment and decrement the interrupt count
 * as interrupts are received and handled.
 */
void afLib::updateIntsPending(int amount) {
    noInterrupts();
    _interrupts_pending += amount;
    interrupts();
}

/**
 * sendCommand
 *
 * This increments the interrupt count to kick off the state machine in the next call to loop().
 */
void afLib::sendCommand(void) {
    noInterrupts();
    if (_interrupts_pending == 0 && _state == STATE_IDLE) {
        updateIntsPending(1);
    }
    interrupts();
}

/**
 * getAttribute
 *
 * The public getAttribute method. This method queues the operation and returns immediately. Applications must call
 * loop() for the operation to complete.
 */
int afLib::getAttribute(const uint16_t attrId) {
    _requestId++;
    uint8_t dummy; // This value isn't actually used.
    return queuePut(MSG_TYPE_GET, _requestId++, attrId, 0, &dummy, 0, 0);
}

/**
 * The many moods of setAttribute
 *
 * These are the public versions of the setAttribute method.
 * These methods queue the operation and return immediately. Applications must call loop() for the operation to complete.
 */
int afLib::setAttributeBool(const uint16_t attrId, const bool value) {
    _requestId++;
    uint8_t val = value ? 1 : 0;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, sizeof(val),
                    (uint8_t *)&val, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int afLib::setAttribute8(const uint16_t attrId, const int8_t value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, sizeof(value),
                    (uint8_t *)&value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int afLib::setAttribute16(const uint16_t attrId, const int16_t value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, sizeof(value),
                    (uint8_t *) &value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int afLib::setAttribute32(const uint16_t attrId, const int32_t value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, sizeof(value),
                    (uint8_t *) &value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int afLib::setAttribute64(const uint16_t attrId, const int64_t value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, sizeof(value),
                    (uint8_t *) &value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int afLib::setAttributeStr(const uint16_t attrId, const String &value) {
    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, value.length(),
                    (uint8_t *) value.c_str(), UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int afLib::setAttributeCStr(const uint16_t attrId, const uint16_t valueLen, const char *value) {
    if (valueLen > MAX_ATTRIBUTE_SIZE) {
        return afERROR_INVALID_PARAM;
    }

    if (value == NULL) {
        return afERROR_INVALID_PARAM;
    }

    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, valueLen,
                    (const uint8_t *) value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int afLib::setAttributeBytes(const uint16_t attrId, const uint16_t valueLen, const uint8_t *value) {
    if (valueLen > MAX_ATTRIBUTE_SIZE) {
        return afERROR_INVALID_PARAM;
    }

    if (value == NULL) {
        return afERROR_INVALID_PARAM;
    }

    _requestId++;
    return queuePut(IS_MCU_ATTR(attrId) ? MSG_TYPE_UPDATE : MSG_TYPE_SET, _requestId, attrId, valueLen, value, UPDATE_STATE_UPDATED, UPDATE_REASON_LOCAL_OR_MCU_UPDATE);
}

int afLib::setAttributeComplete(uint8_t requestId, const uint16_t attrId, const uint16_t valueLen, const uint8_t *value, uint8_t status, uint8_t reason) {
    if (valueLen > MAX_ATTRIBUTE_SIZE) {
        return afERROR_INVALID_PARAM;
    }

    if (value == NULL) {
        return afERROR_INVALID_PARAM;
    }

    return queuePut(MSG_TYPE_UPDATE, requestId, attrId, valueLen, value, status, reason);
}

/**
 * doGetAttribute
 *
 * The private version of getAttribute. This version actually calls sendCommand() to kick off the state machine and
 * execute the operation.
 */
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

/**
 * doSetAttribute
 *
 * The private version of setAttribute. This version actually calls sendCommand() to kick off the state machine and
 * execute the operation.
 */
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

    /**
     * Recognize when the MCU is trying to reboot the ASR. When this is the case, the ASR will reboot before
     * the SPI transaction completes and the _outstandingSetGetAttrId will be left set. Instead, just don't
     * set it for this case.
     */
    if (attrId != AFLIB_SYSTEM_COMMAND_ATTR_ID || *value != AFLIB_SYSTEM_COMMAND_REBOOT) {
        _outstandingSetGetAttrId = attrId;
    }

    // Start the transmission.
    sendCommand();

    return afSUCCESS;
}

/**
 * doUpdateAttribute
 *
 * setAttribute calls on MCU attributes turn into updateAttribute calls. See documentation on the SPI protocol for
 * more information. This method calls sendCommand() to kick off the state machine and execute the operation.
 */
int afLib::doUpdateAttribute(uint8_t requestId, uint16_t attrId, uint16_t valueLen, uint8_t *value, uint8_t status, uint8_t reason) {
    if (_interrupts_pending > 0 || _writeCmd != NULL) {
        return afERROR_BUSY;
    }

    _writeCmd = new Command(_theLog, requestId, MSG_TYPE_UPDATE, attrId, status, reason, valueLen, value);
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

/**
 * parseCommand
 *
 * A debug method for parsing a string into a command. This is not required for library operation and is only supplied
 * as an example of how to execute attribute operations from a command line interface.
 */
#ifdef ATTRIBUTE_CLI
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
#endif

/**
 * runStateMachine
 *
 * The state machine for afLib. This state machine is responsible for implementing the KSP SPI protocol and executing
 * attribute operations.
 * This method is run:
 *      1. In response to receiving an interrupt from the ASR-1.
 *      2. When an attribute operation is pulled out of the queue and executed.
 */
void afLib::runStateMachine(void) {
    if (_interrupts_pending > 0) {
        //_theLog->print("_interrupts_pending: "); _theLog->println(_interrupts_pending);

        switch (_state) {
            case STATE_IDLE:
                onStateIdle();
                return;

            case STATE_STATUS_SYNC:
                onStateSync();
                break;

            case STATE_STATUS_ACK:
                onStateAck();
                break;

            case STATE_SEND_BYTES:
                onStateSendBytes();
                break;

            case STATE_RECV_BYTES:
                onStateRecvBytes();
                break;

            case STATE_CMD_COMPLETE:
                onStateCmdComplete();
                break;
        }

        updateIntsPending(-1);
    } else {
        if (syncRetries > 0 && syncRetries < MAX_SYNC_RETRIES && millis() - lastSync > 1000) {
            _theLog->println("Sync Retry");
            updateIntsPending(1);
        } else if (syncRetries >= MAX_SYNC_RETRIES) {
            _theLog->println("No response from ASR - does profile have MCU enabled?");
            syncRetries = 0;
            _state = STATE_IDLE;
        }
    }
}

/**
 * onStateIdle
 *
 * If there is a command to be written, update the bytes to send. Otherwise we're sending a zero-sync message.
 * Either way advance the state to send a sync message.
 */
void afLib::onStateIdle(void) {
    if (_writeCmd != NULL) {
        // Include 2 bytes for length
        _bytesToSend = _writeCmd->getSize() + 2;
    } else {
        _bytesToSend = 0;
    }
    _state = STATE_STATUS_SYNC;
    printState(_state);
}

/**
 * onStateSync
 *
 * Write a sync message over SPI to let the ASR-1 know that we want to send some data.
 * Check for a "collision" which occurs if the ASR-1 is trying to send us data at the same time.
 */
void afLib::onStateSync(void) {
    int result;

    _txStatus->setAck(false);
    _txStatus->setBytesToSend(_bytesToSend);
    _txStatus->setBytesToRecv(0);

    result = _theTransport->exchangeStatus(_txStatus, _rxStatus);

    if (result == afSUCCESS && _rxStatus->isValid() && inSync(_txStatus, _rxStatus)) {
        syncRetries = 0;   // Flag that sync completed.
        _state = STATE_STATUS_ACK;
        if (_txStatus->getBytesToSend() == 0 && _rxStatus->getBytesToRecv() > 0) {
            _bytesToRecv = _rxStatus->getBytesToRecv();
        }
    } else {
        // Try resending the preamble
        _state = STATE_STATUS_SYNC;
        lastSync = millis();
        syncRetries++;
        //_txStatus->dumpBytes();
        //_rxStatus->dumpBytes();
    }
    printState(_state);
}

/**
 * onStateAck
 *
 * Acknowledge the previous sync message and advance the state.
 * If there are bytes to send, advance to send bytes state.
 * If there are bytes to receive, advance to receive bytes state.
 * Otherwise it was a zero-sync so advance to command complete.
 */
void afLib::onStateAck(void) {
    int result;

    _txStatus->setAck(true);
    _txStatus->setBytesToRecv(_rxStatus->getBytesToRecv());
    _bytesToRecv = _rxStatus->getBytesToRecv();
    result = _theTransport->writeStatus(_txStatus);
    if (result != afSUCCESS) {
        _state = STATE_STATUS_SYNC;
        printState(_state);
        return;
    }
    if (_bytesToSend > 0) {
        _writeBufferLen = (uint16_t) _writeCmd->getSize();
        _writeBuffer = new uint8_t[_bytesToSend];
        memcpy(_writeBuffer, &_writeBufferLen, 2);
        _writeCmd->getBytes(&_writeBuffer[2]);
        _state = STATE_SEND_BYTES;
    } else if (_bytesToRecv > 0) {
        _state = STATE_RECV_BYTES;
    } else {
        _state = STATE_CMD_COMPLETE;
    }
    printState(_state);
}

/**
 * onStateSendBytes
 *
 * Send the required number of bytes to the ASR-1 and then advance to command complete.
 */
void afLib::onStateSendBytes(void) {
    //_theLog->print("send bytes: "); _theLog->println(_bytesToSend);
    _theTransport->sendBytesOffset((char *)_writeBuffer, &_bytesToSend, &_writeCmdOffset);

    if (_bytesToSend == 0) {
        _writeBufferLen = 0;
        delete (_writeBuffer);
        _writeBuffer = NULL;
        _state = STATE_CMD_COMPLETE;
        printState(_state);
    }
}

/**
 * onStateRecvBytes
 *
 * Receive the required number of bytes from the ASR-1 and then advance to command complete.
 */
void afLib::onStateRecvBytes(void) {
    _theTransport->recvBytesOffset((char **)&_readBuffer, &_readBufferLen, &_bytesToRecv, &_readCmdOffset);
    if (_bytesToRecv == 0) {
        _state = STATE_CMD_COMPLETE;
        printState(_state);
        _readCmd = new Command(_theLog, _readBufferLen, &_readBuffer[2]);
    	//_readCmd->dumpBytes();
        delete (_readBuffer);
        _readBuffer = NULL;
    }
}

/**
 * onStateCmdComplete
 *
 * Call the appropriate sketch callback to report the result of the command.
 * Clear the command object and go back to waiting for the next interrupt or command.
 */
void afLib::onStateCmdComplete(void) {
    int result;

    _state = STATE_IDLE;
    printState(_state);
    if (_readCmd != NULL) {
        byte *val = new byte[_readCmd->getValueLen()];
        _readCmd->getValue(val);

        uint8_t state;
        uint8_t reason;

        switch (_readCmd->getCommand()) {
            case MSG_TYPE_SET:
                if (_attrSetHandler(_readCmd->getReqId(), _readCmd->getAttrId(), _readCmd->getValueLen(), val)) {
                    state = UPDATE_STATE_UPDATED;
                    reason = UPDATE_REASON_SERVICE_SET;
                } else {
                    state = UPDATE_STATE_FAILED;
                    reason = UPDATE_REASON_INTERNAL_SET_FAIL;
                }
                result = setAttributeComplete(_readCmd->getReqId(), _readCmd->getAttrId(), _readCmd->getValueLen(), val, state, reason);

                if (result != afSUCCESS) {
                	_theLog->print("Can't reply to SET! This is FATAL!");
                }

                break;

            case MSG_TYPE_UPDATE:
                if (_readCmd->getAttrId() == _outstandingSetGetAttrId) {
                    _outstandingSetGetAttrId = 0;
                }
                _attrNotifyHandler(_readCmd->getReqId(), _readCmd->getAttrId(), _readCmd->getValueLen(), val);
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
            _attrNotifyHandler(_writeCmd->getReqId(), _writeCmd->getAttrId(), _writeCmd->getValueLen(), _writeCmd->getValueP());
        }
        delete (_writeCmd);
        _writeCmdOffset = 0;
        _writeCmd = NULL;
    }
}

/**
 * inSync
 *
 * Check to make sure the Arduino and the ASR-1 aren't trying to send data at the same time.
 * Return true only if there is no collision.
 */
bool afLib::inSync(StatusCommand *tx, StatusCommand *rx) {
    return (tx->getBytesToSend() == 0 && rx->getBytesToRecv() == 0) ||
           (tx->getBytesToSend() > 0 && rx->getBytesToRecv() == 0) ||
           (tx->getBytesToSend() == 0 && rx->getBytesToRecv() > 0);
}

/**
 * isIdle
 *
 * Provide a way for the sketch to know if we're idle. Returns true if there are no attribute operations in progress.
 */
bool afLib::isIdle() {
    return _interrupts_pending == 0 && _state == STATE_IDLE && _outstandingSetGetAttrId == 0;
}

/**
 * These methods are required to disable/enable interrupts for the Linux version of afLib.
 * They are no-ops on Arduino.
 */
#ifndef ARDUINO
void disableInterrupts(){}
void enableInterrupts(){}
#endif

void afLib::mcuISR() {
#if (defined(DEBUG_TRANSPORT) && DEBUG_TRANSPORT > 0)
    _theLog->println("mcuISR");
#endif
    updateIntsPending(1);
}

/****************************************************************************
 *                              Queue Methods                               *
 ****************************************************************************/
/**
 * queueInit
 *
 * Create a small queue to prevent flooding the ASR-1 with attribute operations.
 * The initial size is small to allow running on small boards like UNO.
 * Size can be increased on larger boards.
 */
void afLib::queueInit() {
    for (int i = 0; i < REQUEST_QUEUE_SIZE; i++) {
        _requestQueue[i].p_value = NULL;
    }
}

/**
 * queuePut
 *
 * Add an item to the end of the queue. Return an error if we're out of space in the queue.
 */
int afLib::queuePut(uint8_t messageType, uint8_t requestId, const uint16_t attributeId, uint16_t valueLen,
                    const uint8_t *value, const uint8_t status, const uint8_t reason) {
    for (int i = 0; i < REQUEST_QUEUE_SIZE; i++) {
        if (_requestQueue[i].p_value == NULL) {
            _requestQueue[i].messageType = messageType;
            _requestQueue[i].attrId = attributeId;
            _requestQueue[i].requestId = requestId;
            _requestQueue[i].valueLen = valueLen;
            _requestQueue[i].p_value = new uint8_t[valueLen];
            memcpy(_requestQueue[i].p_value, value, valueLen);
            _requestQueue[i].status = status;
            _requestQueue[i].reason = reason;
            return afSUCCESS;
        }
    }

    return afERROR_QUEUE_OVERFLOW;
}

/**
 * queueGet
 *
 * Pull and return the oldest item from the queue. Return an error if the queue is empty.
 */
int afLib::queueGet(uint8_t *messageType, uint8_t *requestId, uint16_t *attributeId, uint16_t *valueLen,
                    uint8_t **value, uint8_t *status, uint8_t *reason) {
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
            *status = _requestQueue[i].status;
            *reason = _requestQueue[i].reason;
            return afSUCCESS;
        }
    }

    return afERROR_QUEUE_UNDERFLOW;
}

/****************************************************************************
 *                              Debug Methods                               *
 ****************************************************************************/
/**
 * dumpBytes
 *
 * Dump a byte buffer to the debug log.
 */
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

/**
 * printState
 *
 * Print the current state of the afLib state machine. For debugging, just remove the return statement.
 */
void afLib::printState(int state) {
#if (defined(DEBUG_TRANSPORT) && DEBUG_TRANSPORT > 0)
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
#endif
}
