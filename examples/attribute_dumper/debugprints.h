
#define ATTR_PRINT_HEADER_LEN     60
#define ATTR_PRINT_MAX_VALUE_LEN  512   // Max attribute len is 256 bytes; Each HEX-printed byte is 2 ASCII characters
#define ATTR_PRINT_BUFFER_LEN     (ATTR_PRINT_HEADER_LEN + ATTR_PRINT_MAX_VALUE_LEN)

void printAttribute(const char *label, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);
