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

#endif  // MSG_TYPE_H__

