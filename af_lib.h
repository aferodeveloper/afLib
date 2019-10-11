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
 * This file defines everything your application should need for commuinicating with the Afero ASR-1 radio module.
 * Is there is anything missing from this file, please post a request on the developer forum and we will see what
 * we can do.
 */
#ifndef AF_LIB_H
#define AF_LIB_H

#include "af_errors.h"
#include "af_transport.h"
#include "af_command.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define afMINIMUM_TIME_BETWEEN_REQUESTS     1000

#define MAX_ATTRIBUTE_SIZE                  255
#define REQUEST_QUEUE_SIZE                  10

typedef bool (*attr_set_handler_t)(const uint8_t request_id, const uint16_t attribute_id, const uint16_t value_len, const uint8_t *value);
typedef void (*attr_notify_handler_t)(const uint8_t request_id, const uint16_t attribute_id, const uint16_t value_len, const uint8_t *value);

typedef struct af_lib_t af_lib_t;

/**
 * af_lib_create
 *
 * Create an instance of the afLib object.
 *
 * @param   attr_set         Callback for notification of attribute set requests
 * @param   attr_notify      Callback for notification of attribute set request completions
 * @param   the_transport    An instance of an afTransport object to be used for communications with the ASR-1
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
 * loop
 *
 * Called by the loop() method to give afLib some CPU time
 */
void af_lib_loop(af_lib_t *af_lib);

/**
 * getAttribute
 *
 * Request the value of an attribute be returned from the ASR-1.
 * Value will be returned in the attr_notify_handler_t callback.
 */
int af_lib_get_attribute(af_lib_t *af_lib, const uint16_t attr_id);

/**
 * setAttribute
 *
 * Request setting an attribute.
 * For MCU attributes, the attribute value will be updated.
 * For IO attributes, the attribute value will be updated, and then onAttrSetComplete will be called.
 */
int af_lib_set_attribute_bool(af_lib_t *af_lib, const uint16_t attr_id, const bool value);

int af_lib_set_attribute_8(af_lib_t *af_lib, const uint16_t attr_id, const int8_t value);

int af_lib_set_attribute_16(af_lib_t *af_lib, const uint16_t attr_id, const int16_t value);

int af_lib_set_attribute_32(af_lib_t *af_lib, const uint16_t attr_id, const int32_t value);

int af_lib_set_attribute_64(af_lib_t *af_lib, const uint16_t attr_id, const int64_t value);

int af_lib_set_attribute_str(af_lib_t *af_lib, const uint16_t attr_id, const uint16_t value_len, const char *value);

int af_lib_set_attribute_bytes(af_lib_t *af_lib, const uint16_t attr_id, const uint16_t value_len, const uint8_t *value);

/**
 * isIdle
 *
 * Call to find out of the ASR-1 is currently handling a request.
 *
 * @return true if an operation is in progress
 */
bool af_lib_is_idle(af_lib_t *af_lib);

/**
 * mcuISR
 *
 * Called to pass the interrupt along to afLib.
 */
void af_lib_mcu_isr(af_lib_t *af_lib);


#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* AF_LIB_H */
