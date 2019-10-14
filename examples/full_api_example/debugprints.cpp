/****************************************************************************************************
   Debug Functions
 *                                                                                                  *
   Some helper functions to make debugging a little easier...
 ****************************************************************************************************/

#include <Arduino.h>
#include "debugprints.h"
#include "attribute_db.h"
#include "af_logger.h"

void printAttribute(const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value, af_lib_error_t error) {
  char *buffer = new char[ATTR_PRINT_BUFFER_LEN];
  memset(buffer, 0, ATTR_PRINT_BUFFER_LEN);
  sprintf(buffer, "id: %5u (%s) rc: %03d len: %u value: ", attributeId, getAttrName(attributeId), error, valueLen);

  char *valueBuf = new char[ATTR_PRINT_MAX_VALUE_LEN];
  memset(valueBuf, 0, ATTR_PRINT_MAX_VALUE_LEN);

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
      sprintf(valueBuf, "0x%0lu", *((uint32_t *)value));
      strcat(buffer, valueBuf);
      break;
    case ATTRIBUTE_TYPE_SINT64:
      sprintf(valueBuf, "0x%0llu", *((uint64_t *)value));
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

  af_logger_print_buffer(buffer);
  delete(buffer);
  delete(valueBuf);
  af_logger_println_buffer(extendedInfo(attributeId, valueLen, value));

}
