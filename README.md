# Afero Secure Radio Arduino Library #

**afLib3 Version 1.1**

2019-Oct-14: afLib3 is now *deprecated*. Applications built against afLib3 will continue to work as they do now, but please use the latest release of afLib for all new development efforts.

## Welcome ##

This library implements the SPI and UART protocols used to communicate between an MCU and the Afero Secure Radio Module. It provides a simple API for reading and writing attribute values from an MCU application.

This version of the library, called afLib3, supercedes and replaces previous Afero libraries named afLib and afLib2.

afLib3 differs significantly from previous versions by implementing a new more logical API, adding support for other MCUs and operating systems, and reducing several callback routines into a single callback.

However, afLib3 is also API compatible with afLib2. Your code running on afLib2 should not need to be updated to use this library, though updating your code to use the new APIs is strongly recommended for future compatibility.

*Please Note:* This new version of the Afero MCU Library, written in C, is intended to replace the older C++ version called simply *afLib*. You can have both the afLib and afLib3 packages installed at the same time, they are distinct from one another. Please use *afLib3* for all new development, but *afLib* will remain available for compatibility with existing projects that use it.

AfLib3 is packaged as an Arduino-compatible library for ease of installing in the Arduino IDE. For other MCU platforms see the Afero Developer Github at https://github.com/aferodeveloper/.


### Arduino Installation ###

* If you are familiar with using git, you can use it to clone this directory into your Arduino installation.
  From a command line, change directories to your Aduino "libraries" folder ("Documents/Arduino/libraries" for Mac, "My Documents/Ardino/libraries" for Windows) and clone this project with "git clone https://github.com/aferodeveloper/afLib3.git"

* If you don't use git, you can download this project from the "Clone or Download" button at the top of the page, then select "Download ZIP". Unzip the downloaded file "afLib3-master.zip" and it will create a folder called "afLib3-master". *Rename this folder* from "afLib3-master" to just "afLib3", then copy it to the Arduino/libraries folder. The folder must be named *afLib3* and not *afLib3-master* for the Arduino sotware to properly recognize it.

* After installation, restart the Arduino IDE if it was running.

### Upgrading an existing Arduino Installation ###

* If you've previously installed an older version of afLib3, delete the old folder under Arduino/libaries and then install this new version via the installation instructions above.

### More Information ###

<http://developer.afero.io>

### Release Notes ###

afLib3 1.2.319 7/17/19 Release Notes

* afLib3 support for Modulo-1 was broken and would not talk to a properly connected ASR
* Renamed example profiles so "device name + device type" are a unique combination (Profile Editor complains if it's not)
* Profiles for Modulo-1B had LED on GPIO0 as Active Low (it's Active High)
* Re-added missing LICENSE.txt file
* Re-added arduino_transport_create_spi(CS_PIN) API inadvertently removed when adding arduino_transport_create_spi(CS_PIN,frame_length)

afLib3 1.1.306 1/07/19 Release Notes

* changed signatures for af_lib_asr_has_capability() and af_lib_send_set_response() for consistency
* Added ASR_STATE_RELINKED to indicate when ASR drops offline and reconnects.
* Greatly simplified afBlink example app.
* Fixed issue in example apps that caused Modulo-1 to not work properly.
* Updated example apps for new “MCU update all” behavior in Firmware R2.1.1.
* Removed reference to undefined RESET pin when using Arduino Uno.
* Created example app profiles for Modulo-1B.
* Removed obsoleted BoE-Bot example apps

