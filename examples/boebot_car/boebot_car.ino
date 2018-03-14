/**
 * Copyright 2017 Afero, Inc.
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
#include <iafLib.h>
#include <ArduinoSPI.h>
#include <ModuleStates.h>
#include <ModuleCommands.h>
#include "profile/device-description.h"

#define INT_PIN                 2
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

iafLib *aflib;
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
        Serial.print("Updating RIGHT servo with speed: "); Serial.println(currentRightSpeed);
    }
    rightWheel.writeMicroseconds(currentRightSpeed);
}

void updateLeftServoSpeed(int newValue) {
    currentLeftSpeed = newValue;
    if (DEBUG) {
        Serial.print("Updating LEFT servo speed: "); Serial.println(currentLeftSpeed);
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
        Serial.print("ACCELERATOR UPDATE EVENT. Value: "); Serial.println(currentAccelVal);
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
        Serial.print("STEERING UPDATE EVENT. Value: "); Serial.println(currentSteeringVal);
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
        Serial.print("TRANSMISSION UPDATE EVENT. Value: "); Serial.println(currentTransVal);
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
    int valAsInt = *((int *)value);

    if (DEBUG) {
        Serial.print("attrSetHandler() attrId: "); Serial.print(attributeId); Serial.print(" value: "); Serial.println(valAsInt);
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
    int valAsInt = *((int *)value);

    if (DEBUG) {
        Serial.print("attrNotifyHandler() attrId: ");
        Serial.print(attributeId);
        Serial.print(" value: ");
        Serial.println(valAsInt);
    }

    switch (attributeId) {
    case AF_SERVO1:
        updateRightServoSpeed(valAsInt);
        break;

    case AF_SERVO2:
        updateLeftServoSpeed(valAsInt);
        break;

    case AF_SYSTEM_ASR_STATE:
        Serial.print("ASR state: ");
        switch (value[0]) {
        case MODULE_STATE_REBOOTED:
            Serial.println("Rebooted");
            break;

        case MODULE_STATE_LINKED:
            Serial.println("Linked");
            break;

        case MODULE_STATE_UPDATING:
            Serial.println("Updating");
            break;

        case MODULE_STATE_UPDATE_READY:
            Serial.println("Update ready - reboot requested");
            reboot_pending = true;
            break;

        default:
            break;
        }
        break;
    }

}

void ISRWrapper() {
    if (aflib) {
        aflib->mcuISR();
    }
}

void setup() {
    if (DEBUG) {
        Serial.begin(9600);
        // Wait for Serial to be ready...
        while (!Serial) {
            ;
        }
        Serial.println("Serial up; debugging on, script starting.");
    }

    // Set the pins for the servos
    leftWheel.attach(3);
    rightWheel.attach(4);

    afTransport *arduinoSPI = new ArduinoSPI(CS_PIN, &Serial);
    aflib = iafLib::create(digitalPinToInterrupt(INT_PIN), ISRWrapper, attrSetHandler, attrNotifyHandler, &Serial, arduinoSPI);

    // Initialize variables
    initVars();

    // We allow the system to get ready before we start firing commands at it
    start = millis() + STARTUP_DELAY_MILLIS;
}

void loop() {

    // If we were asked to reboot (e.g. after an OTA firmware update), make the call here in loop()
    // and retry as necessary
    if (reboot_pending) {
       int retVal = aflib->setAttribute32(AF_SYSTEM_COMMAND, MODULE_COMMAND_REBOOT);
       reboot_pending = (retVal != 0);
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
        while (aflib->setAttribute16(AF_SERVO1, currentRightSpeed) != afSUCCESS) {
            aflib->loop();
        }
        while (aflib->setAttribute16(AF_SERVO2, currentLeftSpeed) != afSUCCESS) {
            aflib->loop();
        }
        if (currentTransVal == TRANS_PARK) {
            // If we're parked, make sure the UI gets reset
            while (aflib->setAttribute16(AF_ACCEL_ATTR, 0) != afSUCCESS) {
                aflib->loop();
            }
            while (aflib->setAttribute16(AF_STEER_ATTR, 0) != afSUCCESS) {
                aflib->loop();
            }
        }
        state = STATE_RUNNING;
        break;
    }

    aflib->loop();
}
