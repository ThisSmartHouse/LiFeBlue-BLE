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

#include "wifi_config.h"

#ifndef LIFEBLUE_H_
#define LIFEBLUE_H_

#define CLIENT_DEVICE_NAME "lifeblue-client"

#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

#ifndef MAX_BATTERIES
#define MAX_BATTERIES 10
#endif

#ifndef CELLS_PER_BATTERY
#define CELLS_PER_BATTERY 4
#endif

#ifndef WIFI_CONNECT_ATTEMPTS
#define WIFI_CONNECT_ATTEMPTS 3
#endif


#define MQTT_OBJECT_SIZE JSON_ARRAY_SIZE(4) + 2*JSON_OBJECT_SIZE(9)

// The service UUID of the LiFeBlue battery
static BLEUUID serviceUUID((uint16_t)0xffe0);
static BLEUUID    charUUID((uint16_t)0xffe4);

void onBLEScanComplete(BLEScanResults);
void connectWiFi();
void startDeviceScan();
void IRAM_ATTR onScanTimer();
#endif
