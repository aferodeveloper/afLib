/**
 * Copyright 2018 Afero, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef AF_MCU_OTA_H
#define AF_MCU_OTA_H

#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

#define AF_MCU_OTA_INFO         1301
#define AF_MCU_OTA_TRANSFER     1302

/** Useful macro to convert an MCU OTA type to it's version attribute */
#define AF_MCU_TYPE_TO_VERSION_ATTRIBUTE(x)     (2000 + x)
/** The offset to respond with in the transfer attribute when the MCU wants the ASR to stop sending it data */
#define AF_MCU_OTA_STOP_TRANSFER_OFFSET         0xFFFFFFFF

/**
 * The various MCU OTA states used when delivering the binary data to the MCU.  As an MCU developer it will be useful to understand the flow and
 * what's expected at each state.  All multi-byte data types are in little endian.  Below is how a typical OTA will appear to the MCU:
 *
 * Step 1.  The ASR will send the af_ota_info_t data down the AF_MCU_OTA_INFO attribute.  The state will be set to AF_OTA_TRANSFER_BEGIN and the
 *          begin_info will contain the OTA type and the size of the binary data to be delivered.  At this point the MCU should save off the OTA type
 *          since it will be needed in a later step.  The MCU should also do whatever is needed for it to prepare to receive an OTA for the specific OTA type.
 *          If the MCU can handle getting the OTA data at this moment then it should send back the same af_ota_info_t data via the af_lib_set_attribute_bytes() call.
 *          If the MCU can not handle getting the OTA data at this moment it should set the af_ota_info_t state to AF_OTA_IDLE and then send back the af_ota_info_t
 *          data via the af_lib_set_attribute_bytes() call - this will effectively make the ASR abandon the MCU OTA.
 *
 * Step 2.  If the MCU wants the OTA to progress the next thing the ASR will send is the OTA data via a set on the AF_MCU_OTA_TRANSFER attribute.  The format
 *          for this attribute is the first 4 bytes are always the offset of the data, followed by the data itself - up to the size of the attribute specified in the profile.
 *          Upon consuming the OTA data, the MCU needs to tell the ASR what the next offset of data it expects.  This is accomplished by a call to af_lib_set_attribute_bytes()
 *          for the AF_MCU_OTA_TRANSFER attribute with a data length of 4 bytes (specifying the next offset requested).  There's a special offset value, AF_MCU_OTA_STOP_TRANSFER_OFFSET,
 *          that the MCU can use to tell the ASR to stop sending it the OTA bytes if the MCU wants to end the transfer prematurely.  The process in this step will repeat until the entire
 *          OTA data has been delivered to the MCU at which point the MCU should send the AF_MCU_OTA_STOP_TRANSFER_OFFSET value back to the ASR.
 *
 * Step 3.  After receiving the entire OTA data payload the MCU needs to SHA the image (via the supplied af-lib code) and send that value back to the ASR via the AF_MCU_OTA_INFO attribute.
 *          The attribute data should be the af_ota_info_t structure with the state set to AF_OTA_VERIFY_SIGNATURE and the verify_info structure filled in.
 *
 * Step 4.  The ASR will verify the signature of the image and respond via an attribute set on the AF_MCU_OTA_INFO attribute.  For a failed signature the state will be AF_OTA_FAIL
 *          at which point the MCU should discard the downloaded OTA data.  For a successful signature check the state will be AF_OTA_APPLY and the apply_info structure will contain
 *          the Afero version number to be reported after the OTA data has been applied.  Upon the MCU successfully applying the OTA data it should set the AF_MCU_OTA_INFO attribute
 *          with the af_ota_info_t state set to AF_OTA_IDLE.  After the MCU is satisfied with the OTA data it needs to report to the Afero server that it has successfully
 *          consumed the data.  This is accomplished by setting the version attribute for the MCU OTA type using the AF_MCU_TYPE_TO_VERSION_ATTRIBUTE macro and the OTA type
 *          saved from Step 1 and the version specified in the apply_info structure.
 *
 */
typedef enum {
    AF_OTA_IDLE,
    AF_OTA_TRANSFER_BEGIN,
    AF_OTA_TRANSFER_END,
    AF_OTA_VERIFY_SIGNATURE,
    AF_OTA_APPLY,
    AF_OTA_FAIL,
} af_ota_state_t;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#pragma GCC diagnostic ignored "-Wpacked"
typedef struct {
    uint16_t ota_type;
    uint32_t size;
    uint64_t version_id;
} __attribute__ ((__packed__)) af_ota_begin_info_t;

typedef struct {
    uint8_t sha[32];
} __attribute__ ((__packed__)) af_ota_verify_info_t;

typedef struct {
    uint64_t  version_id;
} __attribute__ ((__packed__)) af_ota_apply_info_t;

typedef struct {
    uint8_t state; // One of the af_ota_state_t values
    union {
        af_ota_begin_info_t begin_info;    // Valid for AF_OTA_TRANSFER_BEGIN state
        af_ota_verify_info_t verify_info;  // Valid for OTA_VERIFY_SIGNATURE state
        af_ota_apply_info_t apply_info;    // Valid for OTA_APPLY state
    } info;
} __attribute__ ((__packed__)) af_ota_info_t;

typedef struct {
    uint32_t offset;
    uint8_t data[0];
} __attribute__ ((__packed__)) af_ota_xfer_t;
#pragma GCC diagnostic pop

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* AF_MCU_OTA_H */

