/*
 * Afero Device Profile header file
 * Device Description:		4b2e8a30-a939-4f28-bd43-9e6881e703c6
 * Schema Version:	2
 */


#define ATTRIBUTE_TYPE_SINT8                                   2
#define ATTRIBUTE_TYPE_SINT16                                  3
#define ATTRIBUTE_TYPE_SINT32                                  4
#define ATTRIBUTE_TYPE_SINT64                                  5
#define ATTRIBUTE_TYPE_BOOLEAN                                 1
#define ATTRIBUTE_TYPE_UTF8S                                  20
#define ATTRIBUTE_TYPE_BYTES                                  21
#define ATTRIBUTE_TYPE_FLOAT32                                10

//region Service ID 1
// Attribute Servo1
#define AF_SERVO1                                              1
#define AF_SERVO1_SZ                                           2
#define AF_SERVO1_TYPE                     ATTRIBUTE_TYPE_SINT16

// Attribute Servo2
#define AF_SERVO2                                              2
#define AF_SERVO2_SZ                                           4
#define AF_SERVO2_TYPE                     ATTRIBUTE_TYPE_SINT32

// Attribute Accel_Attr
#define AF_ACCEL_ATTR                                          3
#define AF_ACCEL_ATTR_SZ                                       2
#define AF_ACCEL_ATTR_TYPE                 ATTRIBUTE_TYPE_SINT16

// Attribute Steer_Attr
#define AF_STEER_ATTR                                          4
#define AF_STEER_ATTR_SZ                                       2
#define AF_STEER_ATTR_TYPE                 ATTRIBUTE_TYPE_SINT16

// Attribute Transmission_Attr
#define AF_TRANSMISSION_ATTR                                   5
#define AF_TRANSMISSION_ATTR_SZ                                2
#define AF_TRANSMISSION_ATTR_TYPE          ATTRIBUTE_TYPE_SINT16

// Attribute Bootloader Version
#define AF_BOOTLOADER_VERSION                               2001
#define AF_BOOTLOADER_VERSION_SZ                               8
#define AF_BOOTLOADER_VERSION_TYPE         ATTRIBUTE_TYPE_SINT64

// Attribute Softdevice Version
#define AF_SOFTDEVICE_VERSION                               2002
#define AF_SOFTDEVICE_VERSION_SZ                               8
#define AF_SOFTDEVICE_VERSION_TYPE         ATTRIBUTE_TYPE_SINT64

// Attribute Application Version
#define AF_APPLICATION_VERSION                              2003
#define AF_APPLICATION_VERSION_SZ                              8
#define AF_APPLICATION_VERSION_TYPE        ATTRIBUTE_TYPE_SINT64

// Attribute Profile Version
#define AF_PROFILE_VERSION                                  2004
#define AF_PROFILE_VERSION_SZ                                  8
#define AF_PROFILE_VERSION_TYPE            ATTRIBUTE_TYPE_SINT64

// Attribute Security Enabled
#define AF_SECURITY_ENABLED                                60000
#define AF_SECURITY_ENABLED_SZ                                 1
#define AF_SECURITY_ENABLED_TYPE          ATTRIBUTE_TYPE_BOOLEAN

// Attribute Attribute ACK
#define AF_ATTRIBUTE_ACK                                   65018
#define AF_ATTRIBUTE_ACK_SZ                                    2
#define AF_ATTRIBUTE_ACK_TYPE              ATTRIBUTE_TYPE_SINT16

// Attribute Reboot Reason
#define AF_REBOOT_REASON                                   65019
#define AF_REBOOT_REASON_SZ                                   64
#define AF_REBOOT_REASON_TYPE               ATTRIBUTE_TYPE_UTF8S

// Attribute BLE Comms
#define AF_BLE_COMMS                                       65020
#define AF_BLE_COMMS_SZ                                       12
#define AF_BLE_COMMS_TYPE                   ATTRIBUTE_TYPE_BYTES

// Attribute SPI Enabled
#define AF_SPI_ENABLED                                     65021
#define AF_SPI_ENABLED_SZ                                      1
#define AF_SPI_ENABLED_TYPE               ATTRIBUTE_TYPE_BOOLEAN
//endregion
