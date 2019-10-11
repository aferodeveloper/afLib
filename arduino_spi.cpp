/**
 * Copyright 2018 Afero, Inc.
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

#include "arduino_spi.h"
#include "af_logger.h"
#include "af_lib.h"

#include <SPI.h>

class ArduinoSPI {
public:

    ArduinoSPI(int chipSelect, uint16_t frame_length);

    void checkForInterrupt(volatile int *interrupts_pending, bool idle);
    int exchangeStatus(af_status_command_t *tx, af_status_command_t *rx);
    int writeStatus(af_status_command_t *c);
    void sendBytes(uint8_t *bytes, int len);
    void recvBytes(uint8_t *bytes, int len);
    void sendBytesOffset(uint8_t *bytes, uint16_t *bytesToSend, uint16_t *offset);
    void recvBytesOffset(uint8_t **bytes, uint16_t *bytesLen, uint16_t *bytesToRecv, uint16_t *offset);

private:
    SPISettings _spiSettings;
    int _chipSelect;
    uint16_t _frameLength;

    void begin();
    void beginSPI(); /* settings are in this class */
    void endSPI();
    void transfer(uint8_t *bytes, int len);
};

struct af_transport_t {
    ArduinoSPI *arduinoSPI;
};

static af_lib_t* s_af_lib = NULL;

void isrWrapper() {
    if (s_af_lib) {
        af_lib_mcu_isr(s_af_lib);
    }
}

af_transport_t* arduino_spi_create(int chipSelect, uint16_t frame_length) {
    af_transport_t *result = new af_transport_t();
    result->arduinoSPI = new ArduinoSPI(chipSelect, frame_length);
    return result;
}

af_lib_error_t arduino_spi_setup_interrupts(af_lib_t* af_lib, int mcuInterrupt) {
    if (!af_lib) {
        return AF_ERROR_INVALID_PARAM;
    }
    s_af_lib = af_lib;
    pinMode(mcuInterrupt, INPUT);
    attachInterrupt(mcuInterrupt, isrWrapper, FALLING);

    return AF_SUCCESS;
}

void arduino_spi_destroy(af_transport_t *af_transport) {
    delete af_transport->arduinoSPI;
    delete af_transport;
}

void af_transport_check_for_interrupt_spi(af_transport_t *af_transport, volatile int *interrupts_pending, bool idle) {
    af_transport->arduinoSPI->checkForInterrupt(interrupts_pending, idle);
}

int af_transport_exchange_status_spi(af_transport_t *af_transport, af_status_command_t *af_status_command_tx, af_status_command_t *af_status_command_rx) {
    return af_transport->arduinoSPI->exchangeStatus(af_status_command_tx, af_status_command_rx);
}

int af_transport_write_status_spi(af_transport_t *af_transport, af_status_command_t *af_status_command) {
    return af_transport->arduinoSPI->writeStatus(af_status_command);
}

void af_transport_send_bytes_offset_spi(af_transport_t *af_transport, uint8_t *bytes, uint16_t *bytes_to_send, uint16_t *offset) {
    af_transport->arduinoSPI->sendBytesOffset(bytes, bytes_to_send, offset);
}

void af_transport_recv_bytes_offset_spi(af_transport_t *af_transport, uint8_t **bytes, uint16_t *bytes_len, uint16_t *bytes_to_recv, uint16_t *offset) {
    af_transport->arduinoSPI->recvBytesOffset(bytes, bytes_len, bytes_to_recv, offset);
}

ArduinoSPI::ArduinoSPI(int chipSelect, uint16_t frame_length)
{
    _chipSelect = chipSelect;
    _frameLength = frame_length;
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
void ArduinoSPI::transfer(uint8_t *bytes, int len)
{
    SPI.transfer(bytes, len);
}

void ArduinoSPI::checkForInterrupt(volatile int *interrupts_pending, bool idle)
{
    // Nothing required here.
}

int ArduinoSPI::exchangeStatus(af_status_command_t *tx, af_status_command_t *rx)
{
    int result = AF_SUCCESS;
    uint16_t len = af_status_command_get_size(tx);
    uint8_t bytes[len];
    uint8_t rbytes[len + 1];
    int index = 0;
    af_status_command_get_bytes(tx, bytes);

    beginSPI();

    for (int i=0; i < len; i++)
    {
        rbytes[i]=bytes[i];
    }
    rbytes[len]=af_status_command_get_checksum(tx);

    transfer(rbytes, len + 1);

    byte cmd = bytes[index++];
    if (cmd != SYNC_REQUEST && cmd != SYNC_ACK) {
        af_logger_print_buffer("exchangeStatus bad cmd: ");
        af_logger_println_formatted_value(cmd, AF_LOGGER_HEX);
        result = AF_ERROR_INVALID_COMMAND;
    }

    af_status_command_set_bytes_to_send(rx, (rbytes[index + 0] | (rbytes[index + 1] << 8)));
    af_status_command_set_bytes_to_recv(rx, (rbytes[index + 2] | (rbytes[index + 3] << 8)));
    af_status_command_set_checksum(rx, rbytes[index+4]);

    endSPI();

    return result;
}

int ArduinoSPI::writeStatus(af_status_command_t *c)
{
    int result = AF_SUCCESS;
    uint16_t len = af_status_command_get_size(c);
    uint8_t bytes[len];
    uint8_t rbytes[len+1];
    int index = 0;
    af_status_command_get_bytes(c, bytes);

    beginSPI();

    for (int i=0;i<len;i++)
    {
        rbytes[i]=bytes[i];
    }
    rbytes[len]=af_status_command_get_checksum(c);
    transfer(rbytes, len + 1);

    byte cmd = rbytes[index++];
    if (cmd != SYNC_REQUEST && cmd != SYNC_ACK) {
        af_logger_print_buffer("writeStatus bad cmd: ");
        af_logger_println_formatted_value(cmd, AF_LOGGER_HEX);
        result = AF_ERROR_INVALID_COMMAND;
    }

    endSPI();

    return result;
}

void ArduinoSPI::sendBytes(uint8_t *bytes, int len)
{
    beginSPI();

    transfer(bytes, len);

    endSPI();
}

void ArduinoSPI::recvBytes(uint8_t *bytes, int len)
{
    beginSPI();

    transfer(bytes, len);

    endSPI();
}

void ArduinoSPI::sendBytesOffset(uint8_t *bytes, uint16_t *bytesToSend, uint16_t *offset)
{
    uint16_t len = 0;

    len = *bytesToSend > _frameLength ? _frameLength : *bytesToSend;

    uint8_t buffer[len];
    memset(buffer, 0xff, sizeof(buffer));

    memcpy(buffer, &bytes[*offset], len);

    sendBytes(buffer, len);

    *offset += len;
    *bytesToSend -= len;
}

void ArduinoSPI::recvBytesOffset(uint8_t **bytes, uint16_t *bytesLen, uint16_t *bytesToRecv, uint16_t *offset)
{
    uint16_t len = 0;

    len = *bytesToRecv > _frameLength ? _frameLength : *bytesToRecv;

    if (*offset == 0) {
        *bytesLen = *bytesToRecv;
        *bytes = (uint8_t*)malloc(*bytesLen);
    }

    uint8_t *start = *bytes + *offset;

    recvBytes(start, len);

    *offset += len;
    *bytesToRecv -= len;
}
