/**
   Copyright 2017 Afero, Inc.

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

// This app is for Teensy 3 ONLY! It'll crash on anything smaller.

// This example app demonstrates a little bit of "extra info" that is presented
// to the MCU from the Modulo developer board. All Afero attributes that are
// normally delivered to the MCU are defined here, and useful information in
// them is parsed. Under most circumstances, an MCU application wouldn't have
// much need for any of this extra information, but sometimes it might be nice
// to know.

// Unlike most Afero apps, this application does not need to know what profile
// is installed on the Modulo - it doesn't need a device-description.h file at all,
// as long as the profile on the Modulo has an MCU Interface enabled that matches the
// MCU interface defined here.

#include <SPI.h>

#include "af_lib.h"
#include "arduino_spi.h"
#include "arduino_uart.h"
#include "af_module_commands.h"
#include "af_module_states.h"
#include "arduino_transport.h"

#include "debugprints.h"
#include "attrDB.h"

// If you want the full effect of debugging MCU attributes as well, you can use one
// of the example profiles here (or your own)
//#include "profile/attribute_dumper_Modulo-1_SPI/device-description.h"           // For Modulo-1 SPI
//#include "profile/attribute_dumper_Modulo-1_UART/device-description.h"          // For Modulo-1 UART
//#include "profile/attribute_dumper_Modulo-1B_SPI/device-description.h"          // For Modulo-1B SPI
//#include "profile/attribute_dumper_Modulo-1B_UART/device-description.h"         // For Modulo-1B UART
//#include "profile/attribute_dumper_Modulo-2_SPI/device-description.h"           // For Modulo-2 SPI
//#include "profile/attribute_dumper_Modulo-2_UART/device-description.h"          // For Modulo-2 UART

// However, this is all we need out of a profile. defining it here means we don't have to include
// a device-description.h here and we'll work with anything as long as the profile
// has a compatible MCU interface set
#if !defined(AF_SYSTEM_COMMAND)
#define AF_SYSTEM_COMMAND                                        65012
#endif

// Select the MCU interface used (set in the profile, and in hardware)
//#define ARDUINO_USE_UART                  1
#define ARDUINO_USE_SPI                   1

#define BAUD_RATE                 115200

// Teensy 3.2 pins
#define INT_PIN                   14    // Modulo uses this to initiate communication
#define CS_PIN                    10    // Standard SPI chip select (aka SS)
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots

#define RX_PIN                    7
#define TX_PIN                    8

af_lib_t* af_lib = NULL;
bool initializationPending = false; // If true, we're waiting on AF_MODULE_STATE_INITIALIZED
bool rebootPending = false;         // If true, a reboot is needed, e.g. if we received an OTA firmware update.


// Callback executed any time ASR has information for the MCU
void attrEventCallback(const af_lib_event_type_t eventType,
                       const af_lib_error_t error,
                       const uint16_t attributeId,
                       const uint16_t valueLen,
                       const uint8_t* value) {

  switch (eventType) {
    case AF_LIB_EVENT_ASR_NOTIFICATION:
      // Unsolicited notification of non-MCU attribute change
      printAttribute("notify", attributeId, valueLen, value);
      switch (attributeId) {
        case 65013: //AF_SYSTEM_ASR_STATE:
          switch (value[0]) {
            case 0: // AF_MODULE_STATE_REBOOTED
              initializationPending = true;   // Rebooted, so we're not yet initialized
              break;

            case 1: // AF_MODULE_STATE_LINKED
              initializationPending = false;
              break;

            case 3: // MODULE_STATE_UPDATE_READY
              rebootPending = true;
              break;

          }
          break;

        default:
          break;
      }
      break;
    case AF_LIB_EVENT_MCU_SET_REQUEST:
      // Request from ASR to MCU to set an MCU attribute, requires a call to af_lib_send_set_response()
      printAttribute("   set", attributeId, valueLen, value);
      af_lib_send_set_response(af_lib, attributeId, true, valueLen, value);
      break;

    default:
      break;
  }
}






void setup() {
  Serial.begin(BAUD_RATE);
  while (!Serial || millis() < 4000) {
    ;
  }

  Serial.println("Resetting Modulo");
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, 0);
  delay(250);
  digitalWrite(RESET, 1);

  // Start the sketch awaiting initialization
  initializationPending = true;

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
  af_transport_t *arduinoUART = arduino_transport_create_uart(RX_PIN, TX_PIN, 9600);
  af_lib = af_lib_create_with_unified_callback(attrEventCallback, arduinoUART);
#elif defined(ARDUINO_USE_SPI)
  Serial.println("SPI");
  af_transport_t* arduinoSPI = arduino_transport_create_spi(CS_PIN);
  af_lib = af_lib_create_with_unified_callback(attrEventCallback, arduinoSPI);
  arduino_spi_setup_interrupts(af_lib, digitalPinToInterrupt(INT_PIN));
#else
#error "Please define a a communication transport (ie SPI or UART)."
#endif

  createAttrDB();
  Serial.print("Attr DB size=");
  Serial.println(getAttrDBSize());
}

void loop() {
  // Give the afLib state machine some time.
  af_lib_loop(af_lib);

  if (initializationPending) {
    // If we're awaiting initialization, don't bother checking/setting attributes
  } else {

    // If we were asked to reboot (e.g. after an OTA firmware update), make the call here in loop().
    // In order to make this fault-tolerant, we'll continue to retry if the command fails.
    if (rebootPending) {
      int retVal = af_lib_set_attribute_32(af_lib, AF_SYSTEM_COMMAND, AF_MODULE_COMMAND_REBOOT);
      rebootPending = (retVal != AF_SUCCESS);
      if (!rebootPending) {
        Serial.println("*************************************************************************");
        Serial.println("REBOOT COMMAND SENT; NOW AWAITING AF_MODULE_STATE_INITIALIZED");
        initializationPending = true;
      }
    }
  }
}



