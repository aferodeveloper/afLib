/**
   Copyright 2015-2019 Afero, Inc.

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

/*
  Get a reasonably accurate time-of-day from the Afero Cloud, two different ways

  When the ASR links with the Afero Cloud, it returns two attributes to the MCU:
  65015: LINKED_TIMESTAMP     (signed 32-bit long)
         This is a UNIX Epoch (number of seconds since 00:00:00 1/1/1970) UTC timestamp
         of when the ASR successfully last linked to the Afero Cloud. When returned
         shortly after the ASR reboots, it should be reasonably close to the actual
         current time. The latency from the Cloud back to the MCU should typically be less
         than 1-2 seconds, but it's not safe to rely on this timestamp to be more accurate
         than maybe +/- 1 minute or so. But if your TOD requirements are not too critical, it's
         a usable timestamp that's given to you automatically.
  65001: UTC_OFFSET_DATA      (byte[8])
         This byte array contains the current localtime offset from UTC, the next
         UTC offset, and a UNIX Epoch timestamp of when the "next" UTC offset is valid.
         The byte array consists of:
           [0-1]: little-endian signed int containing the local timezone offzet from UTC in minutes
           [2-5]: little-endian unsigned long containing an Epoch timestamp (UTC) for "next" offset validity
           [6-7]: little-endian signed int containing the "next" local timezone offset from UTC in minutes

         NOTE: UTC Offset is determined by the Location attributes set (by you) for the ASR and
               ARE NOT DYNAMIC in any way. The UTC offset can and will be wrong if the Location in the
               device configuration is incorrect. You can set this location for your Afero device
               with the Afero mobile app or in the Inspector tool at https://inspector.afero.io

  For example, you may receive a attributes that look like this:
  LINKED_TIMESTAMP=0x5a2b06c3
  UTC_OFFSET_DATA=0xD4FEF0D3A45A10FF

  This translates to:
  Link Time: 0x5a2b06c3 (Fri 2017-12-08 21:40:19 GMT)
  Current UTC offset: -300 minutes (0xFED4)    (Eastern Standard Time, in this example)
  Next UTC offset: -240 minutes (0xFF10)       (Eastern Daylight Time, in this example)
  UTC Offset Change: Sun 2018-03-11 07:00:00 GMT (0x5AA4D3F0) (Spring 2018 USA DST Time Change, 2am local time)

  Care should be taken that LINKED_TIMESTAMP only be assumed to be current when the ASR state transitions
  to a connected state. It is possible to use getAttribute to return the linked timestamp at any point,
  and the result returned will be the time the ASR was first connected to the Cloud, which might be
  significantly in the past if the device has been connected for a long time. This example code uses a flag called
  timestamp_read as a very paranoid way to prevent these timestamps from being updated - if the risk of a spurious
  linked timestamp update is not that much of a concern to you all of the logic that around that flag can be
  removed for simplification.

  This example app will provide a simple method to listen for these attributes in attrNotifyHandler and set a
  C time struct that can be manipulated within your application. For better results, you should use a real
  RTC chip and use these attributes to set the RTC so you have a reasonably accurate clock when not connected
  to the Afero Cloud.

  To work properly, this application needs a Modulo-1 or 2 running firmware 1.2.1 or higher, with a
  profile created with Afero Profile Editor 1.2.1 or higher.

  When enabled in your profile, the ASR will return this attribute every minute after it's linked:
   1201: ASR_UTC_TIME     (signed 32-bit long)
         This is a UNIX Epoch (number of seconds since 00:00:00 1/1/1970) UTC timestamp
         sent from the ASR every minute "on the minute" (:00 seconds) after the ASR connects to the Afero Cloud.
         This attribute is not synced to any Network Time protocol, so it's not guaranteed to be accurate to more
         than +/- 1-2s or so, but it is accurate enough to use this timestamp to provide a time of day if your
         hardware doesn't have RTC support.

   ** Note: ASR_UTC_TIME is not supported on Modulo-1/ASR-1 but is supported on all other Afero modules.

*/

#include <stdio.h>
#include <time.h>

#include "af_lib.h"
#include "af_module_commands.h"
#include "af_module_states.h"
#include "af_logger.h"

// Platform specific includes
#include <SPI.h>
#include "arduino_spi.h"
#include "arduino_uart.h"
#include "arduino_transport.h"
#include "arduino_logger.h"


// We don't actually need any specific profile config on the Modulo since these
// system attributes exist in all profiles created with Profile Editor > 1.2.1
// as long as the Modulo is flashed with a profile that has MCU support enabled.
// We have a demo profile here that you can flash to our Modulo if you need one
//#include "profile/afero_clock_Modulo-1_SPI/device-description.h"           // For Modulo-1 SPI
//#include "profile/afero_clock_Modulo-1_UART/device-description.h"          // For Modulo-1 UART
//#include "profile/afero_clock_Modulo-1B_SPI/device-description.h"          // For Modulo-1B SPI
//#include "profile/afero_clock_Modulo-1B_UART/device-description.h"         // For Modulo-1B UART
//#include "profile/afero_clock_Modulo-2_SPI/device-description.h"           // For Modulo-2 SPI
//#include "profile/afero_clock_Modulo-2_UART/device-description.h"          // For Modulo-2 UART
#include "profile/afero_clock_Plumo-2D_UART/device-description.h"         // For Plumo-2D, UART



// By default, this application uses the SPI interface to the Modulo.
// This code works just fine using the UART interface to the Modulo as well,
// see the afBlink example for suggested UART setup - to keep this code simple
// we're just going to use the SPI interface accessible through the Uno+Plinto or
// through the Modulo's Teensyduino compatible pinout.

//  See the afBlink example app for communicating with Modulo over its UART interface.
// Select the MCU interface to be used. This define must also match
// the physical hardware pins connecting the MCU and Modulo, and also
// the MCU Configuration section of the profile loaded on the Modulo
//     (in the Afero Profile Editor app "Attributes" menu, interface and UART speed)
//#define ASR_USE_SPI                   1
#define ASR_USE_UART                  1

// For Modulo-1 UART speed is fixed at 9600
// For Modulo-2 the UART_BAUD_RATE here must match the UART speed set in Profile Editor
#define UART_BAUD_RATE            9600

// Automatically detect MCU board type
#if defined(ARDUINO_AVR_UNO)          // Arduino Uno using an Afero Plinto shield
#undef RESET                            // On Plinto the ASR RESET line is tied to the Uno RESET line
// Only needed for SPI
#define INT_PIN                   2     // Modulo sends an interrupt on this pin to initiate communication
#define CS_PIN                    10    // Standard SPI chip select line
// Only needed for UART
// For Uno and UART you will need to connect the UART pins on the Modulo board to these
// pins on the Plinto board
#define RX_PIN                    7     // Connect to Modulo UART_TX
#define TX_PIN                    8     // Connect to Modulo UART_RX

// The Modulo developer boards match the pinout of the Teensyduino 3.2 from www.pjrc.com
// You can connect the pins below to the same pins on the Modulo for easy connection,
// or you can put socket headers on the Teensy and the Modulo will plug right on top of the MCU
// (The Modulo has 8 extra pins that the Teensy doesn't, so it will overhang the MCU a bit)

#elif defined(TEENSYDUINO)            // Teensy 3.2 MCU from www.pjrc.com
#define RESET                     21    // This pin is used to reboot the Modulo when needed
// Only needed for SPI
#define INT_PIN                   14    // Modulo sends an interrupt on this pin to initiate communication
#define CS_PIN                    10    // Standard SPI chip select line
// Only needed for UART/
#define RX_PIN                    7
#define TX_PIN                    8

#else
#error "Sorry, afLib can not identify this board, define necessary pins in sketch"
#endif

// Serial monitor baud rate
#define DEBUG_BAUD_RATE           115200

bool    timestamp_read         = false;
int32_t linked_timestamp       = 0;
int16_t utc_offset_now         = 0;
int16_t utc_offset_next        = 0;
int32_t utc_offset_change_time = 0;
int32_t asr_utc_time           = 0;

// define an afLib instance and a few booleans so we know the state of the connection to the Afero Cloud
af_lib_t* af_lib = NULL;
volatile bool asrReady = false;          // If false, we're waiting on AF_MODULE_STATE_INITIALIZED, if true, we can communicate with ASR
volatile bool asrRebootPending = false;  // If true, a reboot is needed, e.g. if we received an OTA firmware or profile update.


// Callback executed any time ASR has information for the MCU
void attrEventCallback(const af_lib_event_type_t eventType,
                       const af_lib_error_t error,
                       const uint16_t attributeId,
                       const uint16_t valueLen,
                       const uint8_t* value) {
  //printAttribute(attributeId, valueLen, value);

  switch (eventType) {

    case AF_LIB_EVENT_ASR_NOTIFICATION:
      // Unsolicited notification of non-MCU attribute change
      switch (attributeId) {
        case AF_SYSTEM_LINKED_TIMESTAMP:
          if (!timestamp_read) {
            linked_timestamp = *((int32_t *)value);
            if (linked_timestamp != 0) {
              af_logger_print_buffer("ASR linked at: ");
              printTimeString((time_t)linked_timestamp);
              // TODO: If you have an RTC this would be a good place to set the clock time
            }
          }
          if (!timestamp_read && linked_timestamp > 0 && utc_offset_change_time > 0) timestamp_read = true;
          break;
        case AF_SYSTEM_UTC_OFFSET_DATA:
          if (valueLen > 0) {
            utc_offset_now         = value[1] << 8 | value[0];
            utc_offset_next        = value[7] << 8 | value[6];
            // The line below extracts the int32 timestamp from bytes 2-5 of the attribute
            utc_offset_change_time = *((int32_t *)(value + 2));
            if (utc_offset_change_time > 0) {
              af_logger_print_buffer("Current TZ: ");
              af_logger_print_value((float)utc_offset_now / 60);
              af_logger_println_buffer("h");
              af_logger_print_buffer("Next Offset: ");
              af_logger_print_value((float)utc_offset_next / 60);
              af_logger_print_buffer("h at ");
              printTimeString((time_t)utc_offset_change_time);
              // TODO: If you have an RTC this would be a good place to set the clock time and/or timezone
            }
            if (!timestamp_read && linked_timestamp > 0 && utc_offset_change_time > 0) timestamp_read = true;
          }
          break;

        case AF_UTC_TIME:
          asr_utc_time = *((int32_t *)value);
          af_logger_print_buffer("ASR UTC Time: ");
          printTimeString((time_t)asr_utc_time);
          // TODO: If you have an RTC this would be a good place to set the clock time
          break;

        case AF_SYSTEM_ASR_STATE:
          af_logger_print_buffer("ASR state=");
          switch (value[0]) {
            case AF_MODULE_STATE_REBOOTED:
              af_logger_println_buffer("Rebooted");
              asrReady = false;   // Rebooted, so we we can't talk to it yet
              break;

            case AF_MODULE_STATE_LINKED:
              af_logger_println_buffer("Linked");
              // "Linked" is the final connected state returned by the Modulo-1
              // all other devices will return an Initialized state below when connection is completed
#if AF_BOARD == AF_BOARD_MODULO_1
              asrReady = true;
#endif
              break;

            case AF_MODULE_STATE_UPDATING:
              af_logger_println_buffer("Updating");
              break;

            case AF_MODULE_STATE_UPDATE_READY:
              af_logger_println_buffer("Updated, need reboot");
              asrRebootPending = true;
              break;

            case AF_MODULE_STATE_INITIALIZED:
              af_logger_println_buffer("Initialized");
              asrReady = true;
              break;

            case AF_MODULE_STATE_FACTORY_RESET:
              // We don't really have anything to clean up since we don't persist any stuff so all we need to do is reboot the ASR
              af_logger_println_buffer("Factory reset - rebooting");
              asrRebootPending = true;
              break;

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
  arduino_logger_start(DEBUG_BAUD_RATE);

  af_logger_println_buffer("afLib: Time of Day from Afero Cloud Example");

  // The Plinto board automatically connects reset on UNO to reset on Modulo
  // For Teensy, we need to reset manually...
#if defined(TEENSYDUINO)
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, 0);
  delay(250);
  digitalWrite(RESET, 1);
  // This leaves the reset pin pulled high when unused
  pinMode(RESET, INPUT_PULLUP);
#endif


  // Start the sketch awaiting initialization
  asrReady = false;


  /**
     Initialize the afLib - this depends on communications protocol used (SPI or UART)

      Configuration involves a few common items:
          af_transport_t - a transport implementation for a specific protocol (SPI or UART)
          attrEventCallback - the function in this application to be called when ASR has data for MCU.
      And a few protocol-specific items:
          for SPI:
              INT_PIN - the pin used for SPI slave interrupt.
              platformSPI - class to handle SPI communications.
          for UART:
              RX_PIN, TX_PIN - pins used for receive, transmit.
              platformUART - class to handle UART communications.
  */

#if defined(ASR_USE_UART)
  af_logger_println_buffer("Using UART");
  af_transport_t *platformUART = arduino_transport_create_uart(RX_PIN, TX_PIN, UART_BAUD_RATE);
  af_lib = af_lib_create_with_unified_callback(attrEventCallback, platformUART);
#elif defined(ASR_USE_SPI)
  af_logger_println_buffer("Using SPI");
  af_transport_t* platformSPI = arduino_transport_create_spi(CS_PIN);
  af_lib = af_lib_create_with_unified_callback(attrEventCallback, platformSPI);
  arduino_spi_setup_interrupts(af_lib, digitalPinToInterrupt(INT_PIN));
#else
#error "Please define a a communication transport (ie SPI or UART)."
#endif
}


void loop() {
  // ALWAYS give the afLib state machine time to do its work - avoid blind delay() calls
  // of any length, instead repeat calls to af_lib_loop() while waiting for whatever it is
  // you're waiting to happen. Some afLib operations take multiple loop() calls to complete.
  af_lib_loop(af_lib);

  // Until the ASR is initialized, we shouldn't talk to it, if it's not ready we just exit loop() altogether.
  if (!asrReady) return;

  // asrReady == true from here on down
  // If we were asked to reboot (e.g. after an OTA firmware update), do it now.
  if (asrRebootPending) {
    int retVal = af_lib_set_attribute_32(af_lib, AF_SYSTEM_COMMAND, AF_MODULE_COMMAND_REBOOT, AF_LIB_SET_REASON_LOCAL_CHANGE);
    // If retVal returns an error, leave asrRebootPending set to try it again next time around
    asrRebootPending = (retVal != AF_SUCCESS);
    // if retVal returned success, then we wait for the ASR reboot to happen
    if (!asrRebootPending) {
      asrReady = false;
    }
  }

}

void printTimeString(time_t t) {
#if defined(ARDUINO_AVR_UNO)
  // Arduino Uno seems to have trouble with strftime that other MCUs don't have.
  // So instead of an easy and simple strftime, let's dump the timestamp here to show it's
  // at least correct. Parsing this into a time value is an exercise left to the user
  // (solution hint: use a better MCU than an Uno)
  af_logger_print_buffer("timestamp=");
  af_logger_println_value(t);
#else
  char buf[80];

  struct tm  ts;
  ts = *localtime(&t);

  // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
  strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
  af_logger_println_buffer(buf);
#endif
}






// Debugging Stuff

// Arduino Uno boards have inadequate memory to tolerate pretty-printing debug methods
// so these are pretty basic just to show program flow.
#define ATTR_PRINT_BUFFER_LEN     60

char attr_print_buffer[ATTR_PRINT_BUFFER_LEN];

void getPrintAttrHeader(const char* attrLabel, const uint16_t attributeId,
                        const uint16_t valueLen) {
  memset(attr_print_buffer, 0, ATTR_PRINT_BUFFER_LEN);
  snprintf(attr_print_buffer, ATTR_PRINT_BUFFER_LEN, "attr %s=", attrLabel);
}

void
printAttr16(const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
            const uint8_t* value) {
  getPrintAttrHeader(attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[6];
    strcat(attr_print_buffer, itoa(*((int16_t*) value), intStr, 10));
  }
  af_logger_println_buffer(attr_print_buffer);
}

void
printAttr32(const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
            const uint8_t* value) {
  getPrintAttrHeader(attrLabel, attributeId, valueLen);
  if (valueLen > 0) {
    char intStr[11];
    strcat(attr_print_buffer, itoa(*((int32_t *)value), intStr, 10));
  }
  af_logger_println_buffer(attr_print_buffer);
}


void
printAttrHex(const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
             const uint8_t* value) {
  getPrintAttrHeader(attrLabel, attributeId, valueLen);
  for (int i = 0; i < valueLen; i++) {
    strcat(attr_print_buffer, String(value[i], HEX).c_str());
  }
  af_logger_println_buffer(attr_print_buffer);
}


void
printAttrStr(const char* attrLabel, const uint16_t attributeId, const uint16_t valueLen,
             const uint8_t* value) {
  getPrintAttrHeader(attrLabel, attributeId, valueLen);
  int len = strlen(attr_print_buffer);
  for (int i = 0; i < valueLen; i++) {
    attr_print_buffer[len + i] = (char) value[i];
  }
  af_logger_println_buffer(attr_print_buffer);
}

void printAttribute(const uint16_t attributeId, const uint16_t valueLen, const uint8_t* value) {
  switch (attributeId) {
    case AF_SYSTEM_LINKED_TIMESTAMP:
      printAttr32("Linked Timestamp", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_UTC_OFFSET_DATA:
      printAttrHex("UTC Offset Data", attributeId, valueLen, value);
      break;

    case AF_UTC_TIME:
      printAttr32("ASR Timestamp", attributeId, valueLen, value);
      break;

    case AF_APPLICATION_VERSION:
      printAttr16("FW Version", attributeId, valueLen, value);
      break;

    case AF_PROFILE_VERSION:
      printAttr16("Profile Version", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_REBOOT_REASON:
      // This attribute can be very long, but the first few characters are all we
      // generally care about - the Uno is very short on RAM so we don't waste it
      // and we only dump the first few characters
      printAttrStr("Reboot Reason", attributeId, 9, value);
      break;
  }
}


