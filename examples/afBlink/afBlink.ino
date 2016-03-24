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

/**
 * afLib Stuff
 */
iafLib *aflib;
volatile long lastBlink = 0;
volatile bool blinking = false;
volatile bool moduloLEDIsOn = false;      // Track whether the Modulo LED is on
volatile uint16_t moduleButtonValue = 1;  // Track the button value so we know when it has changed

void setup() {
    Serial.begin(BAUD_RATE);

    // Serial takes a bit to startup on Teensy...
#if defined(TEENSYDUINO)
    delay(1000);
#endif

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

    aflib = iafLib::create(digitalPinToInterrupt(INT_PIN), ISRWrapper, onAttrSet, onAttrSetComplete, &Serial, arduinoSPI);
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
void onAttrSet(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    Serial.print("onAttrSet id: ");
    Serial.print(attributeId);
    Serial.print(" value: ");
    Serial.println(*value);

    switch (attributeId) {
        // This MCU attribute tells us whether we should be blinking.
        case AF_BLINK:
            blinking = (*value == 1);
            break;

        default:
            break;
    }
    if (aflib->setAttributeComplete(requestId, attributeId, valueLen, value) != afSUCCESS) {
        Serial.println("setAttributeComplete failed!");
    }
}

// This is called when either an Afero attribute has been changed via setAttribute or in response
// to a getAttribute call.
void onAttrSetComplete(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    Serial.print("onAttrSetComplete id: ");
    Serial.print(attributeId);
    Serial.print(" value: ");
    Serial.println(*value);
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
