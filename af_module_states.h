/**
 * Copyright 2016 Afero, Inc.
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

#ifndef AF_MODULE_STATES_H
#define AF_MODULE_STATES_H

#ifdef  __cplusplus
extern "C" {
#endif

// The MCU should now look for AF_MODULE_STATE_REBOOTED or AF_MODULE_STATE_LINKED to indicate that the ASR has rebooted, and AF_MODULE_STATE_RELINKED to mean relinked
typedef enum {
    AF_MODULE_STATE_REBOOTED,
    AF_MODULE_STATE_LINKED, // means rebooted & linked, which will only happen after the first link after a reboot
    AF_MODULE_STATE_UPDATING,
    AF_MODULE_STATE_UPDATE_READY,
    AF_MODULE_STATE_INITIALIZED,
    AF_MODULE_STATE_RELINKED, // means subsequent relinks
    AF_MODULE_STATE_FACTORY_RESET, // means ASR has performed a factory reset on itself and requires a reboot by the MCU after it's reset it's own state
} af_module_states_t;

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* AF_MODULE_STATES_H */
