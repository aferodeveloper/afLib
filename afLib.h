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

#include "Arduino.h"
#include "SPI.h"
#include "Command.h"
#include "StatusCommand.h"
#include "afErrors.h"

#define STATE_IDLE              0
#define STATE_STATUS_SYNC       1
#define STATE_STATUS_ACK        3
#define STATE_SEND_BYTES        4
#define STATE_RECV_BYTES        5
#define STATE_CMD_COMPLETE      6

#define SPI_FRAME_LEN			16

#define REQUEST_QUEUE_SIZE     	5
typedef struct {
	uint8_t messageType;
	uint16_t attrId;
	uint8_t requestId;
	uint16_t valueLen;
	uint8_t *p_value;
} request_t;

typedef void (* isr) ();
typedef void (* onAttributeSet) (uint8_t requestId, uint16_t attributeId, uint16_t valueLen, uint8_t *value);
typedef void (* onAttributeSetComplete) (uint8_t requestId, uint16_t attributeId, uint8_t *value);

class afLib {
  public:
    afLib(int chipSelect, int mcuInterrupt, isr isr_wrapper,
          onAttributeSet attrSet, onAttributeSetComplete attrUpdate);
    void loop(void);
    int getAttribute(uint16_t attrId);

	int setAttribute(uint16_t attrId, uint8_t value);
	int setAttribute(uint16_t attrId, uint16_t value);
	int setAttribute(uint16_t attrId, uint32_t value);
	int setAttribute(uint16_t attrId, uint64_t value);
	int setAttribute(uint16_t attrId, String value);
	int setAttribute(uint16_t attrId, uint16_t valueLen, const char *value);
	int setAttribute(uint16_t attrId, uint16_t valueLen, const uint8_t *value);

	int setAttributeComplete(uint8_t requestId, uint16_t attrId, uint16_t valueLen, const uint8_t *value);

	int parseCommand(const char *cmd);
	void sendCommand(void);
	bool isIdle();
	void mcuISR();

  private:
    SPISettings _spiSettings;
    int _chipSelect;
    volatile int _interrupts_pending;
	int _state;
	uint16_t _bytesToSend;
	uint16_t _bytesToRecv;
	int _requestId;

	// Application Callbacks.
	onAttributeSet _onAttrSet;
	onAttributeSetComplete _onAttrSetComplete;
	
	Command *_writeCmd;
	uint16_t _writeBufferLen;
	uint8_t *_writeBuffer;
	
	Command *_readCmd;
	uint16_t _readBufferLen;
	uint8_t *_readBuffer;
	
	int _writeCmdOffset;
	int _readCmdOffset;
	
	StatusCommand *_txStatus;
	StatusCommand *_rxStatus;

	request_t _request;
	request_t _requestQueue[REQUEST_QUEUE_SIZE];

	void checkInterrupt(void);
	void printState(int state);
	void beginSPI();
	void endSPI();
	int exchangeStatus(StatusCommand *tx, StatusCommand *rx);
	bool inSync(StatusCommand *tx, StatusCommand *rx);
	int writeStatus(StatusCommand *c);
	void sendBytes();
	void recvBytes();
	void onIdle();
	void dumpBytes(char *label, int len, uint8_t *bytes);
	void updateIntsPending(int amount);
	void queueInit(void);
	int queuePut(uint8_t messageType, uint8_t requestId, uint16_t attributeId, uint16_t valueLen, const uint8_t *value);
	int queueGet(uint8_t *messageType, uint8_t *requestId, uint16_t *attributeId, uint16_t *valueLen, uint8_t **value);
	int doGetAttribute(uint8_t requestId, uint16_t attrId);
	int doSetAttribute(uint8_t requestId, uint16_t attrId, uint16_t valueLen, uint8_t *value);
	int doUpdateAttribute(uint8_t requestId, uint16_t attrId, uint8_t status, uint16_t valueLen, uint8_t *value);

};

#endif // AFLIB_H__
