
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

#ifndef ATTRIBUTE_DB_H
#define ATTRIBUTE_DB_H

// These defines are in device-description.h as well
#define ATTRIBUTE_TYPE_BOOLEAN                                     1
#define ATTRIBUTE_TYPE_SINT8                                       2
#define ATTRIBUTE_TYPE_SINT16                                      3
#define ATTRIBUTE_TYPE_SINT32                                      4
#define ATTRIBUTE_TYPE_SINT64                                      5
#define ATTRIBUTE_TYPE_Q_16_16                                     6
#define ATTRIBUTE_TYPE_UTF8S                                      20
#define ATTRIBUTE_TYPE_BYTES                                      21


struct af_attribute {
  uint16_t        id;
  const char*   name;
  uint16_t      size;
  uint8_t       type;
};

void createAttrDB();
int getAttrDBSize();
void isAsr1(bool isasr1);
const char* getAttrName(const uint16_t attrId);
uint16_t getAttrSize(const uint16_t attrId);
uint8_t getAttrType(const uint16_t attrId);
char* extendedInfo(const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);

#endif /* ATTRIBUTE_DB_H */
