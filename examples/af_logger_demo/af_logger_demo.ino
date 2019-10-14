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

/*
   af_logger is a platform-independent logging system used internally within afLib.

   Use of af_logger in your own code is optional, but recommended. af_logger provides
   a standard interface for debugging output, so porting your Afero application from 
   one platform to another is as simple as changing two lines of platform-specific code
   to point to a different implementation. 

   Functions available in the arduino_logger platform-specific implementation:

   void arduino_logger_start(int baud_rate);
   void arduino_logger_stop();

   The Arduino implementation uses the default "Serial" console output, so all you have to
   give it is a baud rate for your console interface. Other platforms may require different
   start() and stop() parameters.

   The use of "_logger_stop()" is optional and only needed if you need to reuse the debug
   interface for some other use.

   Functions available in the platform-independent af_logger subsystem:

   void af_logger_print_value(int32_t val);
   void af_logger_print_buffer(const char* val);
   void af_logger_print_formatted_value(int32_t val, af_logger_format_t format);
   void af_logger_println_value(int32_t val);
   void af_logger_println_buffer(const char* val);
   void af_logger_println_formatted_value(int32_t val, af_logger_format_t format);

   "Format" is one of type:
   AF_LOGGER_BIN = 2
   AF_LOGGER_OCT = 8
   AF_LOGGER_DEC = 10
   AF_LOGGER_HEX = 16

   af_logger_print_value(val) is equivalent to af_logger_print_formatted_value(val, AF_LOGGER_DEC)
   
 */

#include "af_logger.h"
// The only platform specific include is this one
#include "arduino_logger.h"

void setup() {
  // The only platform specific function is this one
  arduino_logger_start(115200);

  af_logger_println_buffer("afLib af_logger logging subsystem demo");
  af_logger_println_buffer("--------------------------------------");
  af_logger_println_buffer("");   // prints a blank newline

  af_logger_print_buffer("The Ultimate Answer to Life, the Universe, and Everything is: ");
  uint8_t answer = 0x2a;
  af_logger_println_value(answer);
  af_logger_println_buffer("");

  af_logger_print_buffer("Party in octal like it's ");
  af_logger_println_formatted_value(1999, AF_LOGGER_OCT);
  af_logger_println_buffer("");

  af_logger_print_buffer("There are ");
  af_logger_print_formatted_value(24, AF_LOGGER_BIN);
  af_logger_println_buffer(" hours in a binary day.");
  af_logger_println_buffer("");

  uint32_t ph = 8675309;
  af_logger_print_value(ph);
  af_logger_print_buffer(" in hex is ");
  af_logger_println_formatted_value(ph, AF_LOGGER_HEX);
  af_logger_println_buffer("");  
  af_logger_println_buffer("fin");

  
}

void loop() { }
 
