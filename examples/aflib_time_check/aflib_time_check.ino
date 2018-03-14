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

/*
  Get a reasonably accurate time-of-day from the Afero Cloud

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

*/

#include <SPI.h>
#include <iafLib.h>
#include <ArduinoSPI.h>
#include <ModuleCommands.h>
#include <ModuleStates.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <time.h>

// We don't actually need any specific profile config on the Modulo since these
// system attributes exist in all profiles created with Profile Editor > 1.2.1
// as long as the Modulo is flashed with a profile that has MCU support enabled.
// We have a demo profile here that you can flash to our Modulo if you need one
#include "profile/aflib_time_check_modulo1/device-description.h"
//#include "profile/aflib_time_check_modulo2/device-description.h"


// By default, this application uses the SPI interface to the Modulo.
// This code works just fine using the UART interface to the Modulo as well,
// see the afBlink example for suggested UART setup - to keep this code simple
// we're just going to use the SPI interface accessible through the Uno+Plinto or
// through the Modulo's Teensyduino compatible pinout.
//  See the afBlink example app for communicating with Modulo over its UART interface.


// Serial monitor baud rate
#define BAUD_RATE                 115200



// Automatically detect if we are on Teensy or UNO, so we can properly reset the ASR
// if not attached to an Uno+Plinto
#if defined(ARDUINO_AVR_UNO)
#define INT_PIN                   2
#define CS_PIN                    10

#elif defined(TEENSYDUINO)
#define INT_PIN                   14    // Modulo uses this to initiate communication
#define CS_PIN                    10    // Standard SPI chip select (aka SS)
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots
#else
#error "Sorry, afLib does not support this board"
#endif

bool    timestamp_read         = false;
int32_t linked_timestamp       = 0;
int16_t utc_offset_now         = 0;
int16_t utc_offset_next        = 0;
int32_t utc_offset_change_time = 0;

// Used by debugging print routines at the bottom
#define DEBUG_PRINT_BUFFER_LEN    512

iafLib *aflib;


void setup() {
  Serial.begin(BAUD_RATE);
  // Wait for Serial to come up, or time out after 3 seconds if there's no serial monitor attached
  while (!Serial || millis() < 3000) {
    ;
  }

  Serial.println(F("afLib: Time of Day from Afero Cloud Example"));

  // The Plinto board automatically connects reset on UNO to reset on Modulo
  // For Teensy, we need to reset manually...
#if defined(TEENSYDUINO)
  Serial.println(F("Using Teensy - Resetting Modulo"));
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, 0);
  delay(250);
  digitalWrite(RESET, 1);
#endif


  /**
     Initialize the afLib

     Just need to configure a few things:
      INT_PIN - the pin used slave interrupt
      ISRWrapper - function to pass interrupt on to afLib
      onAttrSet - the function to be called when one of your attributes has been set.
      onAttrSetComplete - the function to be called in response to a getAttribute call or when an afero attribute has been updated.
      Serial - class to handle serial communications for debug output.
      theSPI - class to handle SPI communications.
  */

  afTransport *arduinoSPI = new ArduinoSPI(CS_PIN, &Serial);
  aflib = iafLib::create(digitalPinToInterrupt(INT_PIN), ISRWrapper, attrSetHandler, attrNotifyHandler, &Serial, arduinoSPI);
}

void loop() {
  // We don't actually do anything here, so we just give afLib->loop() some time to process
  // The actual functionality all happens in attrNotifyHandler
  aflib->loop();
}

void printTimeString(time_t t) {
#if defined(ARDUINO_AVR_UNO)
  // Arduino Uno seems to have trouble with strftime that other larger MCUs don't have.
  // So instead of an easy and simple strftime, let's dump the timestamp here to show it's
  // at least correct. Parsing this into a time value is an exercise left to the user
  // (solution hint: use a better MCU than an Uno)
  Serial.print("timestamp=");
  Serial.println(t);
#else
  char buf[80];

  struct tm  ts;
  ts = *localtime(&t);

  // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
  strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
  Serial.println(buf);
#endif
}


// This is called when the service changes one of our attributes.
bool attrSetHandler(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  // We don't have any MCU attributes to be set by the service, so we just fall through here
  return true;
}

// This is called when either an Afero attribute has been changed via setAttribute or in response
// to a getAttribute call.
void attrNotifyHandler(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  //printAttribute("attrNotifyHandler", attributeId, valueLen, value);

  switch (attributeId) {
    case AF_SYSTEM_LINKED_TIMESTAMP:
      if (!timestamp_read) {
        linked_timestamp = *((int32_t *)value);
        Serial.print(F("ASR linked at: "));
        printTimeString((time_t)linked_timestamp);
        // TODO: If you have an RTC this would be a good place to set the clock time
      }
      if (!timestamp_read && linked_timestamp > 0 && utc_offset_change_time > 0) timestamp_read = true;
      break;
    case AF_SYSTEM_UTC_OFFSET_DATA:
      utc_offset_now         = value[1] << 8 | value[0];
      utc_offset_next        = value[7] << 8 | value[6];
      // The line below extracts the int32 timestamp from bytes 2-5 of the attribute
      utc_offset_change_time = *((int32_t *)(value + 2));
      if (utc_offset_change_time > 0) {
        Serial.print(F("Current TZ offset from UTC: "));
        Serial.print((float)utc_offset_now / 60);
        Serial.print(F("h, isDST: "));
        Serial.println(utc_offset_now > utc_offset_next);
        Serial.print(F("   Next TZ offset from UTC: "));
        Serial.print((float)utc_offset_next / 60);
        Serial.println(F("h"));
        Serial.print(F("Time Zone Change at: "));
        printTimeString((time_t)utc_offset_change_time);
        // TODO: If you have an RTC this would be a good place to set the clock time and/or timezone
      }
      if (!timestamp_read && linked_timestamp > 0 && utc_offset_change_time > 0) timestamp_read = true;
      break;

    case AF_APPLICATION_VERSION:
      if (valueLen > 1) {
        if ((uint16_t) (value[1] << 8 | value[0]) < 20577) {
          Serial.println(F("IMPORTANT: Your Modulo does not have new enough firmware to support"));
          Serial.println(F("this timestamp function. Please reboot your Modulo and let it update OTA"));
          Serial.println(F("to the latest firmware release."));
        }
      }
      break;

    case AF_SYSTEM_ASR_STATE:
      Serial.print(F("Module state: "));
      switch (value[0]) {
        case MODULE_STATE_REBOOTED:
          // if the ASR reboots, then we'll accept the next time data we get (since it'll be fresh)
          timestamp_read = false;
          Serial.println(F("Rebooted"));
          break;
        case MODULE_STATE_LINKED:
          Serial.println(F("Linked"));
          break;
        case MODULE_STATE_UPDATING:
          Serial.println(F("Updating"));
          break;
        case MODULE_STATE_UPDATE_READY:
          Serial.println(F("Update ready - rebooting"));
          aflib->setAttribute32(AF_SYSTEM_COMMAND, MODULE_COMMAND_REBOOT);
          break;
        default:
          break;
      }
      break;

    default:
      break;
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
   Debug Functions
 *                                                                                                  *
   Some helper functions to make debugging a little easier...
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

void printAttr8(const char *sourceLabel, const char *attrLabel, const uint8_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  char *buffer = getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  char intStr[6];
  strcat(buffer, itoa(*((uint8_t *)value), intStr, 10));
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

void printAttr32(const char *sourceLabel, const char *attrLabel, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  char *buffer = getPrintAttrHeader(sourceLabel, attrLabel, attributeId, valueLen);
  char intStr[12];
#if defined(ARDUINO_AVR_UNO)
  strcat(buffer, ltoa(*((int32_t *)value), intStr, 12));
#else
  strcat(buffer, itoa(*((int32_t *)value), intStr, 12));
#endif
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
    case AF_SYSTEM_LINKED_TIMESTAMP:
      printAttr32(label, "AF_SYSTEM_LINKED_TIMESTAMP", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_UTC_OFFSET_DATA:
      printAttrHex(label, "AF_SYSTEM_UTC_OFFSET_DATA", attributeId, valueLen, value);
      break;

    case AF_APPLICATION_VERSION:
      printAttrHex(label, "AF_APPLICATION_VERSION", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_ASR_STATE:
      printAttr8(label, "AF_SYSTEM_ASR_STATE", attributeId, valueLen, value);
      break;

    case AF_SYSTEM_REBOOT_REASON:
      printAttrStr(label, "AF_REBOOT_REASON", attributeId, valueLen, value);
      break;
  }
}
