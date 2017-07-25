/**
 * Copyright 2016 Afero, Inc.
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
#ifndef AFLIB_AFTRANSPORT_H
#define AFLIB_AFTRANSPORT_H

#include "StatusCommand.h"

class afTransport {
public:
    /*
     * checkForInterrupt
     *
     * For interfaces that don't use the interrupt pin to signal they want the state machine to run (like UART).
     */
    virtual void checkForInterrupt(volatile int *interrupts_pending, bool idle) = 0;

    /*
     * exchangeStatus
     *
     * Write a status message to the interface and read one back.
     */
    virtual int exchangeStatus(StatusCommand *tx, StatusCommand *rx) = 0;

    /*
     * writeStatus
     *
     * Write the specified status message to the interface.
     */
    virtual int writeStatus(StatusCommand *c) = 0;

    /*
     * sendBytes
     *
     * Write the specified bytes to the interface.
     */
    virtual void sendBytes(char *bytes, int len) = 0;

    /*
     * recvBytes
     *
     * Read the specified bytes from the interface.
     */
    virtual void recvBytes(char *bytes, int len) = 0;

    /*
     * sendBytesOffset
     *
     * Write bytes using an interface specific packet size.
     * It may take multiple calls to this method to write all of the bytes.
     *
     * @param bytes         buffer of bytes to be written.
     * @param bytesToSend   Pointer to count of total number of bytes to be written.
     *                      This value is decremented after each packet is written and will be 0 when all bytes have been sent.
     * @param offset        Pointer to offset into bytes buffer.
     *                      This value is incremented after each packet is written and will equal bytesToSend when all bytes have been sent.
     */
    virtual void sendBytesOffset(char *bytes, uint16_t *bytesToSend, uint16_t *offset) = 0;

    /*
     * recvBytesOffset
     *
     * Read bytes using an interface specific packet size.
     * It may take multiple calls to this method to read all of the bytes.
     *
     * @param bytes         Pointer to buffer of bytes to read into.
     *                      This buffer is allocated for all bytesToRecv before the first packet is read.
     *                      NOTE: IT IS THE CALLER'S RESPONSIBILITY TO FREE THIS BUFFER.
     * @param bytesToRecv   Pointer to count of total number of bytes to be read.
     *                      This value is decremented after each packet is read and will be 0 when all bytes have been read.
     * @param offset        Pointer to offset into bytes buffer.
     *                      This value is incremented after each packet is read and will equal bytesToRecv when all bytes have been read.
     */
    virtual void recvBytesOffset(char **bytes, uint16_t *bytesLen, uint16_t *bytesToRecv, uint16_t *offset) = 0;
};

#endif


