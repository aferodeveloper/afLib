// System attribute database

#include <Arduino.h>
#include <stdio.h>
#include <time.h>
#include <avr/pgmspace.h>
#include "attrDB.h"

struct af_attribute attrDB[53];

int num_attrs = 0;

void createAttrDB() {
  int i = 0;

  attrDB[i].id =  1024; attrDB[i].name = F("GPIO 0"); attrDB[i].size = 2; attrDB[i++].type = ATTRIBUTE_TYPE_SINT16;
  attrDB[i].id =  1025; attrDB[i].name = F("GPIO 0 Config"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id =  1026; attrDB[i].name = F("GPIO 1"); attrDB[i].size = 2; attrDB[i++].type = ATTRIBUTE_TYPE_SINT16;
  attrDB[i].id =  1027; attrDB[i].name = F("GPIO 1 Config"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id =  1028; attrDB[i].name = F("GPIO 2"); attrDB[i].size = 2; attrDB[i++].type = ATTRIBUTE_TYPE_SINT16;
  attrDB[i].id =  1029; attrDB[i].name = F("GPIO 2 Config"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id =  1030; attrDB[i].name = F("GPIO 3"); attrDB[i].size = 2; attrDB[i++].type = ATTRIBUTE_TYPE_SINT16;
  attrDB[i].id =  1031; attrDB[i].name = F("GPIO 3 Config"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;

  attrDB[i].id =  1201; attrDB[i].name = F("ASR: UTC Time"); attrDB[i].size = 4; attrDB[i++].type = ATTRIBUTE_TYPE_SINT32;
  attrDB[i].id =  1202; attrDB[i].name = F("ASR: Device ID"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id =  1203; attrDB[i].name = F("ASR: Assoc ID"); attrDB[i].size = 12; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id =  1204; attrDB[i].name = F("ASR: Company Code"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;
  attrDB[i].id =  1205; attrDB[i].name = F("ASR: Online Status"); attrDB[i].size = 3; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id =  1206; attrDB[i].name = F("ASR: Capabilities"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;

  attrDB[i].id =  1301; attrDB[i].name = F("OTA MCU Info"); attrDB[i].size = 64; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id =  1302; attrDB[i].name = F("OTA MCU Data"); attrDB[i].size = 253; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;

  attrDB[i].id =  2001; attrDB[i].name = F("Bootloader Version"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_SINT64;
  attrDB[i].id =  2002; attrDB[i].name = F("Soft Device Version"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_SINT64;
  attrDB[i].id =  2003; attrDB[i].name = F("Application Version"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_SINT64;
  attrDB[i].id =  2004; attrDB[i].name = F("Profile Version"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_SINT64;
  attrDB[i].id =  2006; attrDB[i].name = F("Wifi Version"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_SINT64;
  attrDB[i].id =  2007; attrDB[i].name = F("Wifi Certificates Version"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_SINT64;
  attrDB[i].id =  2008; attrDB[i].name = F("WAN APN List Version"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_SINT64;

  attrDB[i].id = 59001; attrDB[i].name = F("Offline Schedule Enabled"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;

  attrDB[i].id = 65000; attrDB[i].name = F("MCU UART Config"); attrDB[i].size = 4; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id = 65001; attrDB[i].name = F("UTC offset data"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id = 65002; attrDB[i].name = F("Maximum Re-link Interval"); attrDB[i].size = 4; attrDB[i++].type = ATTRIBUTE_TYPE_SINT32;
  attrDB[i].id = 65003; attrDB[i].name = F("Attributes CRC32"); attrDB[i].size = 4; attrDB[i++].type = ATTRIBUTE_TYPE_SINT32;
  attrDB[i].id = 65004; attrDB[i].name = F("Configured SSID"); attrDB[i].size = 33; attrDB[i++].type = ATTRIBUTE_TYPE_UTF8S;
  attrDB[i].id = 65005; attrDB[i].name = F("Wi-Fi Strength"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;
  attrDB[i].id = 65006; attrDB[i].name = F("Wi-Fi Steady State"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;
  attrDB[i].id = 65007; attrDB[i].name = F("Wi-Fi Setup State"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;
  attrDB[i].id = 65008; attrDB[i].name = F("Network Type"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;
  attrDB[i].id = 65012; attrDB[i].name = F("Command"); attrDB[i].size = 4; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id = 65013; attrDB[i].name = F("ASR State"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;
  attrDB[i].id = 65014; attrDB[i].name = F("Low Battery Warn"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;
  attrDB[i].id = 65015; attrDB[i].name = F("Linked Timestamp"); attrDB[i].size = 4; attrDB[i++].type = ATTRIBUTE_TYPE_SINT32;
  attrDB[i].id = 65018; attrDB[i].name = F("Attribute ACK"); attrDB[i].size = 2; attrDB[i++].type = ATTRIBUTE_TYPE_SINT16;
  attrDB[i].id = 65019; attrDB[i].name = F("Reboot Reason"); attrDB[i].size = 100; attrDB[i++].type = ATTRIBUTE_TYPE_UTF8S;
  attrDB[i].id = 65020; attrDB[i].name = F("BLE Comms"); attrDB[i].size = 12; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id = 65021; attrDB[i].name = F("MCU Interface"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;
  attrDB[i].id = 65022; attrDB[i].name = F("Network Capabilities"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;
  attrDB[i].id = 65024; attrDB[i].name = F("Wi-Fi Interface State"); attrDB[i].size = 1; attrDB[i++].type = ATTRIBUTE_TYPE_SINT8;
  attrDB[i].id = 65025; attrDB[i].name = F("Wi-Fi IP Address"); attrDB[i].size = 4; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;
  attrDB[i].id = 65026; attrDB[i].name = F("Wi-Fi Uptime"); attrDB[i].size = 4; attrDB[i++].type = ATTRIBUTE_TYPE_SINT32;
  attrDB[i].id = 65066; attrDB[i].name = F("Device Capability"); attrDB[i].size = 8; attrDB[i++].type = ATTRIBUTE_TYPE_BYTES;

  num_attrs = i;
}

int getAttrDBSize() {
  return num_attrs;
}

const char* getAttrName(const uint16_t attrId) {
  for (int i = 0; i < num_attrs; i++) {
    if (attrDB[i].id == attrId) {
      return (const char *)attrDB[i].name;
    }
  }
  char *buffer = new char[32];
  if (attrId < 1024) {
    sprintf(buffer, "MCU Attr #%u", attrId);
  } else {
    sprintf(buffer, "Unknown Attr #%u", attrId);
  }
  return buffer;
}

short getAttrSize(const uint16_t attrId) {
  for (int i = 0; i < num_attrs; i++) {
    if (attrDB[i].id == attrId) {
      return attrDB[i].size;
    }
  }
  return -1;
}

short getAttrType(const uint16_t attrId) {
  for (int i = 0; i < num_attrs; i++) {
    if (attrDB[i].id == attrId) {
      return attrDB[i].type;
    }
  }
  return ATTRIBUTE_TYPE_BYTES;
}

char* timeString(time_t t) {
  static char buf[80];
  struct tm  ts;

  // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
  ts = *localtime(&t);
  strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
  return buf;
}

char* extendedInfo(const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {

  char *buffer = new char[300];
  memset(buffer, 0, 300);
  sprintf(buffer, " ");

  char dataBuf[100];
  memset(dataBuf, 0, 100);

  switch (attributeId) {
    case  1025: // GPIO 0 Configuration
    case  1027: // GPIO 1 Configuration
    case  1029: // GPIO 2 Configuration
    case  1031: // GPIO 3 Configuration
      {
        boolean input = false;
        strcat(buffer, "(");
        if (value[0] & 1) {
          input = true;
          strcat(buffer, "Input ");
        } else {
          strcat(buffer, "Output ");
        }
        if ( input && value[0] &  2) strcat(buffer, "ADC ");
        if ( input && value[0] &  4) strcat(buffer, "Pullup ");
        if ( input && value[0] &  8) strcat(buffer, "Pulldown ");
        if (!input && value[0] & 16) strcat(buffer, "PWM ");
        if ( input && value[0] & 32) strcat(buffer, "Comparator ");
        if ( input && value[0] & 64 && ((value[1] & 2) == 0)) strcat(buffer, "Toggle ");
        if (!input && value[0] & 128) strcat(buffer, "Pulse ");
        if (value[1] & 1) {
          strcat(buffer, "Act.L ");
        } else {
          strcat(buffer, "Act.H ");
        }
        if ( input && value[1] & 2 && ((value[0] & 64) == 0)) strcat(buffer, "Count ");
        strcat(buffer, ")");
      }
      break;
    case  1205: // ASR Online Status
      if (value[0] == 0) {
        strcat(buffer, "(Offline ");
      } else {
        strcat(buffer, "(Online ");
      }
      if (value[1] == 1) strcat(buffer, "BLE");
      if (value[1] == 2) strcat(buffer, "WIFI");
      if (value[1] == 2) {
        strcat(buffer, ", bars=");
        char v[3];
        sprintf(v, "%d", value[2]);
        strcat(buffer, v);
      }
      strcat(buffer, ")");
      break;
    case 1206: // ASR Capabilities
      if (value[0] & 0x80) strcat(buffer, "(MCU OTA Supported)");
      break;
    case  1301: // MCU OTA Info
      // break out these bytes here if needed
      break;
    case  1302: // MCU OTA Data
      // break out these bytes here if needed
      break;
    case 65066: // Device Capability
      strcat(buffer, "(");
      if (value[0] & 0x80) strcat(buffer, "COMMAND_ACK");
      if (value[0] & 0x40) strcat(buffer, " VIEW");
      if (value[0] & 0x20) strcat(buffer, " ADD_REMOVE_DEVICE");
      if (value[0] & 0x10) strcat(buffer, " PERIPHERAL_MODE");
      if (value[0] & 0x08) strcat(buffer, " WIFI_MODE");
      if (value[0] & 0x04) strcat(buffer, " CLAIM");
      if (value[0] & 0x02) strcat(buffer, " ROUTER");
      if (value[0] & 0x01) strcat(buffer, " ENTPERPRISE");
      strcat(buffer, ")");
      break;
    case 65000: // MCU UART Config
      switch (value[0]) {
        case 0:
          strcat(buffer, "(  4800 ");
          break;
        case 1:
          strcat(buffer, "(  9600 ");
          break;
        case 2:
          strcat(buffer, "( 38400 ");
          break;
        case 3:
          strcat(buffer, "(115200 ");
          break;
        default:
          strcat(buffer, "(unk baud ");
          break;
      }
      switch (value[1]) {
        case 0:
          strcat(buffer, "5");
          break;
        case 1:
          strcat(buffer, "6");
          break;
        case 2:
          strcat(buffer, "7");
          break;
        case 3:
          strcat(buffer, "8");
          break;
        case 4:
          strcat(buffer, "9");
          break;
      }
      switch (value[2]) {
        case 0:
          strcat(buffer, "N");
          break;
        case 1:
          strcat(buffer, "O");
          break;
        case 2:
          strcat(buffer, "E");
          break;
      }
      switch (value[3]) {
        case 0:
          strcat(buffer, "1)");
          break;
        case 1:
          strcat(buffer, "1.5)");
          break;
        case 2:
          strcat(buffer, "2)");
          break;
      }
      break;
    case 59001: // Offline Schedule Enabled
      {
        if (*value == 1) {
          strcat(buffer, "(Enabled)");
        } else {
          strcat(buffer, "(Disabled)");
        }
      }
      break;
    case 65001: // TZ Offset Data
      {
        int16_t tzoff1 = value[1] * 256 + value[0];
        uint32_t nextoff = value[5] * 16777216 + value[4] * 65536 + value[3] * 256 + value[2];
        int16_t tzoff2 = value[7] * 256 + value[6];
        if ((tzoff1 > 12 * 60 || tzoff1 < -12 * 60) || (tzoff2 > 12 * 60 || tzoff2 < -12 * 60) || nextoff == 0) {
          strcat(buffer, "(invalid)");
        } else {
          sprintf(dataBuf, "(now=%dm, next=%dm at %s)", tzoff1, tzoff2, timeString((time_t)nextoff));
          strcat(buffer, dataBuf);
        }
      }
      break;
    case 65005: // Wifi Bars
      {
        sprintf(dataBuf, "(%d%%)", (int)value[0] * 100 / 256);
        strcat(buffer, dataBuf);
      }
      break;
    case 65006: // WiFi Setup State
    case 65007: // Wifi Steady State
      {
        strcat(buffer, "(");
        switch ((int)*value) {
          case 0:
            strcat(buffer, "Not Connected");
            break;
          case 1:
            strcat(buffer, "Pending");
            break;
          case 2:
            strcat(buffer, "Connected");
            break;
          case 3:
            strcat(buffer, "Unknown Failure");
            break;
          case 4:
            strcat(buffer, "Association Failed");
            break;
          case 5:
            strcat(buffer, "Handshake Failed");
            break;
          case 6:
            strcat(buffer, "Echo Failed");
            break;
          case 7:
            strcat(buffer, "SSID Not Found");
            break;
        }
        strcat(buffer, ")");
        break;
      }
    case 65013: // ASR State
      {
        switch ((int)*value) {
          case 0:
            strcat(buffer, "(Rebooted)");
            break;
          case 1:
            strcat(buffer, "(Linked)");
            break;
          case 2:
            strcat(buffer, "(Updating)");
            break;
          case 3:
            strcat(buffer, "(Update Ready)");
            break;
          case 4:
            strcat(buffer, "(Initialized)");
            break;
          case 5:
            strcat(buffer, "(Re-Linked)");
            break;
        }
      }
      break;
    case 65014: // Low Battery Warning
      {
        switch ((int)value[0]) {
          case 0:
            strcat(buffer, "(Power OK)");
            break;
          case 1:
            strcat(buffer, "(2.5-2.7V)");
            break;
          case 2:
            strcat(buffer, "(2.3-2.5V)");
            break;
          case 3:
            strcat(buffer, "(2.1-2.3V)");
            break;
        }
      }
      break;
    case  1201: // ASR UTC Time (if enabled in APE)
    case 65015: // Linked Timestamp
      {
        strcat(buffer, "(");
        //        Serial.print("ts=");
        //        Serial.println(*((long *)value), HEX);
        strcat(buffer, timeString(*((long *)value)));
        strcat(buffer, ")");
      }
      break;
    case 65019: // Reboot Reason
      {
        // need to be able to determine ASR-1 or ASR-2 here because reboot reasons differ
# if 0
        // for ASR-1:
        if (value[1] != '0') strcat(buffer, "(");
        if ((value[1] - '0') & 1) strcat(buffer, "Reset Pin ");
        if ((value[1] - '0') & 2) strcat(buffer, "Watchdog ");
        if ((value[1] - '0') & 4) strcat(buffer, "Software ");
        if ((value[1] - '0') & 8) strcat(buffer, "CPU Lockup ");
        if (value[1] != '0') strcat(buffer, ")");
#else
        // for ASR-2:
        switch ((int)value[1] - '0') {
          case 0:
            strcat(buffer, "(POR)");
            break;
          case 1:
            strcat(buffer, "(Return from backup mode)");
            break;
          case 2:
            strcat(buffer, "(Watchdog)");
            break;
          case 3:
            strcat(buffer, "(Software Reset)");
            break;
          case 4:
            strcat(buffer, "(Reset Pin)");
            break;
          case 7:
            strcat(buffer, "(32.768KHz crystal fault)");
            break;
        }
#endif
      }
      break;
    case 65021: // MCU Interface
      {
        switch ((int)*value) {
          case 0:
            strcat(buffer, "(no MCU)");
            break;
          case 1:
            strcat(buffer, "(SPI)");
            break;
          case 2:
            strcat(buffer, "(UART)");
            break;
          default:
            sprintf(dataBuf, "(unknown (%d))", (int)*value);
            strcat(buffer, dataBuf);
            break;
        }
      }
      break;
    default:
      switch (getAttrType(attributeId)) {
        case ATTRIBUTE_TYPE_SINT8:
          sprintf(dataBuf, "(%hi)", *((uint8_t *)value));
          strcat(buffer, dataBuf);
          break;
        case ATTRIBUTE_TYPE_SINT16:
          sprintf(dataBuf, "(%i)", *((uint16_t *)value));
          strcat(buffer, dataBuf);
          break;
        case ATTRIBUTE_TYPE_SINT32:
          sprintf(dataBuf, "(%li)", *((unsigned long *)value));
          strcat(buffer, dataBuf);
          break;
        case ATTRIBUTE_TYPE_SINT64:
          sprintf(dataBuf, "(%lli)", *((unsigned long long *)value));
          strcat(buffer, dataBuf);
          break;
      }
      break;
  }

  return buffer;
}

