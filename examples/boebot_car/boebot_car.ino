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
#define STATE_UPDATE_ATTRS      2

#define RIGHT_SERVO_CTR         1499    // Servo "centerpoints" are around 1500, but exact values depend on the specific servo
#define LEFT_SERVO_CTR          1498    // So values you use here may be different
#define SERVO_MIN_LIMIT         1300
#define SERVO_MAX_LIMIT         1700

#define TRANS_FORWARD           1
#define TRANS_PARK              0
#define TRANS_REVERSE           -1

af_lib_t *af_lib;
int state = STATE_INIT;
unsigned long start;

bool reboot_pending = false;            // Track information that a reboot is needed, e.g. if we received an OTA firmware update.


Servo leftWheel;
Servo rightWheel;

// Vars which track attribute values
int currentLeftSpeed;
int currentRightSpeed;

int currentAccelVal;
int lastAccelVal;

int currentSteeringVal;
int lastSteeringVal;

int currentTransVal;
int lastTransVal;

// Turn up the noise
boolean DEBUG = true;

void updateRightServoSpeed(int newValue) {
    currentRightSpeed = newValue;
    if (DEBUG) {
        af_logger_print_buffer("Updating RIGHT servo with speed: "); af_logger_println_value(currentRightSpeed);
    }
    rightWheel.writeMicroseconds(currentRightSpeed);
}

void updateLeftServoSpeed(int newValue) {
    currentLeftSpeed = newValue;
    if (DEBUG) {
        af_logger_print_buffer("Updating LEFT servo speed: "); af_logger_println_value(currentLeftSpeed);
    }
    leftWheel.writeMicroseconds(currentLeftSpeed);
}

void swapWheelSpeeds() {
    int tempSpeed = currentLeftSpeed;
    currentLeftSpeed = currentRightSpeed;
    currentRightSpeed = tempSpeed;
}

// Ensure wheel speed does not exceed limits
int limitSpeed(int speed) {
    return (min(max(speed, SERVO_MIN_LIMIT), SERVO_MAX_LIMIT));
}

int carUpdateAccelerator(int newValue) {
    currentAccelVal = newValue;
    if (DEBUG) {
        af_logger_print_buffer("ACCELERATOR UPDATE EVENT. Value: "); af_logger_println_value(currentAccelVal);
    }
    int accelDelta = currentAccelVal - lastAccelVal;
    // To move the BoEBOT in a given direction, the servos must rotate in opposite directions
    currentLeftSpeed += accelDelta * currentTransVal;
    currentLeftSpeed = limitSpeed(currentLeftSpeed);
    currentRightSpeed -= accelDelta * currentTransVal;
    currentRightSpeed = limitSpeed(currentRightSpeed);
    return currentAccelVal;
}

int carUpdateSteering(int newValue) {
    currentSteeringVal = newValue;
    if (DEBUG) {
        af_logger_print_buffer("STEERING UPDATE EVENT. Value: "); af_logger_println_value(currentSteeringVal);
    }
    int steeringDelta = currentSteeringVal - lastSteeringVal;
    currentLeftSpeed += steeringDelta * currentTransVal;
    currentLeftSpeed = limitSpeed(currentLeftSpeed);
    currentRightSpeed += steeringDelta * currentTransVal;
    currentRightSpeed = limitSpeed(currentRightSpeed);
    return currentSteeringVal;
}

int carUpdateTransmission(int newValue) {
    currentTransVal = newValue;
    if (DEBUG) {
        af_logger_print_buffer("TRANSMISSION UPDATE EVENT. Value: "); af_logger_println_value(currentTransVal);
    }
    switch (currentTransVal) {
    case (TRANS_PARK):
        initVars();
        break;

    case (TRANS_FORWARD):
        currentTransVal = TRANS_FORWARD;
        if (lastTransVal == TRANS_REVERSE) {
            swapWheelSpeeds();
        }
        break;

    case (TRANS_REVERSE):
        currentTransVal = TRANS_REVERSE;
        if (lastTransVal == TRANS_FORWARD) {
            swapWheelSpeeds();
        }
        break;
    }
    carUpdateAccelerator(currentAccelVal);
    carUpdateSteering(currentSteeringVal);
    return currentTransVal;
}

void initVars() {
    currentLeftSpeed = LEFT_SERVO_CTR;      // Servos initialized to center position--i.e. not moving
    currentRightSpeed = RIGHT_SERVO_CTR;
    currentAccelVal = 0;
    lastAccelVal = 0;
    currentSteeringVal = 0;
    lastSteeringVal = 0;
    currentTransVal = TRANS_PARK;
    lastTransVal = TRANS_PARK;
}

// attrSetHandler() is called when a client changes an attribute.
bool attrSetHandler(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    int valAsInt = *((int16_t *)value);

    if (DEBUG) {
        af_logger_print_buffer("attrSetHandler() attrId: ");
        af_logger_print_value(attributeId);
        af_logger_print_buffer(" value: ");
        af_logger_println_value(valAsInt);
    }

    switch (attributeId) {
    case AF_ACCEL_ATTR:
        lastAccelVal = carUpdateAccelerator(valAsInt);
        state = STATE_UPDATE_ATTRS;
        break;

    case AF_STEER_ATTR:
        lastSteeringVal = carUpdateSteering(valAsInt);
        state = STATE_UPDATE_ATTRS;
        break;

    case AF_TRANSMISSION_ATTR:
        lastTransVal = carUpdateTransmission(valAsInt);
        state = STATE_UPDATE_ATTRS;
        break;
    }

    // Return value from this call informs service whether or not MCU was able to update local state to reflect the
    // attribute change that triggered callback.
    // Return false here if your MCU was unable to update local state to correspond with the attribute change that occurred;
    // return true if MCU was successful.
    return true;
}

// attrNotifyHandler() is called when either an ASR module attribute has been changed or in response to a getAttribute operation.
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
    case AF_SERVO1:
        updateRightServoSpeed(valAsInt);
        break;

    case AF_SERVO2:
        updateLeftServoSpeed(valAsInt);
        break;

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
    }

}

void setup() {
    if (DEBUG) {
        arduino_logger_start(9600);
        af_logger_println_buffer("Serial up; debugging on, script starting.");
    }

    // Set the pins for the servos
    leftWheel.attach(3);
    rightWheel.attach(4);

    af_transport_t *arduinoSPI = arduino_transport_create_spi(CS_PIN);
    af_lib = af_lib_create(attrSetHandler, attrNotifyHandler, arduinoSPI);
    arduino_spi_setup_interrupts(af_lib, digitalPinToInterrupt(INT_PIN));

    // Initialize variables
    initVars();

    // We allow the system to get ready before we start firing commands at it
    start = millis() + STARTUP_DELAY_MILLIS;
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
        }
        break;

    case STATE_RUNNING:
        break;

    case STATE_UPDATE_ATTRS:
        // Could simply call setAttribute16() and ignore return value, but
        // we'll be more robust and retry until success
        while (af_lib_set_attribute_16(af_lib, AF_SERVO1, currentRightSpeed) != AF_SUCCESS) {
            af_lib_loop(af_lib);
        }
        while (af_lib_set_attribute_16(af_lib, AF_SERVO2, currentLeftSpeed) != AF_SUCCESS) {
            af_lib_loop(af_lib);
        }
        if (currentTransVal == TRANS_PARK) {
            // If we're parked, make sure the UI gets reset
            while (af_lib_set_attribute_16(af_lib, AF_ACCEL_ATTR, 0) != AF_SUCCESS) {
                af_lib_loop(af_lib);
            }
            while (af_lib_set_attribute_16(af_lib, AF_STEER_ATTR, 0) != AF_SUCCESS) {
                af_lib_loop(af_lib);
            }
        }
        state = STATE_RUNNING;
        break;
    }

    af_lib_loop(af_lib);
}
