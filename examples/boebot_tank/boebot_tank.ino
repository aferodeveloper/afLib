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

#define SERVO_MIN_LIMIT         1300
#define SERVO_MAX_LIMIT         1700

iafLib *aflib;
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

void updateRightServoSpeed(int newValue) {
    if (DEBUG) {
        Serial.print("Updating RIGHT servo with speed: "); Serial.println(newValue);
    }
    rightWheel.writeMicroseconds(newValue);
}

void updateLeftServoSpeed(int newValue) {
    if (DEBUG) {
        Serial.print("Updating LEFT servo speed: "); Serial.println(newValue);
    }
    leftWheel.writeMicroseconds(newValue);
}

// attrSetHandler() is called when a client changes an attribute.
bool attrSetHandler(const uint8_t requestId,
                    const uint16_t attributeId,
                    const uint16_t valueLen,
                    const uint8_t *value) {

    int valAsInt = *((int *)value);

    if (DEBUG) {
        Serial.print("attrSetHandler() attrId: ");
        Serial.print(attributeId);
        Serial.print(" value: ");
        Serial.println(valAsInt);
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
void attrNotifyHandler(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    int valAsInt = *((int *)value);

    if (DEBUG) {
        Serial.print("attrNotifyHandler() attrId: ");
        Serial.print(attributeId);
        Serial.print(" value: ");
        Serial.println(valAsInt);
    }

    switch (attributeId) {
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

        default:
            break;
    }

}

void ISRWrapper() {
    if (aflib) {
        aflib->mcuISR();
    }
}

void setup() {
    Serial.begin(9600);
    // Wait for Serial to be ready...
    while (!Serial) {
        ;
    }
    Serial.println("Serial up; script starting.");

    // Set the pins for the servos
    leftWheel.attach(3);
    rightWheel.attach(4);

    afTransport *arduinoSPI = new ArduinoSPI(CS_PIN, &Serial);
    aflib = iafLib::create(digitalPinToInterrupt(INT_PIN), ISRWrapper, attrSetHandler, attrNotifyHandler, &Serial, arduinoSPI);

    // We allow the system to get ready before sending commands
    start = millis()  + STARTUP_DELAY_MILLIS;
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
    }

    aflib->loop();
}
