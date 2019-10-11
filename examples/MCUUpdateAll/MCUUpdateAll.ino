/**
   Copyright 2015-2018 Afero, Inc.

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
//#include "profile/mcu_update_all_Modulo-1_SPI/device-description.h"           // For Modulo-1 SPI
//#include "profile/mcu_update_all_Modulo-1_UART/device-description.h"          // For Modulo-1 UART
//#include "profile/mcu_update_all_Modulo-1B_SPI/device-description.h"          // For Modulo-1B SPI
//#include "profile/mcu_update_all_Modulo-1B_UART/device-description.h"         // For Modulo-1B UART
#include "profile/mcu_update_all_Modulo-2_SPI/device-description.h"           // For Modulo-2 SPI
//#include "profile/mcu_update_all_Modulo-2_UART/device-description.h"          // For Modulo-2 UART

// Select the MCU interface used (set in the profile, and in hardware)
//#define ARDUINO_USE_UART                  1
#define ARDUINO_USE_SPI                   1

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
#define UART_BAUD_RATE            9600

#else
#error "Sorry, afLib does not support this board"
#endif

/**
   afLib Stuff
*/
af_lib_t* af_lib = NULL;
bool rebootPending = false;              // True if reboot needed, e.g. if we received an OTA firmware update
bool asrReady = false;                   // We don't want to talk to the ASR until it's connected and online
bool syncNeededSampleRate = false;       // When the ASR reboots, it'll ask for current values for MCU attributes
bool syncNeededAlarmTemp = false;        // We set these flags in EVENT_MCU_DEFAULT_NOTIFICATION and clear them in loop()
bool syncNeededCurrentTemp = false;      // after the ASR is reconnected

uint8_t sample_rate;                     // sample rate for temp sensor
uint16_t current_temp = 150;              // current sensor reading
uint16_t alarm_temp = 100;                // default alarm temp

// Uno boards have insufficient memory to tolerate pretty-print debug methods
// Try a Teensyduino
#if !defined(ARDUINO_AVR_UNO)
#define ATTR_PRINT_HEADER_LEN     60
#define ATTR_PRINT_MAX_VALUE_LEN  512   // Max attribute len is 256 bytes; Each HEX-printed byte is 2 ASCII characters
#define ATTR_PRINT_BUFFER_LEN     (ATTR_PRINT_HEADER_LEN + ATTR_PRINT_MAX_VALUE_LEN)
char attr_print_buffer[ATTR_PRINT_BUFFER_LEN];
# endif

/**
   Forward definition of debug-print method
*/
void printAttribute(const char* label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t* value);

// Callback executed any time ASR has information for the MCU
void attrEventCallback(const af_lib_event_type_t eventType,
                       const af_lib_error_t error,
                       const uint16_t attributeId,
                       const uint16_t valueLen,
                       const uint8_t* value) {
  printAttribute("attrEventCallback", attributeId, valueLen, value);

  switch (eventType) {
    case AF_LIB_EVENT_UNKNOWN:
      break;

    case AF_LIB_EVENT_ASR_SET_RESPONSE:
      // Response to af_lib_set_attribute() for an ASR attribute
      break;

    case AF_LIB_EVENT_MCU_SET_REQ_SENT:
      // Request from af_lib_set_attribute() for an MCU attribute has been sent to ASR
      break;

    case AF_LIB_EVENT_MCU_SET_REQ_REJECTION:
      // Request from af_lib_set_attribute() for an MCU attribute was rejected by ASR
      break;

    case AF_LIB_EVENT_ASR_GET_RESPONSE:
      // Response to af_lib_get_attribute()
      break;

    case AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION:
      // Unsolicited default notification for an MCU attribute
      Serial.println("AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION");
      Serial.print("attributeId: ");
      Serial.print(attributeId);
      Serial.print(", default value ");
      Serial.println(*value);
      switch (attributeId) {
        case AF_SAMPLE_RATE:
          syncNeededSampleRate = true;
          // we dont have any reason to change the default value in the profile (which is sent to us
          // here) so we set the application sample rate to the value the profile wants us to use
          sample_rate = *value;
          break;

        case AF_CURRENT_TEMP:
          syncNeededCurrentTemp = true;
          // we dont have a default value for this attribute in the profile, because it's a sensor value.
          // When we need to send the ASR a default value for this attribute, use the current sensor setting
          break;

        case AF_ALARM_TEMP:
          syncNeededAlarmTemp = true;
          // our application has a different default value than the profile does (in our example case
          // its hardcoded in the app, but it could easily be stored in flash or some other storage
          // applicable to your device). If the profile default is different than our hard-coded one,
          // we'll use the value in the profile. This allows us to have default behavior in the application
          // but change that default behavior by changing something in the profile, which can be OTAed to
          // a remote device without having to change the MCU code to change behavior
          if (alarm_temp != (uint16_t)*value) {
            alarm_temp = *value;
          }
          break;
      }
      break;

    case AF_LIB_EVENT_ASR_NOTIFICATION:
      // Unsolicited notification of non-MCU attribute change
      switch (attributeId) {
        case AF_SYSTEM_ASR_STATE:
          Serial.print("ASR state: ");
          switch (value[0]) {
            case AF_MODULE_STATE_REBOOTED:
              Serial.println("Rebooted");
              asrReady = false;
              break;

            case AF_MODULE_STATE_LINKED:
              Serial.println("Linked");
#if AF_BOARD == AF_BOARD_MODULO_1
              // For Modulo-1 this is the last connected state you'll get
              asrReady = true;
#endif
              break;

            case AF_MODULE_STATE_UPDATING:
              Serial.println("Updating");
              break;

            case AF_MODULE_STATE_UPDATE_READY:
              Serial.println("Update ready - reboot needed");
              rebootPending = true;
              break;

            case AF_MODULE_STATE_INITIALIZED:
              Serial.println("Initialized");
              asrReady = true;
              break;

            case AF_MODULE_STATE_RELINKED:
              Serial.println("Reinked");
              break;

            default:
              Serial.print("Unexpected state - "); Serial.println(value[0]);
              break;
          }
          break;

        default:
          break;
      }
      break;

    case AF_LIB_EVENT_MCU_SET_REQUEST:
      // Request from ASR to MCU to set an MCU attribute, requires a call to af_lib_send_set_response()
      switch (attributeId) {

        case AF_ALARM_TEMP:
          // if we receive an update to an MCU attr from remote, we must respond that we were
          // able to set the value or not.
          if ((uint16_t)*value > 0 && (uint16_t)*value <= 400) {
            alarm_temp = (uint16_t) * value;
            af_lib_send_set_response(af_lib, AF_ALARM_TEMP, true, valueLen, value);
          } else {
            af_lib_send_set_response(af_lib, AF_ALARM_TEMP, false, valueLen, value);
          }
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

  Serial.begin(115200);

  while (!Serial || millis() < 4000) { // wait for serial port
    ;
  }

  Serial.println("Starting sketch: MCU Update All");

  // The Plinto board automatically connects reset on UNO to reset on Modulo
  // If we're using Teensy, we need to reset manually...
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
          attrEventCallback - the function to be called when ASR has data for MCU.
      And a few protocol-specific items:
          for SPI:
              INT_PIN - the pin used for SPI slave interrupt.
              arduinoSPI - class to handle SPI communications.
          for UART:
              RX_PIN, TX_PIN - pins used for receive, transmit.
              arduinoUART - class to handle UART communications.
  */

  Serial.print("Configuring communications...sketch will use ");
#if defined(ARDUINO_USE_UART)
  Serial.println("UART");
  af_transport_t *arduinoUART = arduino_transport_create_uart(RX_PIN, TX_PIN, UART_BAUD_RATE);
  af_lib = af_lib_create_with_unified_callback(attrEventCallback, arduinoUART);
#elif defined(ARDUINO_USE_SPI)
  Serial.println("SPI");
  af_transport_t* arduinoSPI = arduino_transport_create_spi(CS_PIN);
  af_lib = af_lib_create_with_unified_callback(attrEventCallback, arduinoSPI);
  arduino_spi_setup_interrupts(af_lib, digitalPinToInterrupt(INT_PIN));
#else
#error "Please define a a communication transport (ie SPI or UART)."
#endif
}

void loop() {
  
  // ALWAYS give the afLib state machine some time every loop() that it's possible to do so.
  af_lib_loop(af_lib);

  // If we were asked to reboot (e.g. after an OTA firmware update), make the call here in loop().
  // In order to make this fault-tolerant, we'll continue to retry if the command fails.
  if (rebootPending) {
    int retVal = af_lib_set_attribute_32(af_lib, AF_SYSTEM_COMMAND, AF_MODULE_COMMAND_REBOOT);
    rebootPending = (retVal != AF_SUCCESS);
  }

  if (!asrReady) return;  // Dont do anything below this point if the ASR isn't connected

  // Some housekeeping if needed

  int rc;
  
  if (syncNeededSampleRate) {
    syncNeededSampleRate = false;
    Serial.print("Sending default value for AF_SAMPLE_RATE: ");
    rc = af_lib_set_attribute_8(af_lib, AF_SAMPLE_RATE, sample_rate);
    if (rc == AF_SUCCESS) {
      Serial.println(sample_rate);
      // More robust error handling here would be nice, but it's a bit much for this small example app
    } else {
      Serial.print("error = ");
      Serial.println(rc);      
    }
  }

  if (syncNeededAlarmTemp) {
    syncNeededAlarmTemp = false;
    rc = af_lib_set_attribute_16(af_lib, AF_ALARM_TEMP, alarm_temp);
    if (rc == AF_SUCCESS) {
      Serial.print("Sending default value for AF_ALARM_TEMP: ");
      Serial.println(alarm_temp);
    } else {
      Serial.print("error = ");
      Serial.println(rc);      
    }
  }

  if (syncNeededCurrentTemp) {
    // we dont have a default value for this attribute in the profile, because it's a sensor value.
    // When we need to send the ASR a default value for this attribute, use the current sensor setting
    syncNeededCurrentTemp = false;
    rc = af_lib_set_attribute_16(af_lib, AF_CURRENT_TEMP, current_temp);
    if (rc == AF_SUCCESS) {
      Serial.print("Sending default value for AF_CURRENT_TEMP: ");
      Serial.println(current_temp);
    } else {
      Serial.print("error = ");
      Serial.println(rc);      
    }
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

void getPrintAttrHeader(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId,
                        const uint16_t valueLen) {
  memset(attr_print_buffer, 0, ATTR_PRINT_BUFFER_LEN);
  snprintf(attr_print_buffer, ATTR_PRINT_BUFFER_LEN, "%s id: %s len: %05d value: ", sourceLabel, attrLabel,
           valueLen);
}

void
printAttrBool(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
              const uint8_t* value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    strcat(attr_print_buffer, *value == 1 ? "true" : "false");
  }
  Serial.println(attr_print_buffer);
}

void printAttr8(const char* sourceLabel, const char* attrLabel, const uint8_t attributeId, const uint16_t valueLen,
                const uint8_t* value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[6];
    strcat(attr_print_buffer, itoa(*((int8_t*) value), intStr, 10));
  }
  Serial.println(attr_print_buffer);
}

void
printAttr16(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
            const uint8_t* value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[6];
    strcat(attr_print_buffer, itoa(*((int16_t*) value), intStr, 10));
  }
  Serial.println(attr_print_buffer);
}

void
printAttr32(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
            const uint8_t* value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[11];
    strcat(attr_print_buffer, itoa(*((int32_t*) value), intStr, 10));
  }
  Serial.println(attr_print_buffer);
}

void
printAttrHex(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
             const uint8_t* value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  for (int i = 0; i < valueLen; i++) {
    if (value[i] < 16) {
      strcat(attr_print_buffer, "0");
    }
    strcat(attr_print_buffer, String(value[i], HEX).c_str());
  }
  Serial.println(attr_print_buffer);
}

void
printAttrStr(const char* sourceLabel, const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
             const uint8_t* value) {
  getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  int len = strlen(attr_print_buffer);
  for (int i = 0; i < valueLen; i++) {
    attr_print_buffer[len + i] = (char) value[i];
  }
  Serial.println(attr_print_buffer);
}

#endif

void printAttribute(const char* label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t* value) {
#ifdef ATTR_PRINT_BUFFER_LEN
  switch (attributeId) {
    case AF_APPLICATION_VERSION:
      printAttrHex(label, "AF_APPLICATION_VERSION", attributeId, valueLen, value);
      break;

    case AF_PROFILE_VERSION:
      printAttrHex(label, "AF_PROFILE_VERSION", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_ASR_STATE:
      printAttr8(label, "AF_SYSTEM_ASR_STATE", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_REBOOT_REASON:
      printAttrStr(label, "AF_REBOOT_REASON", attributeId, valueLen, value);
      break;
  }
#endif
}
