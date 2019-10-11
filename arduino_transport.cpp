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

#include "arduino_transport.h"
#include "arduino_spi.h"
#include "arduino_uart.h"

typedef enum {
    ARDUINO_TRANSPORT_SPI,
    ARDUINO_TRANSPORT_UART
} arduino_transport_t;

static arduino_transport_t s_transport_type;

af_transport_t* arduino_transport_create_spi(int chipSelect) {
    s_transport_type = ARDUINO_TRANSPORT_SPI;
    return arduino_spi_create(chipSelect, DEFAULT_SPI_FRAME_LEN);
}


af_transport_t* arduino_transport_create_spi(int chipSelect, uint16_t frame_length) {
    s_transport_type = ARDUINO_TRANSPORT_SPI;
    return arduino_spi_create(chipSelect, frame_length);
}

af_transport_t* arduino_transport_create_uart(uint8_t rxPin, uint8_t txPin, uint32_t baud_rate) {
    s_transport_type = ARDUINO_TRANSPORT_UART;
    return arduino_uart_create(rxPin, txPin, baud_rate);
}

void arduino_transport_destroy(af_transport_t *af_transport) {
    if (ARDUINO_TRANSPORT_SPI == s_transport_type) {
        arduino_spi_destroy(af_transport);
    } else {
        arduino_uart_destroy(af_transport);
    }
}

void af_transport_check_for_interrupt(af_transport_t *af_transport, volatile int *interrupts_pending, bool idle) {
    if (ARDUINO_TRANSPORT_SPI == s_transport_type) {
        af_transport_check_for_interrupt_spi(af_transport, interrupts_pending, idle);
    } else {
        af_transport_check_for_interrupt_uart(af_transport, interrupts_pending, idle);
    }
}

int af_transport_exchange_status(af_transport_t *af_transport, af_status_command_t *af_status_command_tx, af_status_command_t *af_status_command_rx) {
    if (ARDUINO_TRANSPORT_SPI == s_transport_type) {
        return af_transport_exchange_status_spi(af_transport, af_status_command_tx, af_status_command_rx);
    } else {
        return af_transport_exchange_status_uart(af_transport, af_status_command_tx, af_status_command_rx);
    }
}

int af_transport_write_status(af_transport_t *af_transport, af_status_command_t *af_status_command) {
    if (ARDUINO_TRANSPORT_SPI == s_transport_type) {
        return af_transport_write_status_spi(af_transport, af_status_command);
    } else {
        return af_transport_write_status_uart(af_transport, af_status_command);
    }
}

void af_transport_send_bytes_offset(af_transport_t *af_transport, uint8_t *bytes, uint16_t *bytes_to_send, uint16_t *offset) {
    if (ARDUINO_TRANSPORT_SPI == s_transport_type) {
        af_transport_send_bytes_offset_spi(af_transport, bytes, bytes_to_send, offset);
    } else {
        af_transport_send_bytes_offset_uart(af_transport, bytes, bytes_to_send, offset);
    }
}

int af_transport_recv_bytes_offset(af_transport_t *af_transport, uint8_t **bytes, uint16_t *bytes_len, uint16_t *bytes_to_recv, uint16_t *offset) {
    if (ARDUINO_TRANSPORT_SPI == s_transport_type) {
        af_transport_recv_bytes_offset_spi(af_transport, bytes, bytes_len, bytes_to_recv, offset);
    } else {
        return af_transport_recv_bytes_offset_uart(af_transport, bytes, bytes_len, bytes_to_recv, offset);
    }
    return AF_SUCCESS;
}
