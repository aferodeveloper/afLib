/**
   Copyright 2019 Afero, Inc.

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
  This application is intended to show every possible event that can be sent
  from ASR, with commentary and suggestions on how to handle the events. The
  intended purpose of this app is to act as a "compilable API doc" so that you
  can use handler code from this application where it may be needed in your own
  code. It's unlikely that your application would need to manage every one of
  these events, and the rarer or more obscure events will be noted.

  For an MCU, this code will need a slightly beefy processor - as written it's
  intended to run on a Teensyduino or some decently-memoried and decently-flashed
  processor.

*/

/* API exercises included in this application:
 *  
 *  At boot time:
 *  Test Pass A: Accept the default value of an MCU attribute from a Profile
 *  Test Pass B: Respond to ASR request for an MCU attribute value at boot
 *  
 *  At run time:
 *  Test Step 1: successful af_lib_set_attribute() on non-MCU attribute
 *  Test Step 2: unsuccessful af_lib_set_attribute() on non-MCU attribute
 *  Test Step 3: successful af_lib_get_attribute() on an attribute
 *  Test Step 4: unsuccessful af_lib_get_attribute() on an attribute
 *  Test Step 5: successfully change MCU Attribute from mobile app (interaction required)
 *  Test Step 6: unsuccessfully change MCU Attribute from mobile app (interaction required)
 */



// Because of high memory usage, this example app is written for a Teensy Arduino-
// compatible device and for the sake of simplifying some of the code down to
// relevant API examples, some of the usual example app functionality is removed.
// See other example apps for various other set-up options.

// afLib includes
#include "af_lib.h"
#include "af_command.h"
#include "af_mcu_ota.h"
#include "af_module_commands.h"
#include "af_module_states.h"
#include "af_msg_types.h"
#include "af_utils.h"
#include "af_logger.h"
#include "arduino_logger.h"
#include "arduino_transport.h"
#include "arduino_spi.h"
#include "arduino_uart.h"

#include "debugprints.h"
#include "attribute_db.h"

// These profiles include attributes used in some of the example code
//#include "profile/full_api_example_Modulo-1_UART/device-description.h"          // For Modulo-1 UART
//#include "profile/full_api_example_Modulo-1B_UART/device-description.h"         // For Modulo-1B UART
//#include "profile/full_api_example_Modulo-2_UART/device-description.h"          // For Modulo-2 UART
#include "profile/full_api_example_Plumo-2D_UART/device-description.h"          // For Plumo-2D UART

// Teensy 3.2 pins - we just use UART in this app.
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots.
#define RX_PIN                    7
#define TX_PIN                    8
// For Modulo-1 UART speed is fixed at 9600.
// For Modulo-2 the UART_BAUD_RATE here must match the UART speed set in Profile Editor.
#define UART_BAUD_RATE            9600

af_lib_t* af_lib = NULL;
volatile bool asrReady = false;          // If false, we're waiting on AF_MODULE_STATE_INITIALIZED; if true, we can communicate with ASR.
volatile bool asrRebootPending = false;  // If true, a reboot is needed, e.g. if we received an OTA firmware or Profile update.

// For our test application here, keep a count of the test steps so we don't run them all at once.
int lastStep = 0;  // Last test completed.
int currStep = 1;  // Current test running.
uint8_t currAttrValue = 0; // To hold the value of our test MCU attribute.

// Callback executed any time ASR has information for the MCU.
void attrEventCallback(const af_lib_event_type_t eventType,
                       const af_lib_error_t error,
                       const uint16_t attributeId,
                       const uint16_t valueLen,
                       const uint8_t* value) {

// Some of these callback events can get VERY chatty.
// (I'm talkin' to you, AF_LIB_EVENT_ASR_NOTIFICATION!)
// So if you want to see program flow with a little less spam, leave this definition commented out.
// If you want to see every attribute that ASR sends your application, uncomment it.
// Its useful to see this traffic in all it's volume and glory at some point to get familiar with it.

//#define VERBOSE 1

  switch (eventType) {
    // Two events mostly for debugging/development use
    case AF_LIB_EVENT_UNKNOWN:
      af_logger_print_buffer("AF_LIB_EVENT_UNKNOWN                         ");
      printAttribute(attributeId, valueLen, value, error);
      
      // This event should, in theory, "Never Happen", but if you're porting afLib to a
      // different platform, it might be worth checking this event at least while
      // testing.
      break;

    case AF_LIB_EVENT_COMMUNICATION_BREAKDOWN:
      af_logger_print_buffer("AF_LIB_EVENT_COMMUNICATION_BREAKDOWN         ");
      printAttribute(attributeId, valueLen, value, error);

      // This event is sent from afLib when it gives up trying to communicate with ASR.
      // In normal circumstances this event doesn't occur unless there's a significant
      // failure of the afLib-to-ASR communication. During development, common reasons why you
      // might receive this event are:
      // - Application and Profile MCU communications mismatch (SPI on one end, UART on the other)
      // - UART speed or parameter mismatch between application and ASR
      // - ASR not powered on, not connected to the MCU properly
      // - Incorrect GPIO pins defined/ASR connected to wrong GPIO pins
      // Your console output will display the message "Sync Retry" multiple times as afLib attempts
      // to establish communications with ASR. Your application will get this event when those
      // retries are exhausted.

      // Handling this event in a production device could perhaps do something like inform them that
      // ASR (or the module it's configured in) might be failing and to call for support. Outside of
      // misconfigurations during development, the only time you should see this event in a
      // production device is if ASR hardware is failing or doesn't have power. If this event
      // occurs, resetting ASR is worth a try before giving up, but it is unlikely that a reset
      // will re-establish communications, in most cases. 
      break;


    // The next handlers are for different af_lib_set_attribute() situations.
    case AF_LIB_EVENT_ASR_SET_RESPONSE:
      af_logger_print_buffer("AF_LIB_EVENT_ASR_SET_RESPONSE                ");
      printAttribute(attributeId, valueLen, value, error);

      // When you call af_lib_set_attribute() on any NON-MCU attribute, such
      // as any ASR GPIO attributes, this event will be returned with the value of the
      // attribute in response. It's possible that ASR might reject your request
      // (e.g. if you tried to change a read-only attribute or something like that)
      // so the value returned in this event might NOT actually be the value you
      // tried to set. Check the value returned here and if it's different from
      // what you asked for, make sure you handle that possibility. You will get
      // an af_error_t response code (https://developer.afero.io/afLibErrors) and
      // the value will usually be the previous value of the attribute before you tried
      // to set it.

      // Another way to think of it is that the MCU does not "own" any non-MCU
      // attributes, so calling set_attribute() on one is a request, not a
      // command, and it's perfectly acceptable for ASR to say "no" to the request.

      // We received a response to our test request, so run the next test:
      if (currStep == 2) currStep++; // Test 2 response comes here.
      if (currStep == 1) currStep++; // Test 1 response comes here.
      
      break;

    case AF_LIB_EVENT_MCU_SET_REQ_SENT:
      af_logger_print_buffer("AF_LIB_EVENT_MCU_SET_REQ_SENT                ");
      printAttribute(attributeId, valueLen, value, error);

      // When you call af_lib_set_attribute on an MCU attribute, you will get this
      // event in response when afLib has sent your request to ASR (which,
      // subsequently, will send it to the Afero Cloud). This event just informs you
      // when the request has left afLib's internal work queue and has made it to 
      // ASR.
      // When ASR processes your request, you'll receive AF_LIB_EVENT_ASR_NOTIFICATION
      // if the request was successful, or AF_LIB_EVENT_MCU_SET_REQ_REJECTION if it was not.
      // In most cases, this event isn't very interesting to your application and can
      // be ignored. The success or failure events are far more important.
      break;

    case AF_LIB_EVENT_MCU_SET_REQ_REJECTION:
      af_logger_print_buffer("AF_LIB_EVENT_MCU_SET_REQ_REJECTION           ");
      printAttribute(attributeId, valueLen, value, error);

      // If, for some reason, ASR is unable to set an MCU attribute that you told it
      // to set, you will receive this event in response, with an af_error_t return code
      // explaining what happened. If this event is returned, the attribute value returned
      // is the previous value of the attribute before you tried to set it.

      // In contrast to the comments in AF_LIB_EVENT_ASR_SET_RESPONSE above, in this case
      // the MCU "owns" MCU attributes and af_lib_set_attribute() is more of a command than
      // a request, in that ASR won't reject an MCU attribute update for any reason other
      // than a technical inability to set it. (For example, MCU attributes are *never*
      // "read only" to the MCU.)
      break;

    case AF_LIB_EVENT_MCU_SET_REQUEST:
      af_logger_print_buffer("AF_LIB_EVENT_MCU_SET_REQUEST                 ");
      printAttribute(attributeId, valueLen, value, error);

      // You will receive this event when ASR wants you to change the value of an MCU attribute.
      // This is a common event for any MCU attributes that are controllable from the Cloud, such as
      // a user's changing device state via the mobile app. Since the MCU "owns" MCU attributes, this
      // event from ASR is a request, and it is possible for the MCU to refuse the request. In
      // either case, you *must* handle this event with a call to af_lib_send_set_response() with the
      // set_succeeded option to "true" if you accepted the request to your satisfaction, and "false"
      // if you did not. You must return the current value of the MCU attribute back in the
      // af_lib_send_set_response() call, though this value may be different than the value requested
      // in the initial request.

      // The use of "set_succeeded" is a bit of a misnomer - on first glance it would seem that 
      // if you were able to set the attribute requested by the ASR, then you should return "true", but
      // if you were unable to set the requested value, you should return "false". This is not
      // strictly the case.

      // If your application is able to process the request AT ALL, then you should return "true"
      // regardless of whether you used the requested value or not. Some examples of a "successful"
      // request would be:
      // - The MCU requests you set an attribute to "5", and you accepted the request and set it to "5".
      //   This is by far the most common case.
      // - The MCU requests you set an attribute to "11", but you only allow a range of 1-10.
      //   You altered the requested value, but still changed it.
      // - The MCU requests you set an attribute to "5" and it's current value is already "5".
      //   Even though you didn't have to do anything, the request was still successful.

      // If your application is unable to process the request, then you should return "false" along
      // with the value of the attribute that you are using. In practice, it turns out that returning
      // set_succeeded "false" should be an uncommon operation, for example, if there were a phsyical
      // reason why you couldn't handle a request. For example, if the request was to change the state
      // of a physical piece of hardware in your device, and you were unable to do so, then returning
      // "false" here be proper.

      // reject the change
      if (currStep == 6) {
        // Dont accept the passed value for currAttrValue (compare to step 5 below).
        af_lib_send_set_response(af_lib, AF_MCU_ATTRIBUTE, false, valueLen, &currAttrValue);
        currStep++;
      }
      // Accept the change.
      if (currStep == 5) {
        currAttrValue = *(uint8_t *)value;
        af_lib_send_set_response(af_lib, AF_MCU_ATTRIBUTE, true, valueLen, &currAttrValue);
        currStep++;
      }
break;

    case AF_LIB_EVENT_MCU_SET_REQUEST_RESPONSE_TIMEOUT:
      af_logger_print_buffer("AF_LIB_EVENT_MCU_SET_REQUEST_RESPONSE_TIMEOUT");
      printAttribute(attributeId, valueLen, value, error);

      // afLib will send this event if an af_lib_send_set_response() call has not been returned
      // for a previous AF_LIB_EVENT_MCU_SET_REQUEST event within AF_LIB_SET_RESPONSE_TIMEOUT_SECONDS.
      // This either indicates that the AF_LIB_SET_RESPONSE_TIMEOUT_SECONDS time is too short for some
      // attribute set operations or there's a logic error on the MCU and a bug that should be fixed.
      // The latter reason (MCU logic error) is by far the most common reason for seeing this event
      // during development, and should be extremely rare if encountered at all, in a production device.
      
      // This event is informational only. When you receive it, afLib will inform the server that
      // the last AF_LIB_EVENT_MCU_SET_REQUEST event for this attribute has failed - it's equivalent
      // to you sending af_lib_send_set_response() with a set_succeeded flag of "false" and the value
      // of the attribute before the AF_LIB_EVENT_MCU_SET_REQUEST was issued.
      break;



    // The next handlers are for different af_lib_get_attribute() situations.
    case AF_LIB_EVENT_MCU_GET_REQUEST:
      af_logger_print_buffer("AF_LIB_EVENT_MCU_GET_REQUEST                 ");
      printAttribute(attributeId, valueLen, value, error);

      // You will receive this event when ASR needs to know the value of an MCU attribute.
      // Since you "own" MCU attributes, ASR considers you the authoritative source for
      // the proper value and will, in some cases, ask you for the value of an attribute so
      // it can be sure the rest of the Cloud platform knows the correct value.

      // While this event can (in theory) happen any time, you will receive this event for
      // every MCU attribute when your application boots and ASR connects to the network.

      // To handle this event, all you need to do is call af_lib_set_attribute() on the
      // attrId passed here, the current value, and the reason code of AF_LIB_SET_REASON_GET_RESPONSE.

      // Because the attrId is numeric, and attribute names are #defines in the device-description.h
      // header, mapping the attrId back to a name (so you can map it back to an actual variable
      // of yours) is likely best handled with a switch/case block checking for the attribute by name.
      // This is a common structure for afLib handlers, and many of the event handlers in this
      // callback require some similar kind of structure:

switch (attributeId) {
        case AF_MCU_ATTRIBUTE:
          af_logger_println_buffer("Test Step B: ASR wants to know the value of AF_MCU_ATTRIBUTE");
          af_lib_set_attribute_8(af_lib, AF_MCU_ATTRIBUTE, currAttrValue, AF_LIB_SET_REASON_GET_RESPONSE);
          break;
      }

      break;

    case AF_LIB_EVENT_GET_RESPONSE:
      af_logger_print_buffer("AF_LIB_EVENT_GET_RESPONSE                    ");
      printAttribute(attributeId, valueLen, value, error);

      // When you call af_lib_get_attribute() on any attribute, the returned result will
      // come back to you in this event. af_lib_get_attribute(), like all afLib requests, are queued and
      // sent to ASR and the Cloud asynchronously.

      // It's fairly uncommon this event will need handling. As far as MCU attributes go, your application
      // will always know the correct MCU attribute values. For non-MCU attributes, if there are any
      // that your application is listening for, handling them in AF_LIB_EVENT_ASR_NOTIFICATION will notify you
      // of the current value at boot time and again whenever the attribute changes. So watching those
      // attributes in that event hander will almost completely eliminate the need for specifically
      // having to request a value via af_lib_get_attribute(). Getting a specific attribute via this method
      // is useful when you want to know a single point-in-time value of a non-MCU attribute but don't want
      // the AF_LIB_EVENT_ASR_NOTIFICATION handler to listen for all changes to it.

      // We received a response to our test request, so run the next test:
      if (currStep == 4) currStep++; // Test 4 response comes here.
      if (currStep == 3) currStep++; // Test 3 response comes here.

      break;

    case AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION:
      af_logger_print_buffer("AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION        ");
      printAttribute(attributeId, valueLen, value, error);

      // This is an informational event sent to the MCU at boot time to inform you if there are
      // any MCU attributes with "default values" defined in the device Profile. A default value can
      // be defined for an attribute in the Profile, which can then be used by your application
      // when you don't have a known value for it. Defining application defaults can be
      // useful for setting your application's behavior when a user onboards your device;
      // because the user wouldn't have set any preferences for the device's settings (as it's
      // new to them), you can provide your application with the default settings that you want to
      // have "out of the box".

      // You will receive this event for all MCU attributes that have default values in the Profile
      // BEFORE you will ever receive a AF_LIB_EVENT_MCU_GET_REQUEST event asking you for the current
      // value of the attribute. In that case you can then return the proper value for the attribute,
      // or have the default sent to you here, as applicable.

      switch (attributeId) {
        case AF_MCU_ATTRIBUTE:
          af_logger_print_buffer("Test Step A: ASR says the default value for AF_MCU_ATTRIBUTE should be: ");
          currAttrValue = *(uint8_t *)value;
          af_logger_println_value(currAttrValue);
          break;
      }
 

      break;

    case AF_LIB_EVENT_ASR_NOTIFICATION:
#if defined(VERBOSE)
      af_logger_print_buffer("AF_LIB_EVENT_ASR_NOTIFICATION                ");
      printAttribute(attributeId, valueLen, value, error);
#endif
      // AF_LIB_EVENT_ASR_NOTIFICATION is where most of your afLib attribute processing will occur.
      // This event is called every time *any* attribute changes value, and at boot time, every
      // attribute that ASR knows about will get sent to you in this event. All readable attributes
      // defined in the list at https://developer.afero.io/AttrRegistry will be sent to you, but you
      // only need to "listen for" and act on any attributes that your application cares about. 
      // Other than your own MCU attributes, the only attribute that your application is required to
      // handle is AF_SYSTEM_ASR_STATE. This event is critical to your application's knowing the state
      // of ASR and whether or not you can communicate with it, and whether or not you need to reboot
      // ASR for one of several reasons.


      switch (attributeId) {

        case AF_SYSTEM_ASR_STATE:
          af_logger_print_buffer("ASR state: ");
          switch (value[0]) {
            case AF_MODULE_STATE_REBOOTED:
              af_logger_println_buffer("Rebooted");
              asrReady = false;
              break;

            case AF_MODULE_STATE_LINKED:
              af_logger_println_buffer("Linked");
#if AF_BOARD == AF_BOARD_MODULO_1
              // For Modulo-1 this is the last connected state you'll get.
              asrReady = true;
#endif
              break;

            case AF_MODULE_STATE_UPDATING:
              af_logger_println_buffer("Updating");
              break;

            case AF_MODULE_STATE_UPDATE_READY:
              af_logger_println_buffer("Update ready - reboot needed");
              asrRebootPending = true;
              break;

            case AF_MODULE_STATE_INITIALIZED:
              af_logger_println_buffer("Initialized");
              asrReady = true;
              break;

            case AF_MODULE_STATE_RELINKED:
              af_logger_println_buffer("Relinked");
              break;

            case AF_MODULE_STATE_FACTORY_RESET:
              // We don't really have anything to clean up since we don't persist any stuff so all we need to do is reboot the ASR
              af_logger_println_buffer("Factory reset - rebooting");
              asrRebootPending = true;
              break;

            default:
              af_logger_print_buffer("Unexpected state: "); af_logger_println_value(value[0]);
              break;
          }
          break;


        default:
          break;
      }
      break;


    default:
      af_logger_println_buffer("OOPS: default handler, we weren't looking for this one!");
      // Another "can't happen" event - in this case afLib has returned an eventType outside the
      // range of expected events. If you're porting afLib to a new platform, a message here
      // might be useful for debugging, but beyond that it is acceptable to ignore any event that
      // makes it here, since you wouldn't know how to respond to it.
      break;
  }
}

void rebootAsr(bool force) {
  // Forcibly reset ASR if we know we know it's in a bad state.
  if (force) {
    af_logger_println_buffer("rebootAsr: resetting ASR");
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, 0);
    delay(250);
    digitalWrite(RESET, 1);
    // This leaves the reset pin pulled high when unused.
    pinMode(RESET, INPUT_PULLUP);
    asrReady = false;
    asrRebootPending = false;
  }
  else {
    // Politely ask ASR to reboot and make sure we don't have any pending requests first.
    af_logger_print_buffer("rebootAsr: rebooting ASR... rc=");
    af_lib_sync(af_lib); // This may take a little time if there's pending requests, but usually it's pretty quick.
    af_lib_error_t rc = af_lib_set_attribute_32(af_lib, AF_SYSTEM_COMMAND, AF_MODULE_COMMAND_REBOOT, AF_LIB_SET_REASON_LOCAL_CHANGE);
    af_logger_println_value(rc);
    // If the command is queued, stop trying.
    if (rc == AF_SUCCESS) {
      asrReady = false;
      asrRebootPending = false;
    }
  }

}

void af_delay(long delayms) {
  // Becuase afLib needs processing time even if our own application doesn't, we can't really
  // use any long-timer delays as we would in other apps.

  // This is a platform-dependent millisecond timer call -
  // for Arduino it's the same as plain ol' millis().
  long then = af_utils_millis() + delayms;
  while (af_utils_millis() <= then) {
    af_lib_loop(af_lib);
  }
}


void setup() {
  // Set up the logging subsystem - https://developer.afero.io/afLibLogging
  arduino_logger_start(115200);

    // Set up reset pin
    pinMode(RESET, INPUT_PULLUP);

  // Force a reset of ASR since at boot we can't be sure what state it's in.
  rebootAsr(true);

  // If you want SPI, look at the other example apps, SPI vs UART protocol is selected via
  // different arduino_transport_create() calls; once the af_lib object is created, your app
  // doesn't have to worry about which protocol is being used.
  af_logger_println_buffer("setup: Connecting to ASR via UART");
  af_transport_t *platformUART = arduino_transport_create_uart(RX_PIN, TX_PIN, UART_BAUD_RATE);
  af_lib = af_lib_create_with_unified_callback(attrEventCallback, platformUART);

  // Create our attribute database so we can display ASR attributes and decode them.
  createAttrDB();
  isAsr1(AF_BOARD == AF_BOARD_MODULO_1);

}

void loop() {
  // ALWAYS give the afLib state machine time to do its work - avoid blind delay() calls
  // of any length; instead, repeat calls to af_lib_loop() while waiting for whatever it is
  // you're waiting for to happen. Some afLib operations take multiple loop() calls to complete.
  af_lib_loop(af_lib);

  // Until the ASR is completely initialized, we shouldn't talk to it
  // Since we do't really need to do anything if it's not ready we just exit loop() altogether.
  // You could do some of your own application's processing here as needed
  if (!asrReady) return;

  // We can talk to ASR from here on down. First, some simple housekeeping:

  // If we need to reboot ASR (e.g. after an OTA firmware update), do it now.
  if (asrRebootPending) {
    rebootAsr(false);  // reboot cleanly, don't force it
    // FIXME - we should force a reboot if this repeatedly fails.
  }

  // Housekeeping done, now let's run our actual MCU code. Run through some
  // API calls and follow their results:

  if (lastStep == currStep) return; // At this point we're still waiting on the last test to complete.

  af_delay(5000); // Wait for previous events to finish up.

  af_logger_print_buffer("Test Step ");
  af_logger_print_value(currStep);
  af_logger_print_buffer(": ");
  af_lib_error_t rc;
  
  switch (currStep) {
    case 1:
      af_logger_print_buffer("af_lib_set_attribute() on non-MCU attribute ");
      af_logger_print_value(AF_MODULO_LED);
      af_logger_print_buffer(" (successful), rc=");
      rc=af_lib_set_attribute_16(af_lib, AF_MODULO_LED, 0, AF_LIB_SET_REASON_LOCAL_CHANGE);
      af_logger_println_value(rc);
      if (rc == 0) {
        af_logger_println_buffer("Expect rc=0 AF_LIB_EVENT_ASR_SET_RESPONSE shortly");
      }
      lastStep = currStep;
      break;

    case 2:
      af_logger_print_buffer("af_lib_set_attribute() on non-MCU attribute ");
      af_logger_print_value(AF_MODULO_BUTTON);
      af_logger_print_buffer(" (unsuccessful), rc=");
      rc=af_lib_set_attribute_32(af_lib, AF_MODULO_BUTTON, 1, AF_LIB_SET_REASON_LOCAL_CHANGE);
      af_logger_println_value(rc);
      if (rc == 0) {
        af_logger_println_buffer("Expect rc<0 AF_LIB_EVENT_ASR_SET_RESPONSE shortly");
      }
      lastStep = currStep;
      break;

    case 3:
      af_logger_print_buffer("af_lib_get_attribute() on attribute ");
      af_logger_print_value(AF_MODULO_LED);
      af_logger_print_buffer(" (successful), rc=");
      rc=af_lib_get_attribute(af_lib, AF_MODULO_LED);
      af_logger_println_value(rc);
      if (rc == 0) {
        af_logger_println_buffer("Expect rc=0 AF_LIB_EVENT_GET_RESPONSE shortly");
      }
      lastStep = currStep;
      break;

    case 4:
      af_logger_print_buffer("af_lib_get_attribute() on attribute ");
      af_logger_print_value(65021);
      af_logger_print_buffer(" (unsuccessful), rc=");
      rc=af_lib_get_attribute(af_lib, 65021);
      af_logger_println_value(rc);
      if (rc == 0) {
        af_logger_println_buffer("Expect rc<0 AF_LIB_EVENT_GET_RESPONSE shortly");
      }
      lastStep = currStep;
      break;

    case 5:
      af_logger_println_buffer("Change MCU Attribute from mobile app (successful)");
      af_logger_println_buffer("Expect AF_LIB_EVENT_MCU_SET_REQUEST  with new value");
      af_logger_println_buffer("Expect AF_LIB_EVENT_MCU_SET_REQ_SENT with new value");
      af_logger_println_buffer("Change attribute from mobile app now (Paused)");
      lastStep = currStep;
      break;

    case 6:
      af_logger_println_buffer("Change MCU Attribute from mobile app (unsuccessful)");
      af_logger_println_buffer("Expect AF_LIB_EVENT_MCU_SET_REQUEST  with new value");
      af_logger_println_buffer("Expect AF_LIB_EVENT_MCU_SET_REQ_SENT with previous value");
      af_logger_println_buffer("Change attribute from mobile app now (Paused)");
      lastStep = currStep;
      break;

    


    default:
      af_logger_println_buffer("Done!");
      lastStep = currStep;
      break;
  }

  
}


