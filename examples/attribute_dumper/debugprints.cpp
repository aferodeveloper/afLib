/****************************************************************************************************
   Debug Functions
 *                                                                                                  *
   Some helper functions to make debugging a little easier...
 ****************************************************************************************************/

#include <Arduino.h>
#include "debugprints.h"
#include "attrDB.h"

void printAttribute(const char *label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
  char *buffer = new char[ATTR_PRINT_BUFFER_LEN];
  memset(buffer, 0, ATTR_PRINT_BUFFER_LEN);
  sprintf(buffer, "%s id: %5u (%s) len: %u value: ", label, attributeId, getAttrName(attributeId), valueLen);

  char *valueBuf = new char[40];
  memset(valueBuf, 0, 40);

  switch (getAttrType(attributeId)) {
    case ATTRIBUTE_TYPE_SINT8:
      sprintf(valueBuf, "0x%02x", *((uint8_t *)value));
      strcat(buffer, valueBuf);
      break;
    case ATTRIBUTE_TYPE_SINT16:
      sprintf(valueBuf, "0x%04x", *((uint16_t *)value));
      strcat(buffer, valueBuf);
      break;
    case ATTRIBUTE_TYPE_SINT32:
      sprintf(valueBuf, "0x%0x", *((uint32_t *)value));
      strcat(buffer, valueBuf);
      break;
    case ATTRIBUTE_TYPE_SINT64:
      sprintf(valueBuf, "0x%0x", *((uint64_t *)value));
      strcat(buffer, valueBuf);
      break;
    case ATTRIBUTE_TYPE_BOOLEAN:
      strcat(buffer, *value == 1 ? "true" : "false");
      break;
    case ATTRIBUTE_TYPE_UTF8S:
      {
        if (valueLen == 0) {
          strcat(buffer, "null");
        } else {
          int len = strlen(buffer);
          for (int i = 0; i < valueLen; i++) {
            buffer[len + i] = (char)value[i];
          }
        }
      }
      break;
    case ATTRIBUTE_TYPE_Q_16_16:
      // bytes for now
      // break;
    case ATTRIBUTE_TYPE_BYTES:
      if (valueLen == 0) {
        strcat(buffer, "null");
      } else {
        strcat(buffer, "0x");
        for (int i = 0; i < valueLen; i++) {
          if (valueLen > 1 && value[i] < 16) strcat(buffer, "0");
          strcat(buffer, String(value[i], HEX).c_str());
        }
      }
      break;
    default:
      break;
  }

  Serial.print(buffer);
  delete(buffer);
  delete(valueBuf);
  Serial.println(extendedInfo(attributeId, valueLen, value));

}
