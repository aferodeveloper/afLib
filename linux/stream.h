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
#ifndef AFLIB_STREAM_H
#define AFLIB_STREAM_H

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

class Stream {
        public:

        virtual void print(int val) = 0 ;
        virtual void print(const char *val) = 0 ;
        virtual void print(int val ,int format) = 0 ;
        virtual void println(int val) = 0 ;
        virtual void println(const char *val) = 0 ;
        virtual void println(int val,int format) = 0 ;
};

#endif

