# Afero Secure Radio Library #

**Version 2.0**

2019-Oct-11: afLib2 is now fully end-of-life. Please use the latest release of afLib for all development efforts.

## Welcome ##

This library implements the SPI and UART protocols used to communicate with the Afero Radio Module on development boards such as the Modulo-1 and Modulo-2. It provides a simple API for reading and writing attribute values from an Arduino sketch.

This version of the library supports Arduino compatible MCUs. It has been tested on Arduino/Genuino Uno boards and the the PJRC Teensyduino 3.2 board, but should work fine on any Arduino compatible MCU as long as the SPI or UART pins are connected properly. 

*Please Note:* This new version of the Afero MCU Library, written in C, is intended to replace the older C++ version called simply *afLib*. You can have both the afLib and afLib2 packages installed at the same time, they are distinct from one another. Please use *afLib2* for all new development, but *afLib* will remain available for compatibility with existing projects that use it.

While afLib2 is packaged as an Arduino-compatible library, this major reason for this release is to simplify porting this library to other MCU platforms.


### Arduino Installation ###

* If you are familiar with using git, you can use it to clone this directory into your Arduino installation.
  From a command line, change directories to your Aduino "libraries" folder ("Documents/Arduino/libraries" for Mac, "My Documents/Ardino/libraries" for Windows) and clone this project with "git clone https://github.com/aferodeveloper/afLib2.git"

* If you don't use git, you can download this project from the "Clone or Download" button at the top of the page, then select "Download ZIP". Unzip the downloaded file "afLib2-master.zip" and it will create a folder called "afLib2-master". *Rename this folder* from "afLib2-master" to just "afLib2", then copy it to the Arduino/libraries folder. The folder must be named *afLib2* and not *afLib2-master* for the Arduino sotware to properly recognize it.

* After installation, restart the Arduino IDE if it was running.

### Upgrading an existing Arduino Installation ###

* If you've previously installed an older version of afLib2, delete the old folder under Arduino/libaries and then install this new version via the installation instructions above.

### More Information ###

<http://developer.afero.io>

### Release Notes ###

afLib2 2.0.204 3/23/18 Release Notes

* Initial release of C Afero MCU Library
* Port all example apps to use afLib2
* Full documentation available at https://developer.afero.io/API-Arduino

