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

#ifndef LIFEBLUE_H_
#define LIFEBLUE_H_

#include <BLEDevice.h>
#include "BatteryManager.h"

#define CLIENT_DEVICE_NAME "lifeblue-client"
#define MAX_BATTERIES 10

// The service UUID of the LiFeBlue battery
static BLEUUID serviceUUID((uint16_t)0xffe0);
static BLEUUID    charUUID((uint16_t)0xffe4);

int16_t getBufferValue(uint8_t);
void processBuffer();
static void notifyCallback(BLERemoteCharacteristic *, uint8_t *, size_t, bool);
void onBLEScanComplete(BLEScanResults);

#endif
