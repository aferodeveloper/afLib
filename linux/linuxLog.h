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
#ifndef AFLIB_LINUXLOG_H
#define AFLIB_LINUXLOG_H

#include "stream.h"

class linuxLog : public Stream {
        public:

        virtual void print(int val);
        virtual void print(const char *val);
        virtual void print(int val ,int format);
        virtual void println(int val);
        virtual void println(const char *val);
        virtual void println(int val,int format);
};

#endif

