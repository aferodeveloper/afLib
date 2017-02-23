/**
 * Copyright 2015 Afero, Inc.
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

#ifndef MSG_TYPE_H__
#define MSG_TYPE_H__

typedef uint8_t msg_type_t;

#define MSG_TYPE_UNKNOWN    0
#define MSG_TYPE_ERROR      1

// Messaging channel messages
#define MESSAGE_CHANNEL_BASE                    10
#define MSG_TYPE_SET                            (MESSAGE_CHANNEL_BASE + 1)
#define MSG_TYPE_GET                            (MESSAGE_CHANNEL_BASE + 2)
#define MSG_TYPE_UPDATE                         (MESSAGE_CHANNEL_BASE + 3)

#define NEGOTIATOR_CHANNEL_BASE                 20
#define MSG_TYPE_AUTHENTICATOR_SESSION_INFO     (NEGOTIATOR_CHANNEL_BASE + 1)
#define MSG_TYPE_PERIPHERAL_SESSION_INFO        (NEGOTIATOR_CHANNEL_BASE + 2)
#define MSG_TYPE_SIGNED_SESSION_PUBLIC_KEYS     (NEGOTIATOR_CHANNEL_BASE + 3)
#define MSG_TYPE_MESSAGING_AVAILABLE            (NEGOTIATOR_CHANNEL_BASE + 4)
#define MSG_TYPE_PAIRING_COMPLETE               (NEGOTIATOR_CHANNEL_BASE + 5)

// Success states
#define UPDATE_STATE_UPDATED                0

// Failure states
#define UPDATE_STATE_INTERRUPTED            1
#define UPDATE_STATE_UNKNOWN_UUID           2
#define UPDATE_STATE_LENGTH_EXCEEDED        3
#define UPDATE_STATE_CONFLICT               4
#define UPDATE_STATE_FAILED                 5

#define UPDATE_REASON_UNKNOWN                   0x00
#define UPDATE_REASON_LOCAL_OR_MCU_UPDATE       0x01 // local or unsolicited UPDATE from MCU
#define UPDATE_REASON_SERVICE_SET               0x02 // response to Service SET
#define UPDATE_REASON_MCU_SET                   0x03 // response to MCU SET
#define UPDATE_REASON_RELINK                    0x04
#define UPDATE_REASON_BIND_FOLLOW               0x05 // a bound attribute was changed
#define UPDATE_REASON_FAKE_UPDATE               0x06 // fake update launched
#define UPDATE_REASON_NOTIFY_MCU_WE_REBOOTED    0x07 // notify MCU we rebooted. Never goes to Service
#define UPDATE_REASON_INTERNAL_LAST_VALID       UPDATE_REASON_NOTIFY_MCU_WE_REBOOTED // always last valid #define
#define UPDATE_REASON_INTERNAL_SET_FAIL         0xfd // Set failed (not to be sent to Service)
#define UPDATE_REASON_INTERNAL_INITIAL_VALUE    0xfe // initial value (not to be sent to Service)
#define UPDATE_REASON_INTERNAL_NO_CHANGE        0xff // do not change current value (not to be sent to Service)

#endif  // MSG_TYPE_H__

