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
#include <afLib.h>

// Include the constants required to access attribute ids from your profile.
#include "profile/afBlink/device-description.h"

#define BAUD_RATE                 38400

// Automatically detect if we are on Teensy or UNO.
#if defined(__AVR_ATmega328P__)
#define UNO     1
#else
#define TEENSY  1
#endif

// Define the pins we need for the platforms we support.
#ifdef TEENSY
#define INT_PIN                   14    // Modulo uses this to initiate communication
#define CS_PIN                    10    // Standard SPI chip select (aka SS)
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots

#elif UNO
#define INT_PIN                   2
#define CS_PIN                    10
#endif

// Modulo LED is active low
#define LED_OFF                   1
#define LED_ON                    0

#define BLINK_INTERVAL 1000

/**
 * afLib Stuff
 */
afLib *aflib;
volatile long lastBlink = 0;
volatile bool blinking = false;
volatile bool moduloLEDIsOn = false;      // Track whether the Modulo LED is on
volatile uint16_t moduleButtonValue = 1;  // Track the button value so we know when it has changed

void setup() {
  Serial.begin(BAUD_RATE);

  // Serial takes a bit to startup on Teensy...
#ifdef TEENSY
  delay(1000);
#endif

  Serial.println("Hello World");

  // The Plinto board automatically connects reset on UNO to reset on Modulo
  // For Teensy, we need to reset manually...
#ifdef TEENSY
  Serial.println("Using Teensy - Resetting Modulo");
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, 0);
  delay(250);
  digitalWrite(RESET, 1);
#endif

  // Initialize the afLib
  // Just need to configure a few things:
  // CS_PIN - the pin to use for chip select
  // INT_PIN - the pin used slave interrupt
  // onAttrSet - the function to be called when one of your attributes has been set.
  // onAttrSetComplete - the function to be called in response to a getAttribute call or when a afero attribute has been updated.
  aflib = new afLib(CS_PIN, digitalPinToInterrupt(INT_PIN), ISRWrapper, onAttrSet, onAttrSetComplete);
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
void onAttrSet(uint8_t requestId, uint16_t attributeId, uint16_t valueLen, uint8_t *value) {
  Serial.print("onAttrSet id: "); Serial.print(attributeId); Serial.print(" value: "); Serial.println(*value);

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
void onAttrSetComplete(uint8_t requestId, uint16_t attributeId, uint8_t *value) {
  Serial.print("onAttrSetComplete id: "); Serial.print(attributeId); Serial.print(" value: "); Serial.println(*value);
    switch (attributeId) {
      // Update the state of the LED based on the actual attribute value.
      case AF_MODULO_LED:
        moduloLEDIsOn = (*value == 0);
        break;

      // Allow the button on Modulo to control our blinking state.
      case AF_MODULO_BUTTON: {
        uint16_t *buttonValue = (uint16_t *)value;
        if (moduleButtonValue != *buttonValue) {
          moduleButtonValue = *buttonValue;
          blinking = !blinking;
          if (aflib->setAttribute(AF_BLINK, (uint8_t)blinking) != afSUCCESS) {
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
    uint16_t attrVal = on ? LED_ON : LED_OFF; // Modulo LED is active low
    if (aflib->setAttribute(AF_MODULO_LED, attrVal) != afSUCCESS) {
      Serial.println("Cound not set LED");
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
