#include <Servo.h>

#include <iafLib.h>
#include <arduinoSPI.h>
#include "profile/device-description.h"

#define INT_PIN                 2
#define CS_PIN                  10    // Standard SPI chip select (aka SS)

#define STARTUP_DELAY_MICROS    10000
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

// This is called when the service changes one of our attributes.
void onAttrSet(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    int valAsInt = *((int *)value);

    if (DEBUG) {
        Serial.print("onAttrSet() attrId: "); Serial.print(attributeId); Serial.print(" value: "); Serial.println(valAsInt);
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

    if (aflib->setAttributeComplete(requestId, attributeId, valueLen, value) != afSUCCESS) {
        Serial.println("setAttributeComplete failed!");
    }
}

// This is called when either an Afero attribute has been changed via setAttribute
// or in response to a getAttribute call.
void onAttrSetComplete(const uint8_t requestId,
                       const uint16_t attributeId,
                       const uint16_t valueLen,
                       const uint8_t *value) {
    int valAsInt = *((int *)value);

    if (DEBUG) {
        Serial.print("onAttrSetComplete() attrId: ");
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

    // Initialize the afLib
    ArduinoSPI *theSPI = new ArduinoSPI(CS_PIN);
    aflib = iafLib::create(digitalPinToInterrupt(INT_PIN), ISRWrapper, onAttrSet, onAttrSetComplete, &Serial, theSPI);

    // Initialize variables
    initVars();

    // Mark the start time
    start = millis();
}

void loop() {

    switch (state) {
    case STATE_INIT:
        // We allow the system to get ready before we start firing commands at it
        if (millis() > start + STARTUP_DELAY_MICROS) {
            state = STATE_RUNNING;
        }
        break;

    case STATE_RUNNING:
        break;

    case STATE_UPDATE_ATTRS:
        aflib->setAttribute16(AF_SERVO1, currentRightSpeed);
        aflib->setAttribute16(AF_SERVO2, currentLeftSpeed);
        if (currentTransVal == TRANS_PARK) {
            // If we're parked, make sure the UI gets reset
            aflib->setAttribute16(AF_ACCEL_ATTR, 0);
            aflib->setAttribute16(AF_STEER_ATTR, 0);
        }
        state = STATE_RUNNING;
        break;
    }

    aflib->loop();
}
