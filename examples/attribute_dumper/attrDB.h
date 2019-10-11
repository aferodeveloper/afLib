#define ATTRIBUTE_TYPE_BOOLEAN                                     1
#define ATTRIBUTE_TYPE_SINT8                                       2
#define ATTRIBUTE_TYPE_SINT16                                      3
#define ATTRIBUTE_TYPE_SINT32                                      4
#define ATTRIBUTE_TYPE_SINT64                                      5
#define ATTRIBUTE_TYPE_Q_16_16                                     6
#define ATTRIBUTE_TYPE_UTF8S                                      20
#define ATTRIBUTE_TYPE_BYTES                                      21


struct af_attribute {
  int     id;
  const __FlashStringHelper*   name;
  short   size;
  short   type;
};

void createAttrDB();
int getAttrDBSize();
const char* getAttrName(const uint16_t attrId);
short getAttrSize(const uint16_t attrId);
short getAttrType(const uint16_t attrId);
char* extendedInfo(const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);
