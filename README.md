# Afero Secure Radio Library #

**Version 1.3.157**

## Welcome ##

This library implements the SPI and UART protocols used to communicate with the Afero Radio Module on development boards such as the Modulo. It provides a simple API for reading and writing attribute values from an Arduino sketch.

This version of the library support both the Arduino and Linux platforms.

### Supported Arduino Platforms ###

This version of the library has been tested against *UNO*, *Teensy*, *ATmega2560*. *Raspberry PI* support is included out currently out of sync with the Arduino version. This should be fixed in the next afLib release.

### Arduino Installation ###

* If you are familiar with using git, you can use it to clone this directory into your Arduino installation.
  From a command line, change directories to your Aduino "libraries" folder ("Documents/Arduino/libraries" for Mac, "My Documents/Ardino/libraries" for Windows) and clone this project with "git clone https://github.com/aferodeveloper/afLib.git"

* If you don't use git, you can download this project from the "Clone or Download" button at the top of the page, then select "Download ZIP".  Unzip the downloaded file "afLib-master.zip" and it will create a folder called "afLib-master". *Rename this folder* from "afLib-master" to just "afLib", then copy it to the Arduino/libraries folder. The folder must be named *afLib* and not *afLib-master* for the Arduino sotware to properly recognize it.

* After installation, restart the Arduino IDE if it was running.

### Ugrading an existing Arduino Installation ###

* If you've previously installed an older version of afLib, delete the old folder under Arduino/libaries and then install this new version via the installation instructions above.

### More Information ###

<http://developer.afero.io>

### Release Notes ###

afLib 1.3.153 3/14/18 Release Notes

* Fixed internal afLib FIFO queues that sometimes weren't FIFO
* Updated Modulo-2 profiles for example projects
* added getReason() call to get a reason code for why an update happened
* hide "fake" fuzzing traffic from MCU to keep MCU from having to process it
* updated debugging code in afBlink to current standards
* added example app to get linked timestamp and device UTC offset data (aflib_time_check)


afLib 1.2.114 7/25/17 Release Notes

* Reduce serial monitor baud rate to 9600 to match Arduino default
* Add UART support for the MCU interface
* Add a define at the top of the examples to allow you to specify SPI or UART
* Define RX/TX pins for the UART interface
* Add some helper functions for printing received system attributes
* Add code to reboot ASR when an update is pending
* Retry setAttribute operations until they are successful
* Update profiles with latest version of Afero Profile Editor to get new system attributes
* Define a common transport interface that is implemented by both UART and SPI interfaces
* Remove an obsolete update reason
* Add constants for the SYNC_REQUEST and SYNC_ACK messages
* Add a define to make it easy to turn on debugging of the transport layer
* Send the correct update reason in update messages to the ASR

afLib 0.9.217 2/23/17 Release Notes

* Renamed some dubiously-named methods for clarity:
  onAttrSet -> attrSetHandler
  onAttrSetComplete -> attrNotifyHandler

* Renamed a bunch of also dubiously-named internal methods and variable names
* Miscellaneous code formatting changes
* MOST developer applications should not require any changes, if you implement onAttrSet and onAttrSetComplete you'll need to change to the new names (see forum.afero.io for some code change notes)

afLib 5/26/16 Release Notes

* Retry the initial sync operation if it fails.
* Remove the old "Collision" message as it was just confusing.
* Print all attribute names and values at startup.
* Switch away from using the String object for debug printing to reduce memory.
* Use the "!Serial" trick to wait for Serial to startup.
* Fix a bug with bool types.
* Remove a bunch of used cargo code.
* Add some comments and rename some methods to make them more clear.

afLib 4/21/16 Release Notes

* Restore a missing paren in boebot_tank example.
* Move the example profiles back to the released version of the profile editor.

afLib 4/20/16 Release Notes

* Add two new examples that demonstrate how attributes can be manipulated either directly or through a transform.
* Fix a problem with initializing the `spi_ioc_transfer` structure under Linux that was causing errors for ceratin kernels.

afLib 3/24/16 Release Notes

* The library now supports a String of any length. In the previous releases, String support was unreliable. 
* The library was extended to support both Arduino and Raspberry Pi using a common code base.
* The library now always calls onSetAttributeComplete for an associated setAttribute call, regardless of where the attribute lives. Previously this function was not called for MCU attributes.
* The library now only allows one set/get attribute call to execute at any one time. Other calls are queued until they can be serviced.
* The setAttribute method names were changed to reflect the associated attribute type. C was not resolving the previous method signatures correctly.
* The Arduino board definitions were cleaned up, plus support was added for the ATMega2560.
