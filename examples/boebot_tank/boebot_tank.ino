/**
 * Copyright 2017-2018 Afero, Inc.
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

#include <Servo.h>

#include <SPI.h>
#include "af_lib.h"
#include "af_logger.h"
#include "arduino_logger.h"
#include "arduino_spi.h"
#include "af_module_commands.h"
#include "af_module_states.h"
#include "arduino_transport.h"

#include "profile/device-description.h"

#define INT_PIN                 2     // INT_PIN 2 for UNO
#define CS_PIN                  10    // Standard SPI chip select (aka SS)

#define STARTUP_DELAY_MILLIS    10000
#define STATE_INIT              0
#define STATE_RUNNING           1

#define SERVO_MIN_LIMIT         1300
#define SERVO_MAX_LIMIT         1700

af_lib_t *af_lib;
int state = STATE_INIT;
unsigned long start;

bool reboot_pending = false;            // Track information that a reboot is needed, e.g. if we received an OTA firmware update.

Servo leftWheel;
Servo rightWheel;

// Print output to console
boolean DEBUG = true;

// Function provides "reversed" value of the supplied speed.
// Determines the equivalent right-wheel speed, given a left-wheel speed
// Needed because identical servos are mounted in opposite directions on robot
int revSpeed(int forSpeed) {
    return SERVO_MAX_LIMIT - (forSpeed - SERVO_MIN_LIMIT);
}

void updateRightServoSpeed(int16_t newValue) {
    if (DEBUG) {
        af_logger_print_buffer("Updating RIGHT servo with speed: "); af_logger_println_value(newValue);
    }
   rightWheel.writeMicroseconds(newValue);
}

void updateLeftServoSpeed(int16_t newValue) {
    if (DEBUG) {
        af_logger_print_buffer("Updating LEFT servo speed: "); af_logger_println_value(newValue);
    }
   leftWheel.writeMicroseconds(newValue);
}

// attrSetHandler() is called when a client changes an attribute.
bool attrSetHandler(const uint8_t requestId,
                    const uint16_t attributeId,
                    const uint16_t valueLen,
                    const uint8_t *value) {

    int valAsInt = *((int16_t *)value);
    if (DEBUG) {
        af_logger_print_buffer("attrSetHandler() attrId: ");
        af_logger_print_value(attributeId);
        af_logger_print_buffer(" value: ");
        af_logger_println_value(valAsInt);
    }

    switch (attributeId) {
    case AF_SERVO_RIGHT:
        updateRightServoSpeed(revSpeed(valAsInt));
        break;

    case AF_SERVO_LEFT:
        updateLeftServoSpeed(valAsInt);
        break;
    }

    return true;
}

// attrNotifyHandler() is called when either an Afero attribute has been changed via setAttribute or in response to a getAttribute call.
void attrNotifyHandler(const uint8_t requestId,
                       const uint16_t attributeId,
                       const uint16_t valueLen,
                       const uint8_t *value) {
    int valAsInt = *((int16_t *)value);

    if (DEBUG) {
        af_logger_print_buffer("attrNotifyHandler() attrId: ");
        af_logger_print_value(attributeId);
        af_logger_print_buffer(" value: ");
        af_logger_println_value(valAsInt);
    }

    switch (attributeId) {
        case AF_SYSTEM_ASR_STATE:
            af_logger_print_buffer("ASR state: ");
            switch (value[0]) {
            case AF_MODULE_STATE_REBOOTED:
                af_logger_println_buffer("Rebooted");
                break;

            case AF_MODULE_STATE_LINKED:
                af_logger_println_buffer("Linked");
                break;

            case AF_MODULE_STATE_UPDATING:
                af_logger_println_buffer("Updating");
                break;

            case AF_MODULE_STATE_UPDATE_READY:
                af_logger_println_buffer("Update ready - reboot requested");
                reboot_pending = true;
                break;

            default:
                break;
            }
            break;

        default:
            break;
    }

}

void setup() {
    arduino_logger_start(9600);
    af_logger_println_buffer("Serial up; script starting.");

    // Set the pins for the servos
   leftWheel.attach(3);
   rightWheel.attach(4);

    af_transport_t *arduinoSPI = arduino_transport_create_spi(CS_PIN);
    af_lib = af_lib_create(attrSetHandler, attrNotifyHandler, arduinoSPI);
    arduino_spi_setup_interrupts(af_lib, digitalPinToInterrupt(INT_PIN));

    // We allow the system to get ready before sending commands
    start = millis()  + STARTUP_DELAY_MILLIS;
}

void loop() {
    // If we were asked to reboot (e.g. after an OTA firmware update), make the call here in loop()
    // and retry as necessary
    if (reboot_pending) {
       int retVal = af_lib_set_attribute_32(af_lib, AF_SYSTEM_COMMAND, AF_MODULE_COMMAND_REBOOT);
       reboot_pending = (retVal != AF_SUCCESS);
    }

    switch (state) {

    case STATE_INIT:
        if (millis() > start) {
            state = STATE_RUNNING;
                af_logger_println_buffer("Out of INIT, into running.");
        }
        break;

    case STATE_RUNNING:
        break;
    }

    af_lib_loop(af_lib);
}
