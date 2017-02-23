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

/**
 * afLib public interface
 *
 * This file defines everything your application should need for commuinicating with the Afero ASR-1 radio module.
 * Is there is anything missing from this file, please post a request on the developer forum and we will see what
 * we can do.
 */
#ifndef AFLIB_IAFLIB_H
#define AFLIB_IAFLIB_H

#include "Arduino.h"
#include "afErrors.h"
#include "afSPI.h"

#define afMINIMUM_TIME_BETWEEN_REQUESTS     1000

#define MAX_ATTRIBUTE_SIZE                  255

typedef void (*isr)();
typedef bool (*AttrSetHandler)(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);
typedef void (*AttrNotifyHandler)(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);

class iafLib {
public:
    /**
     * create
     *
     * Create an instance of the afLib object. The afLib is a singleton. Calling this method multiple
     * times will return the same instance.
     *
     * @param   mcuInterrupt    The Arduino interrupt to be used (returned from digitalPinToInterrupt)
     * @param   isrWrapper      This is the isr method that must be defined in your sketch
     * @param   attrSet         Callback for notification of attribute set requests
     * @param   attrNotify Callback for notification of attribute set request completions
     * @return  iafLib *        Instance of iafLib
     */
    static iafLib * create(const int mcuInterrupt, isr isrWrapper,
                           AttrSetHandler attrSet, AttrNotifyHandler attrNotify, Stream *theLog, afSPI *theSPI);

    /**
     * loop
     *
     * Called by the loop() method in your sketch to give afLib some CPU time
     */
    virtual void loop(void) = 0;

    /**
     * getAttribute
     *
     * Request the value of an attribute be returned from the ASR-1.
     * Value will be returned in the attrNotify callback.
     */
    virtual int getAttribute(const uint16_t attrId) = 0;

    /**
     * setAttribute
     *
     * Request setting an attribute.
     * For MCU attributes, the attribute value will be updated.
     * For IO attributes, the attribute value will be updated, and then onAttrSetComplete will be called.
     */
    virtual int setAttributeBool(const uint16_t attrId, const bool value) = 0;

    virtual int setAttribute8(const uint16_t attrId, const int8_t value) = 0;

    virtual int setAttribute16(const uint16_t attrId, const int16_t value) = 0;

    virtual int setAttribute32(const uint16_t attrId, const int32_t value) = 0;

    virtual int setAttribute64(const uint16_t attrId, const int64_t value) = 0;

    virtual int setAttributeStr(const uint16_t attrId, const String &value) = 0;

    virtual int setAttributeCStr(const uint16_t attrId, const uint16_t valueLen, const char *value) = 0;

    virtual int setAttributeBytes(const uint16_t attrId, const uint16_t valueLen, const uint8_t *value) = 0;

    /**
     * isIdle
     *
     * Call to find out of the ASR-1 is currently handling a request.
     *
     * @return true if an operation is in progress
     */
    virtual bool isIdle() = 0;

    /**
     * mcuISR
     *
     * Called by your sketch to pass the interrupt along to afLib.
     */
    virtual void mcuISR() = 0;
};
#endif //AFLIB_IAFLIB_H
