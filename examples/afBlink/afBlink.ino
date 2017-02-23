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

#include <SPI.h>
#include <iafLib.h>
#include <ArduinoSPI.h>
// Include the constants required to access attribute ids from your profile.
#include "profile/afBlink/device-description.h"

#define BAUD_RATE                 38400

// Automatically detect if we are on Teensy or UNO.
#if defined(ARDUINO_AVR_UNO)
#define INT_PIN                   2
#define CS_PIN                    10

#elif defined(ARDUINO_AVR_MEGA2560)
#define INT_PIN                   2
#define CS_PIN                    10
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots

// Need to define these as inputs so they will float and we can connect the real pins for SPI on the
// 2560 which are 50 - 52. You need to use jumper wires to connect the pins from the 2560 to the Plinto.
// Since the 2560 is 5V, you must use a Plinto adapter.
#define MOSI                      11    // 51 on Mega 2560
#define MISO                      12    // 50 on Mega 2560
#define SCK                       13    // 52 on Mega 2560

#elif defined(TEENSYDUINO)
#define INT_PIN                   14    // Modulo uses this to initiate communication
#define CS_PIN                    10    // Standard SPI chip select (aka SS)
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots
#else
#error "Sorry, afLib does not support this board"
#endif

// Modulo LED is active low
#define LED_OFF                   1
#define LED_ON                    0

// Blink as fast as we can. There is a minimum time between requests defined
// by the afLib. We can send requests slower than this, but try not to send
// requests faster than this so the ASR doesn't get overwhelmed.
#define BLINK_INTERVAL            afMINIMUM_TIME_BETWEEN_REQUESTS

#define DEBUG_PRINT_BUFFER_LEN    256

/**
 * afLib Stuff
 */
iafLib *aflib;
volatile long lastBlink = 0;
volatile bool blinking = false;
volatile bool moduloLEDIsOn = false;      // Track whether the Modulo LED is on
volatile uint16_t moduleButtonValue = 1;  // Track the button value so we know when it has changed

/**
 * Forward definitions
 */
void toggleModuloLED();
void setModuloLED(bool on);
void printAttribute(const char *label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);
bool attrSetHandler(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);
void attrNotifyHandler(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);

void setup() {
    Serial.begin(BAUD_RATE);
    while (!Serial) {
        ;
    }

    Serial.println("Hello World");

    // The Plinto board automatically connects reset on UNO to reset on Modulo
    // For Teensy, we need to reset manually...
#if defined(TEENSYDUINO)
    Serial.println("Using Teensy - Resetting Modulo");
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, 0);
    delay(250);
    digitalWrite(RESET, 1);
#endif

#if defined(ARDUINO_AVR_MEGA2560)
    Serial.println("Using MEGA2560 - Resetting Modulo");

    // Allow the Plinto SPI pins to float, we'll drive them from elsewhere
    pinMode(MOSI, INPUT);
    pinMode(MISO, INPUT);
    pinMode(SCK, INPUT);

    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, 0);
    delay(250);
    digitalWrite(RESET, 1);
    delay(1000);
#endif

     ArduinoSPI *arduinoSPI = new ArduinoSPI(CS_PIN);

    /**
     * Initialize the afLib
     *
     * Just need to configure a few things:
     *  INT_PIN - the pin used slave interrupt
     *  ISRWrapper - function to pass interrupt on to afLib
     *  onAttrSet - the function to be called when one of your attributes has been set.
     *  onAttrSetComplete - the function to be called in response to a getAttribute call or when a afero attribute has been updated.
     *  Serial - class to handle serial communications for debug output.
     *  theSPI - class to handle SPI communications.
     */

    aflib = iafLib::create(digitalPinToInterrupt(INT_PIN), ISRWrapper, attrSetHandler, attrNotifyHandler, &Serial, arduinoSPI);
}

void loop() {
    if (blinking) {
        if (millis() - lastBlink > BLINK_INTERVAL) {
            toggleModuloLED();
            lastBlink = millis();
        }
    } else {
        setModuloLED(false);
    }

    // Give the afLib state machine some time.
    aflib->loop();
}

// This is called when the service changes one of our attributes.
bool attrSetHandler(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    printAttribute("attrSetHandler", attributeId, valueLen, value);

    switch (attributeId) {
        // This MCU attribute tells us whether we should be blinking.
        case AF_BLINK:
            blinking = (*value == 1);
            break;

        default:
            break;
    }

    // Return false here if your hardware was unable to perform the set request from the service.
    // This lets the service know that the value did not change.
    return true;
}

// This is called when either an Afero attribute has been changed via setAttribute or in response
// to a getAttribute call.
void attrNotifyHandler(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    printAttribute("attrNotifyHandler", attributeId, valueLen, value);

    switch (attributeId) {
        // Update the state of the LED based on the actual attribute value.
        case AF_MODULO_LED:
            moduloLEDIsOn = (*value == 0);
            break;

            // Allow the button on Modulo to control our blinking state.
        case AF_MODULO_BUTTON: {
            uint16_t *buttonValue = (uint16_t *) value;
            if (moduleButtonValue != *buttonValue) {
                moduleButtonValue = *buttonValue;
                blinking = !blinking;
                if (aflib->setAttributeBool(AF_BLINK, blinking) != afSUCCESS) {
                    Serial.println("Could not set BLINK");
                }
            }
        }
            break;

        default:
            break;
    }
}

void toggleModuloLED() {
    setModuloLED(!moduloLEDIsOn);
}

void setModuloLED(bool on) {
    if (moduloLEDIsOn != on) {
        int16_t attrVal = on ? LED_ON : LED_OFF; // Modulo LED is active low
        if (aflib->setAttribute16(AF_MODULO_LED, attrVal) != afSUCCESS) {
            Serial.println("Could not set LED");
        }
        moduloLEDIsOn = on;
    }
}

// Define this wrapper to allow the instance method to be called
// when the interrupt fires. We do this because attachInterrupt
// requires a method with no parameters and the instance method
// has an invisible parameter (this).
void ISRWrapper() {
    if (aflib) {
        aflib->mcuISR();
    }
}

/****************************************************************************************************
 * Debug Functions                                                                                  *
 *                                                                                                  *
 * Some helper functions to make debugging a little easier...                                       *
 ****************************************************************************************************/
char *getPrintAttrHeader(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen) {
    char intStr[6];
    char *buffer = new char[DEBUG_PRINT_BUFFER_LEN];
    memset(buffer, 0, DEBUG_PRINT_BUFFER_LEN);

    sprintf(buffer, "%s id: %s len: %s value: ", sourceLabel, attrLabel, itoa(valueLen, intStr, 10));
    return buffer;
}

void printAttrBool(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    char *buffer = getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
    strcat(buffer, *value == 1 ? "true" : "false");
    Serial.println(buffer);
    delete(buffer);
}

void printAttr16(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    char *buffer = getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
    char intStr[6];
    strcat(buffer, itoa(*((uint16_t *)value), intStr, 10));
    Serial.println(buffer);
    delete(buffer);
}

void printAttrHex(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    char *buffer = getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
    for (int i = 0; i < valueLen; i++) {
        strcat(buffer, String(value[i], HEX).c_str());
    }
    Serial.println(buffer);
    delete(buffer);
}

void printAttrStr(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    char *buffer = getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
    int len = strlen(buffer);
    for (int i = 0; i < valueLen; i++) {
        buffer[len + i] = (char)value[i];
    }
    Serial.println(buffer);
    delete(buffer);
}

void printAttribute(const char *label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    switch (attributeId) {
        case AF_BLINK:
            printAttrBool(label, "AF_BLINK", attributeId, valueLen, value);
            break;

        case AF_MODULO_LED:
            printAttr16(label, "AF_MODULO_LED", attributeId, valueLen, value);
            break;

        case AF_GPIO_0_CONFIGURATION:
            printAttrHex(label, "AF_GPIO_0_CONFIGURATION", attributeId, valueLen, value);
            break;

        case AF_MODULO_BUTTON:
            printAttr16(label, "AF_MODULO_BUTTON", attributeId, valueLen, value);
            break;

        case AF_GPIO_3_CONFIGURATION:
            printAttrHex(label, "AF_GPIO_3_CONFIGURATION", attributeId, valueLen, value);
            break;

        case AF_BOOTLOADER_VERSION:
            printAttrHex(label, "AF_BOOTLOADER_VERSION", attributeId, valueLen, value);
            break;

        case AF_SOFTDEVICE_VERSION:
            printAttrHex(label, "AF_SOFTDEVICE_VERSION", attributeId, valueLen, value);
            break;

        case AF_APPLICATION_VERSION:
            printAttrHex(label, "AF_APPLICATION_VERSION", attributeId, valueLen, value);
            break;

        case AF_PROFILE_VERSION:
            printAttrHex(label, "AF_PROFILE_VERSION", attributeId, valueLen, value);
            break;

        case AF_SYSTEM_REBOOT_REASON:
            printAttrStr(label, "AF_REBOOT_REASON", attributeId, valueLen, value);
            break;
    }
}