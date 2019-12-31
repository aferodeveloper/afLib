# Aflib: the Afero Secure Radio Library #

**afLib4 Version 1.0**

## Welcome ##

This library implements the SPI and UART protocols used to communicate between an MCU and the Afero Secure Radio Module. It provides a simple API for reading and writing attribute values from an MCU application.

This version of the library, called afLib4, supercedes and replaces previous Afero libraries afLib3, afLib3, and afLib/afLib1.

afLib4 differs slightly from afLib3 by adding an "update reason" to MCU set attribute calls, and simplifying the "MCU Update All" attribute query into simpler, individual events that can be implemented in common boilerplate code instead of a lot of application-specific attribute handling. Also, a new example app is included which shows coding examples to handle the entirety of the afLib API and all the events you may receive from it.

afLib4 is also generally API compatible with afLib3. Your code running on afLib3 does not need to be updated right away and will continue to operate normally. However, as you update your applications it is strongly recommended to make the minr changes to port them to use afLib4 for future compatibility.

afLib is packaged as an Arduino-compatible library for ease of installing in the Arduino IDE. For other MCU platforms see the Afero Developer Github at https://github.com/aferodeveloper/.

## Supported Platforms ##

* afLib4 for Arduino-compatible MCUs is available at <https://github.com/aferodeveloper/afLib>.
* afLib4 for STM32 Arduino-compatible MCUs is available at <https://github.com/aferodeveloper/afLib-ArduinoSTM32>.
* afLib4 for Linux/macOS/generix *NIX hosts is available at <https://github.com/aferodeveloper/afLib-linux>.


### Arduino Installation ###

* If you are familiar with using git, you can use it to clone this directory into your Arduino installation.
  From a command line, change directories to your Aduino "libraries" folder ("Documents/Arduino/libraries" for Mac, "My Documents/Ardino/libraries" for Windows) and clone this project with "git clone https://github.com/aferodeveloper/afLib.git"

* If you don't use git, you can download this project from the "Clone or Download" button at the top of the page, then select "Download ZIP". Unzip the downloaded file "afLib4-master.zip" and it will create a folder called "afLib4-master". *Rename this folder* from "afLib4-master" to just "afLib4", then move it to the Arduino/libraries folder. The folder must be named *afLib4* and not *afLib4-master* for the Arduino sotware to properly recognize it.

* After installation, restart the Arduino IDE if it was running.

### Upgrading an existing Arduino Installation ###

* If you've previously installed an older version of afLib, delete the old folder under Arduino/libaries and then install this new version via the installation instructions above.

### More Information ###

<http://developer.afero.io>

### Release Notes ###

afLib4 1.0.37 10/07/19 Release Notes

* changed AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION to be informational only, no action required
* added update reason codes AF_LIB_SET_REASON_LOCAL_CHANGE and AF_LIB_SET_REASON_GET_RESPONSE to af_lib_set_attribute() APIs
* merged attribute_dumper example into full_api_example application
* removed mcu_update_all example application (no longer needed)
* code simplification and fixes to example apps
* Created example app profiles for Plumo-2D

