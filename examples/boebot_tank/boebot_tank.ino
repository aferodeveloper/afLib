#include <Servo.h>

#include <iafLib.h>
#include <arduinoSPI.h>
#include "profile/device-description.h"

#define INT_PIN                 2
#define CS_PIN                  10    // Standard SPI chip select (aka SS)

#define STARTUP_DELAY_MICROS    10000
#define STATE_INIT              0
#define STATE_RUNNING           1

#define SERVO_MIN_LIMIT         1300
#define SERVO_MAX_LIMIT         1700

iafLib *aflib;
int state = STATE_INIT;
unsigned long start;

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

// This is called when the service changes one of our attributes.
void onAttrSet(const uint8_t requestId,
               const uint16_t attributeId,
               const uint16_t valueLen,
               const uint8_t *value) {

    int valAsInt = *((int *)value);

    if (DEBUG) {
        Serial.print("onAttrSet() attrId: ");
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

    if (aflib->setAttributeComplete(requestId, attributeId, valueLen, value) != afSUCCESS) {
        Serial.println("setAttributeComplete failed!");
    }
}

// This is called when either an Afero attribute has been changed via setAttribute or in response to a getAttribute call.
void onAttrSetComplete(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    int valAsInt = *((int *)value);

    if (DEBUG) {
        Serial.print("onAttrSetComplete() attrId: "); Serial.print(attributeId); Serial.print(" value: "); Serial.println(valAsInt);
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

    // Set the pins for the servos
    leftWheel.attach(3);
    rightWheel.attach(4);

    // Initialize the afLib
    ArduinoSPI *theSPI = new ArduinoSPI(CS_PIN);
    aflib = iafLib::create(digitalPinToInterrupt(INT_PIN), ISRWrapper, onAttrSet, onAttrSetComplete, &Serial, theSPI);

    // Mark the start time
    start = millis();
}

void loop() {

    switch (state) {

    case STATE_INIT:
        // We allow the system to get ready before sending commands
        if (millis() > start + STARTUP_DELAY_MICROS) {
            state = STATE_RUNNING;
        }
        break;

    case STATE_RUNNING:
        break;
    }

    aflib->loop();
}
