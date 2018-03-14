/*
 * Afero Device Profile header file
 * Device Description:		d039c064-520a-436f-a3a5-83f01e9f78ec
 * Schema Version:	2
 */


#define ATTRIBUTE_TYPE_SINT8                                       2
#define ATTRIBUTE_TYPE_SINT16                                      3
#define ATTRIBUTE_TYPE_SINT32                                      4
#define ATTRIBUTE_TYPE_SINT64                                      5
#define ATTRIBUTE_TYPE_BOOLEAN                                     1
#define ATTRIBUTE_TYPE_UTF8S                                      20
#define ATTRIBUTE_TYPE_BYTES                                      21
#define ATTRIBUTE_TYPE_FIXED_16_16                                 6

//region Service ID 1
// Attribute Servo1
#define AF_SERVO1                                                  1
#define AF_SERVO1_SZ                                               2
#define AF_SERVO1_TYPE                         ATTRIBUTE_TYPE_SINT16

// Attribute Servo2
#define AF_SERVO2                                                  2
#define AF_SERVO2_SZ                                               2
#define AF_SERVO2_TYPE                         ATTRIBUTE_TYPE_SINT16

// Attribute Steer_Attr
#define AF_STEER_ATTR                                              3
#define AF_STEER_ATTR_SZ                                           2
#define AF_STEER_ATTR_TYPE                     ATTRIBUTE_TYPE_SINT16

// Attribute Accel_Attr
#define AF_ACCEL_ATTR                                              4
#define AF_ACCEL_ATTR_SZ                                           2
#define AF_ACCEL_ATTR_TYPE                     ATTRIBUTE_TYPE_SINT16

// Attribute Transmission_Attr
#define AF_TRANSMISSION_ATTR                                       5
#define AF_TRANSMISSION_ATTR_SZ                                    2
#define AF_TRANSMISSION_ATTR_TYPE              ATTRIBUTE_TYPE_SINT16

// Attribute Bootloader Version
#define AF_BOOTLOADER_VERSION                                   2001
#define AF_BOOTLOADER_VERSION_SZ                                   8
#define AF_BOOTLOADER_VERSION_TYPE             ATTRIBUTE_TYPE_SINT64

// Attribute Softdevice Version
#define AF_SOFTDEVICE_VERSION                                   2002
#define AF_SOFTDEVICE_VERSION_SZ                                   8
#define AF_SOFTDEVICE_VERSION_TYPE             ATTRIBUTE_TYPE_SINT64

// Attribute Application Version
#define AF_APPLICATION_VERSION                                  2003
#define AF_APPLICATION_VERSION_SZ                                  8
#define AF_APPLICATION_VERSION_TYPE            ATTRIBUTE_TYPE_SINT64

// Attribute Profile Version
#define AF_PROFILE_VERSION                                      2004
#define AF_PROFILE_VERSION_SZ                                      8
#define AF_PROFILE_VERSION_TYPE                ATTRIBUTE_TYPE_SINT64

// Attribute Command
#define AF_SYSTEM_COMMAND                                      65012
#define AF_SYSTEM_COMMAND_SZ                                       4
#define AF_SYSTEM_COMMAND_TYPE                 ATTRIBUTE_TYPE_SINT32

// Attribute ASR State
#define AF_SYSTEM_ASR_STATE                                    65013
#define AF_SYSTEM_ASR_STATE_SZ                                     1
#define AF_SYSTEM_ASR_STATE_TYPE                ATTRIBUTE_TYPE_SINT8

// Attribute Low Power Warn
#define AF_SYSTEM_LOW_POWER_WARN                               65014
#define AF_SYSTEM_LOW_POWER_WARN_SZ                                1
#define AF_SYSTEM_LOW_POWER_WARN_TYPE           ATTRIBUTE_TYPE_SINT8

// Attribute Linked Timestamp
#define AF_SYSTEM_LINKED_TIMESTAMP                             65015
#define AF_SYSTEM_LINKED_TIMESTAMP_SZ                              4
#define AF_SYSTEM_LINKED_TIMESTAMP_TYPE        ATTRIBUTE_TYPE_SINT32

// Attribute Reboot Reason
#define AF_SYSTEM_REBOOT_REASON                                65019
#define AF_SYSTEM_REBOOT_REASON_SZ                               100
#define AF_SYSTEM_REBOOT_REASON_TYPE            ATTRIBUTE_TYPE_UTF8S

// Attribute MCU Interface
#define AF_SYSTEM_MCU_INTERFACE                                65021
#define AF_SYSTEM_MCU_INTERFACE_SZ                                 1
#define AF_SYSTEM_MCU_INTERFACE_TYPE            ATTRIBUTE_TYPE_SINT8
//endregion
