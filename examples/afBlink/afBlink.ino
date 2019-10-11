/**
   Copyright 2015 Afero, Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <SPI.h>

#include "af_lib.h"
#include "arduino_spi.h"
#include "arduino_uart.h"
#include "af_module_commands.h"
#include "af_module_states.h"
#include "arduino_transport.h"

// Include the constants required to access attribute ids from your profile.
#include "profile/afBlink/device-description.h"

// Select the MCU interface used (set in the profile, and in hardware)
//#define ARDUINO_USE_UART                  1
#define ARDUINO_USE_SPI                   1

#define BAUD_RATE                 115200

// Automatically detect MCU board type
#if defined(ARDUINO_AVR_UNO)  // using a Plinto shield
#define INT_PIN                   2
#define CS_PIN                    10

#define RX_PIN                    7
#define TX_PIN                    8

#elif defined(TEENSYDUINO)
#define INT_PIN                   14    // Modulo uses this to initiate communication
#define CS_PIN                    10    // Standard SPI chip select (aka SS)
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots

#define RX_PIN                    7
#define TX_PIN                    8

#else
#error "Sorry, afLib does not support this board"
#endif

// Modulo LED is active low
#define LED_OFF                   1
#define LED_ON                    0

// Set a blink interval. There is a minimum time between requests defined
// by afLib as afMINIMUM_TIME_BETWEEN_REQUESTS (currently, 1000ms).
// We can send requests at a slower rate, of course, but try not to send
// requests faster than the minimum so the ASR doesn't get overwhelmed.
#define BLINK_INTERVAL            afMINIMUM_TIME_BETWEEN_REQUESTS * 2  // 2 seconds

/**
   afLib Stuff
*/
af_lib_t *af_lib;
bool reboot_pending = false;              // Track information that a reboot is needed, e.g. if we received an OTA firmware update.

volatile long lastBlink = 0;
volatile bool blinking = false;
volatile bool moduloLEDIsOn = false;      // Track whether the Modulo LED is on
volatile uint16_t moduleButtonValue = 1;  // Track the button value so we know when it has changed

/**
   Forward definitions
*/
void toggleModuloLED();
void setModuloLED(bool on);
void printAttribute(const char *label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);
bool attrSetHandler(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);
void attrNotifyHandler(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);

// Uno boards have insufficient memory to tolerate pretty debug print methods
// Try a Teensyduino
#if !defined(ARDUINO_AVR_UNO)
#define ATTR_PRINT_HEADER_LEN     60
#define ATTR_PRINT_MAX_VALUE_LEN  512   // Max attribute len is 256 bytes; Each HEX-printed byte is 2 ASCII characters
#define ATTR_PRINT_BUFFER_LEN     (ATTR_PRINT_HEADER_LEN + ATTR_PRINT_MAX_VALUE_LEN)
char attr_print_buffer[ATTR_PRINT_BUFFER_LEN];
# endif

void setup() {

  Serial.begin(BAUD_RATE);

  while (!Serial) { // wait for serial port to come ready
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

  /**
     Initialize the afLib - this depends on communications protocol used (SPI or UART)

      Configuration involves a few common items:
          af_transport_t - a transport implementation for a specific protocol (SPI or UART)
          attrSetHandler - the function to be called when one of your attributes has been set.
          attrNotifyHandler - the function to be called in response to a getAttribute call or when an Afero attribute has been updated.
      And a few protocol-specific items:
          for SPI:
              INT_PIN - the pin used for SPI slave interrupt.
              arduinoSPI - class to handle SPI communications.
          for UART:
              RX_PIN, TX_PIN - pins used for receive, transmit.
              arduinoUART - class to handle UART communications.
  */

#if defined(ARDUINO_USE_UART)
  Serial.println("ARDUINO_USE_UART");
  af_transport_t *arduinoUART = arduino_transport_create_uart(RX_PIN, TX_PIN);
  af_lib = af_lib_create(attrSetHandler, attrNotifyHandler, arduinoUART);
#elif defined(ARDUINO_USE_SPI)
  Serial.println("ARDUINO_USE_SPI");
  af_transport_t *arduinoSPI = arduino_transport_create_spi(CS_PIN);
  af_lib = af_lib_create(attrSetHandler, attrNotifyHandler, arduinoSPI);
  arduino_spi_setup_interrupts(af_lib, digitalPinToInterrupt(INT_PIN));
#else
#error "Please define a a communication transport (ie SPI or UART)."
#endif
}

void loop() {
  // If we were asked to reboot (e.g. after an OTA firmware update), make the call here in loop()
  // and retry as necessary
  if (reboot_pending) {
    int retVal = af_lib_set_attribute_32(af_lib, AF_SYSTEM_COMMAND, AF_MODULE_COMMAND_REBOOT);
    reboot_pending = (retVal != 0);
  }

  if (blinking) {
    if (millis() - lastBlink > BLINK_INTERVAL) {
      toggleModuloLED();
      lastBlink = millis();
    }
  } else {
    setModuloLED(false);
  }

  // Give the afLib state machine some time.
  af_lib_loop(af_lib);
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
          if (af_lib_set_attribute_bool(af_lib, AF_BLINK, blinking) != AF_SUCCESS) {
            Serial.println("Could not set BLINK");
          }
        }
      }
      break;

    case AF_SYSTEM_ASR_STATE:
      Serial.print("ASR state: ");
      switch (value[0]) {
        case AF_MODULE_STATE_REBOOTED:
          Serial.println("Rebooted");
          break;
        case AF_MODULE_STATE_LINKED:
          Serial.println("Linked");
          break;
        case AF_MODULE_STATE_UPDATING:
          Serial.println("Updating");
          break;
        case AF_MODULE_STATE_UPDATE_READY:
          Serial.println("Update ready - rebooting");
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

void toggleModuloLED() {
  setModuloLED(!moduloLEDIsOn);
}

void setModuloLED(bool on) {
  if (moduloLEDIsOn != on) {
    int16_t attrVal = on ? LED_ON : LED_OFF; // Modulo LED is active low
    while (af_lib_set_attribute_16(af_lib, AF_MODULO_LED, attrVal) != AF_SUCCESS) {
      Serial.println("Could not set LED");
      af_lib_loop(af_lib);
    }
    moduloLEDIsOn = on;
  }
}

/****************************************************************************************************
   Debug Functions
 *                                                                                                  *
   Some helper functions to make debugging a little easier...
   NOTE: if your sketch doesn't run due to lack of memory, try commenting-out these methods
   and declaration of attr_print_buffer. These are handy, but they do require significant memory.
 ****************************************************************************************************/
#ifdef ATTR_PRINT_BUFFER_LEN

void getPrintAttrHeader(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen) {
  memset(attr_print_buffer, 0, ATTR_PRINT_BUFFER_LEN);
  snprintf(attr_print_buffer, ATTR_PRINT_BUFFER_LEN, "%s id: %s len: %05d value: ", sourceLabel, attrLabel, valueLen);
}

void printAttrBool(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    strcat(attr_print_buffer, *value == 1 ? "true" : "false");
  }
  Serial.println(attr_print_buffer);
}

void printAttr8(const char *sourceLabel, const char *attrLabel, const uint8_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[6];
    strcat(attr_print_buffer, itoa(*((int8_t *)value), intStr, 10));
  }
  Serial.println(attr_print_buffer);
}

void printAttr16(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[6];
    strcat(attr_print_buffer, itoa(*((int16_t *)value), intStr, 10));
  }
  Serial.println(attr_print_buffer);
}

void printAttr32(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[11];
    strcat(attr_print_buffer, itoa(*((int32_t *)value), intStr, 10));
  }
  Serial.println(attr_print_buffer);
}

void printAttrHex(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  for (int i = 0; i < valueLen; i++) {
    strcat(attr_print_buffer, String(value[i], HEX).c_str());
  }
  Serial.println(attr_print_buffer);
}

void printAttrStr(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  int len = strlen(attr_print_buffer);
  for (int i = 0; i < valueLen; i++) {
    attr_print_buffer[len + i] = (char)value[i];
  }
  Serial.println(attr_print_buffer);
}
#endif

void printAttribute(const char *label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
#ifdef ATTR_PRINT_BUFFER_LEN
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

    case AF_SYSTEM_ASR_STATE:
      printAttr8(label, "AF_SYSTEM_ASR_STATE", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_LOW_POWER_WARN:
      printAttr8(label, "AF_ATTRIBUTE_LOW_POWER_WARN", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_REBOOT_REASON:
      printAttrStr(label, "AF_REBOOT_REASON", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_MCU_INTERFACE:
      printAttr8(label, "AF_SYSTEM_MCU_INTERFACE", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_LINKED_TIMESTAMP:
      printAttr32(label, "AF_SYSTEM_LINKED_TIMESTAMP", attributeId, valueLen, value);
      break;
  }
#endif
}
