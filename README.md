# Afero Secure Radio Library #

**Version 0.9.94**

## Welcome ##

This library implements the SPI protocol used to communicate with the Afero Radio Module on development boards such as the Modulo. It provides a simple API for reading and writing attribute values from an Arduino sketch.

This version of the library support both the Arduino and Linux platforms.

### Supported Arduino Platforms ###

This version of the library has been tested against *UNO*, *Teensy*, *ATmega2560* and *Raspberry PI*.

### Arduino Installation ###

* To install afLib for the first time, follow the instructions for *Importing a .zip Library* on the <http://arduino.cc> site.
* To update an existing installation with a new version, follow the instructions for *Manual installation*.

### More Information ###

<http://developer.afero.io>

### Release Notes ###

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
