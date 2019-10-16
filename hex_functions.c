/*
  +----------------------------------------------------------------------+
  | LiFeBlue Bluetooth Interface                                         |
  +----------------------------------------------------------------------+
  | Copyright (c) 2017-2019 Internet Technology Solutions, LLC,          |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License");      |
  | you may not use this file except in compliance with the License. You |
  | may obtain a copy of the License at:                                 |
  |                                                                      |
  | http://www.apache.org/licenses/LICENSE-2.0                           |
  |                                                                      |
  | Unless required by applicable law or agreed to in writing, software  |
  | distributed under the License is distributed on an "AS IS" BASIS,    |
  | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or      |
  | implied. See the License for the specific language governing         |
  | permissions and limitations under the License.                       |
  +----------------------------------------------------------------------+
  | Authors: John Coggeshall <john@thissmarthouse.com>                   |
  +----------------------------------------------------------------------+
*/

#include "hex_functions.h"

/**
   Helper functions to convert the data stream we get from the battery
   (ASCII values sent as hex) and convert them to either unsigned longs
   or uninsigned int values. This requires first converting the two ASCII
   bytes into a 0-255 value, then shifting everything around to produce
   the current unsigned long or unsigned int value as shown:

   For example:

    58920100 (directly taken from bluetooth stream)
    5892 0100 (split into bytes)
    9258 0001 (swap the bits within each byte)
    0001 9258 (swap the bytes with each other)
    0x19258 = 103000 (hex) = 103000mAh
*/
inline byte hexToByte(char * hex) {
  byte high = hex[0];
  byte low = hex[1];

  if (high > '9') {
    high -= (('A' - '0') - 0xA);
  }
  if (low > '9') {
    low -= (('A' - '0') - 0xA);
  }

  return (((high & 0xF) << 4) | (low & 0xF));
}

inline unsigned int hexToInt(char * hex) {
  return ((hexToByte(hex)) | (hexToByte(hex + 2) << 8));
}

// This is broken, not switching words.
inline unsigned long hexToLong(char * hex) {
  return ((((unsigned long)hexToInt(hex +4 ))) | (hexToInt(hex) << 16));
}
