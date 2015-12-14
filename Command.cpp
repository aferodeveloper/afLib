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
#include <stdio.h>
#include "Command.h"
#include "msg_types.h"

#define CMD_HDR_LEN  4    // 4 byte header on all commands
#define CMD_VAL_LEN  2    // 2 byte value length for commands that have a value

const char *CMD_NAMES[] = {"SET   ", "GET   ", "UPDATE"};

byte getVal(char c);

Command::Command(uint16_t len, uint8_t *bytes) {
  int index = 0;
  
//  Serial.print("Command bytes: ");
//  for (int i = 0; i < len; i++) {
//    Serial.print(bytes[i], HEX); Serial.print(" ");
//  }
//  Serial.println("");
  
  _cmd = bytes[index++];
  _requestId = bytes[index++];
//  Serial.print("req id: "); Serial.println(_requestId);
  _attrId = bytes[index + 0] | bytes[index + 1] << 8;
  index += 2;
//  Serial.print("attr id: "); Serial.println(_attrId);

  if (_cmd == MSG_TYPE_GET) {
    return;
  }
  if (_cmd == MSG_TYPE_UPDATE) {
//    Serial.println("Update");
    _status = bytes[index++];
    _reason = bytes[index++];
//    Serial.print("status: "); Serial.println(_status);
  }
  
  _valueLen = bytes[index + 0] | bytes[index + 1] << 8;
  index += 2;
//  Serial.print("value len: "); Serial.println(_valueLen);
  _value = new uint8_t[_valueLen];
  for (int i = 0; i < _valueLen; i++) {
    _value[i] = bytes[index + i];
  }
}

Command::Command(uint8_t requestId, const char *str) {
//  Serial.print("Command: "); Serial.println(str);
  _requestId = requestId & 0xff;
  
  char *cp = strdup(str);
  char *tok = strtok(cp, " ");
  _cmd = strToCmd(tok);

  tok = strtok(NULL, " ");
  _attrId = strToAttrId(tok);
  
  if (_cmd == MSG_TYPE_GET) {
    _valueLen = 0;
    _value = NULL;
  } else {
    tok = strtok(NULL, " ");
    _valueLen = strlen(tok) / 2;
    _value = new uint8_t[_valueLen];
    strToValue(tok, _value);
  }
  
  free(cp);
}

Command::Command(uint8_t requestId, uint8_t cmd, uint16_t attrId) {
  _requestId = requestId;
  _cmd = cmd;
  _attrId = attrId;
  _valueLen = 0;
  _value = NULL;
}

Command::Command(uint8_t requestId, uint8_t cmd, uint16_t attrId, uint16_t valueLen, uint8_t *value) {
  _requestId = requestId;
  _cmd = cmd;
  _attrId = attrId;
  _valueLen = valueLen;
  _value = new uint8_t[_valueLen];
  memcpy(_value, value, valueLen);
}

Command::Command(uint8_t requestId, uint8_t cmd, uint16_t attrId, uint8_t status, uint8_t reason, uint16_t valueLen, uint8_t *value) {
  _requestId = requestId;
  _cmd = cmd;
  _attrId = attrId;
  _status = status;
  _reason = reason;
  _valueLen = valueLen;
  _value = new uint8_t[_valueLen];
  memcpy(_value, value, valueLen);
}

Command::Command() {
}

Command::~Command() {
  if (_value != NULL) {
    delete(_value);
  }
}

int Command::strToValue(char *valueStr, uint8_t *value) {
  for (int i = 0; i < (int)(strlen(valueStr) / 2); i++) {
    int j = i * 2;
    value[i] = getVal(valueStr[j + 1]) + (getVal(valueStr[j]) << 4);
  }

  return 0;
}

uint16_t Command::strToAttrId(char *attrIdStr) {
  return String(attrIdStr).toInt();
}

uint8_t Command::strToCmd(char *cmdStr) {
  char c = cmdStr[0];
  if (c == 'g' || c == 'G') {
    return MSG_TYPE_GET;
  } else if (c == 's' || c == 'S') {
    return MSG_TYPE_SET;
  } else if (c == 'u' || c == 'U') {
    return MSG_TYPE_UPDATE;
  }
  
  return -1;
}  

uint8_t Command::getCommand() {
  return _cmd;
}

uint8_t Command::getReqId() {
  return _requestId;
}

uint16_t Command::getAttrId() {
  return _attrId;
}

uint16_t Command::getValueLen() {
  return _valueLen;
}

void Command::getValue(uint8_t *value) {
  for (int i = 0; i < _valueLen; i++) {
    value[i] = _value[i];
  }
}

uint16_t Command::getSize() {
  uint16_t len = CMD_HDR_LEN;
 
  if (_cmd != MSG_TYPE_GET) {
    len += CMD_VAL_LEN + _valueLen;
  }
  
  if (_cmd == MSG_TYPE_UPDATE) {
    len += 2; // status byte + reason byte
  }
  
  return len;
}

uint16_t Command::getBytes(uint8_t *bytes) {
  uint16_t len = getSize();
  
  int index = 0;
     
  bytes[index++] = (_cmd);

  bytes[index++] = (_requestId);
  
  bytes[index++] = (_attrId & 0xff);
  bytes[index++] = ((_attrId >> 8) & 0xff);

  if (_cmd == MSG_TYPE_GET) {
    return len;
  }
  
  if (_cmd == MSG_TYPE_UPDATE) {
    bytes[index++] = (_status);
    bytes[index++] = (_reason);
  }
  
  bytes[index++] = (_valueLen & 0xff);
  bytes[index++] = ((_valueLen >> 8) & 0xff);
  
  for (int i = 0; i < _valueLen; i++) {
    bytes[index++] = (_value[i]);
  }
  
  return len;
}

bool Command::isValid() {
  return (_cmd == MSG_TYPE_SET) || (_cmd == MSG_TYPE_GET) || (_cmd == MSG_TYPE_UPDATE);
}

void Command::dumpBytes() {
  uint16_t len = getSize();
  uint8_t bytes[len];
  getBytes(bytes);

  _printBuf[0] = 0;
  sprintf(_printBuf, "len: %d value: ", len);
  for (int i = 0; i < len; i++) {
    int b = bytes[i] & 0xff;
    sprintf(&_printBuf[strlen(_printBuf)], "%02x", b);
  }
  Serial.println(_printBuf);
}

void Command::dump() {
  _printBuf[0] = 0;
  sprintf(_printBuf, "cmd: %s attr: %d value: ",
    CMD_NAMES[_cmd - MESSAGE_CHANNEL_BASE - 1],
    _attrId
    );
  if (_cmd != MSG_TYPE_GET) {
    for (int i = 0; i < _valueLen; i++) {
      int b = _value[i] & 0xff;
      sprintf(&_printBuf[strlen(_printBuf)], "%02x", b);
    }
  }
  Serial.println(_printBuf);
}

byte getVal(char c)
{
   if (c >= '0' && c <= '9')
     return (byte)(c - '0');
   else if (c >= 'A' && c <= 'F')
     return (byte)(c-'A'+10);
   else if (c >= 'a' && c <= 'f')
     return (byte)(c-'a'+10);
   
   Serial.print("bad hex char: "); Serial.println(c);

  return 0;
}

