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

/**
 * afLib public interface
 *
 * This file defines everything your application should need for communicating with the ASR.
 * Is there is anything missing from this file, please post a request on the developer forum and we will see what
 * we can do.
 */
#ifndef AF_LIB_H
#define AF_LIB_H

#include "af_transport.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define AF_SYSTEM_ASR_STATE_ATTR_ID                                    65013

typedef enum {
    AF_SUCCESS                 =  0,   // Operation completed successfully
    AF_ERROR_UNKNOWN           = -1,   // An unknown error has occurred
    AF_ERROR_BUSY              = -2,   // Request already in progress, try again
    AF_ERROR_INVALID_COMMAND   = -3,   // Command could not be parsed
    AF_ERROR_QUEUE_OVERFLOW    = -4,   // Queue is full
    AF_ERROR_QUEUE_UNDERFLOW   = -5,   // Queue is empty
    AF_ERROR_INVALID_PARAM     = -6,   // Bad input parameter
    AF_ERROR_NOT_SUPPORTED     = -7,   // The thing you tried to do isn't supported
    AF_ERROR_NOT_CREATED       = -8,   // The afLib library isn't created yet
    AF_ERROR_TIMEOUT           = -9,   // The thing you tried doing took too long
    AF_ERROR_NO_SUCH_ATTRIBUTE = -10,  // Request was made for unknown attribute id
    AF_ERROR_INVALID_DATA      = -11,  // The data for a given attribute was incorrect
    AF_ERROR_FORBIDDEN         = -12,  // The thing you tried doing is forbidden
    AF_ERROR_ASR_REBOOTING     = -13,  // We are unable to do the thing you tried doing at this moment because the ASR is rebooting.  All get/set calls will return this error until we get the AF_SYSTEM_ASR_STATE_ATTR_ID attribute update from the ASR
} af_lib_error_t;


/* Modify this and rebuild to increase or decrease the number of outstanding requests
 * the library can handle
 */
#ifndef AF_LIB_REQUEST_QUEUE_SIZE
#define AF_LIB_REQUEST_QUEUE_SIZE                  10
#endif

/*
 * Modify this time to be large enough to handle the maximum time that should be given to a MCU set request from the server.
 * After this time has elapsed a AF_LIB_EVENT_MCU_SET_REQUEST_RESPONSE_TIMEOUT event will be triggered.
 */
#ifndef AF_LIB_SET_RESPONSE_TIMEOUT_SECONDS
#define AF_LIB_SET_RESPONSE_TIMEOUT_SECONDS     60
#endif

typedef bool (*attr_set_handler_t)(const uint8_t request_id, const uint16_t attribute_id, const uint16_t value_len, const uint8_t *value);
typedef void (*attr_notify_handler_t)(const uint8_t request_id, const uint16_t attribute_id, const uint16_t value_len, const uint8_t *value);

typedef struct af_lib_t af_lib_t;

/**
 * af_lib_create
 *
 * Create an instance of the afLib object.
 *
 * @param   attr_set         Callback for notification of attribute set requests from the server
 * @param   attr_notify      Callback for notification of unsolicited attribute update request completion
 * @param   the_transport    An instance of an af_transport_t object to be used for communications with the ASR
 *
 * @return  af_lib_t *        Instance of af_lib_t
 */
af_lib_t* af_lib_create(attr_set_handler_t attr_set, attr_notify_handler_t attr_notify, af_transport_t *the_transport);

/**
 * af_lib_destroy
 *
 * Destroy a previously created af_lib_t instance.
 *
 * @param af_lib    - an instance of af_lib_t
 */
void af_lib_destroy(af_lib_t* af_lib);

/**
 * af_lib_loop
 *
 * Called to afLib some CPU time
 */
void af_lib_loop(af_lib_t *af_lib);

/**
 * af_lib_get_attribute
 *
 * Request the value of an attribute be returned from the ASR.
 * Value will be returned in the attr_notify_handler_t callback or via the AF_LIB_EVENT_ASR_GET_RESPONSE event in the af_lib_event_callback_t.
 */
af_lib_error_t af_lib_get_attribute(af_lib_t *af_lib, const uint16_t attr_id);

/**
 * af_lib_set_attribute_*
 *
 * Request setting an attribute.
 * For MCU attributes, the attribute value will be updated.
 * For non-MCU attributes, the attribute value will be updated, and then attr_notify_handler_t callback or the AF_LIB_EVENT_ASR_SET_RESPONSE event in the af_lib_event_callback_t will be called.
 */
af_lib_error_t af_lib_set_attribute_bool(af_lib_t *af_lib, const uint16_t attr_id, const bool value);

af_lib_error_t af_lib_set_attribute_8(af_lib_t *af_lib, const uint16_t attr_id, const int8_t value);

af_lib_error_t af_lib_set_attribute_16(af_lib_t *af_lib, const uint16_t attr_id, const int16_t value);

af_lib_error_t af_lib_set_attribute_32(af_lib_t *af_lib, const uint16_t attr_id, const int32_t value);

af_lib_error_t af_lib_set_attribute_64(af_lib_t *af_lib, const uint16_t attr_id, const int64_t value);

af_lib_error_t af_lib_set_attribute_str(af_lib_t *af_lib, const uint16_t attr_id, const uint16_t value_len, const char *value);

af_lib_error_t af_lib_set_attribute_bytes(af_lib_t *af_lib, const uint16_t attr_id, const uint16_t value_len, const uint8_t *value);

/**
 * af_lib_is_idle
 *
 * Call to find out of ASR is currently handling a request.
 *
 * @param af_lib    - an instance of af_lib_t
 *
 * @return true if an operation is not in progress
 */
bool af_lib_is_idle(af_lib_t *af_lib);

/**
 * af_lib_sync
 *
 * Convenience function that will wait until afLib is idle before returning.
 *
 * @param af_lib    - an instance of af_lib_t
 */
void af_lib_sync(af_lib_t *af_lib);

/**
 * af_lib_mcu_isr
 *
 * Called to pass the interrupt along to afLib.
 *
 * @param af_lib    - an instance of af_lib_t
 */
void af_lib_mcu_isr(af_lib_t *af_lib);

/**
 * ASR exposes its capabilities via the AF_ATTRIBUTE_ID_ASR_CAPABILITIES attribute - which is internally cached by afLib.
 */
#define AF_ATTRIBUTE_ID_ASR_CAPABILITIES            1206

/* ASR supports MCU OTA functionality */
#define AF_ASR_CAPABILITY_MCU_OTA                   0

/**
 * af_lib_asr_has_capability
 *
 * This function can be used to check what capabilities ASR has.  Since it can take non zero time for ASR to communicate this
 * capability to afLib, if this function is called too early it will return the AF_ERROR_BUSY error code in which case it should be tried again
 * "at a later time" - ie. once the attribute is present in the AF_LIB_EVENT_ASR_GET_RESPONSE event.
 *
 * @param af_lib                - an instance of af_lib_t
 * @param af_asr_capability     - a given capability to test
 *
 * @return AF_SUCCESS             - afLib has the capability
 * @return AF_ERROR_NOT_SUPPORTED - afLib does not have the capability
 * @return AF_ERROR_BUSY          - afLib doesn't have the information from ASR yet so you need to try again at a later time
 */
af_lib_error_t af_lib_asr_has_capability(af_lib_t *af_lib, uint32_t af_asr_capability);

typedef enum {
    AF_LIB_EVENT_UNKNOWN = 0,              // Useful for catching bugs
    AF_LIB_EVENT_ASR_SET_RESPONSE,         // Response to af_lib_set_attribute() for an ASR attribute
    AF_LIB_EVENT_MCU_SET_REQ_SENT,         // Request from af_lib_set_attribute() for an MCU attribute has been sent to ASR
    AF_LIB_EVENT_MCU_SET_REQ_REJECTION,    // Request from af_lib_set_attribute() for an MCU attribute was rejected by ASR
    AF_LIB_EVENT_ASR_GET_RESPONSE,         // Response to af_lib_get_attribute()
    AF_LIB_EVENT_MCU_DEFAULT_NOTIFICATION, // Unsolicited default notification for an MCU attribute
    AF_LIB_EVENT_ASR_NOTIFICATION,         // Unsolicited notification of non-MCU attribute change
    AF_LIB_EVENT_MCU_SET_REQUEST,          // Request from ASR to MCU to set an MCU attribute, requires a call to af_lib_send_set_response()
    AF_LIB_EVENT_MCU_SET_REQUEST_RESPONSE_TIMEOUT,  // The af_lib_send_set_response() has not been called from a previous AF_LIB_EVENT_MCU_SET_REQUEST event in the AF_LIB_SET_RESPONSE_TIMEOUT_SECONDS time.
                                                    // This either indicates that the AF_LIB_SET_RESPONSE_TIMEOUT_SECONDS time is too short for some attribute set operations or there's a logic error on the MCU and a bug that should be fixed.
                                                    // In addition to being informational, afLib will inform the server that the last set for this attribute has failed.
    AF_LIB_EVENT_COMMUNICATION_BREAKDOWN,  // The communication between the MCU and the ASR seems to have stopped, take the appropriate action (ie. rebooting the ASR)
} af_lib_event_type_t;

typedef void (*af_lib_event_callback_t)(const af_lib_event_type_t event_type, const af_lib_error_t error, const uint16_t attribute_id, const uint16_t value_len, const uint8_t *value);

/**
 * af_lib_create_with_unified_callback
 *
 * @param   event_cb    Callback for event notifications
 * @param   transport   An instance of an af_transport_t object to be used for communications with ASR
 *
 * @return  af_lib_t *        Instance of af_lib_t
 */
af_lib_t *af_lib_create_with_unified_callback(af_lib_event_callback_t event_cb, af_transport_t *transport);

/**
 * af_lib_send_set_response
 *
 * Send the status and new value in response to a request by ASR to set an MCU attribute.
 *
 * @param af_lib        - an instance of af_lib_t
 * @param attribute_id  - the MCU attribute id that was requested to be set
 * @param set_succeeded - whether or not the set was applied successfully or not
 * @param value_len     - the length of the value for this attribute id
 * @param value         - the value of this attribute id
 *
 * @return AF_SUCCESS               - afLib told the ASR the new value for the attribute
 * @return AF_ERROR_INVALID_PARAM   - the attribute_id wasn't being set from the ASR
 * @return AF_ERROR_QUEUE_OVERFLOW  - there isn't enough room in the afLib queue to process this request
 */
af_lib_error_t af_lib_send_set_response(af_lib_t *af_lib, const uint16_t attribute_id, bool set_succeeded, const uint16_t value_len, const uint8_t *value);

/**
 * af_lib_dump_queue
 *
 * Dump (ie. log) the afLib request queue state (contents and other relevant information)
 */
void af_lib_dump_queue();

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* AF_LIB_H */
