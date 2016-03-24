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
#include "Arduino.h"
#include "linuxLog.h"


void linuxLog::print(int val)
{
printf("%d",val);
}
void linuxLog::print(const char *val)
{
printf("%s",val);
}
void linuxLog::print(int val ,int format)
{
}
void linuxLog::println(int val)
{
printf("%d\n",val);
}
void linuxLog::println(const char *val)
{
printf("%s\n",val);
}
void linuxLog::println(int val,int format)
{
}


