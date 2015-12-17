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

#ifndef AF_ERRORS_H__
#define AF_ERRORS_H__

#define afSUCCESS                    0    // Operation completed successfully
#define afERROR_NO_SUCH_ATTRIBUTE   -1    // Request was made for unknown attribute id
#define afERROR_BUSY                -2    // Request already in progress, try again
#define afERROR_INVALID_COMMAND     -3    // Command could not be parsed
#define afERROR_QUEUE_OVERFLOW      -4    // Queue is full
#define afERROR_QUEUE_UNDERFLOW     -5    // Queue is empty
#define afERROR_INVALID_PARAM       -6    // Bad input parameter

#endif // AF_ERRORS_H__
