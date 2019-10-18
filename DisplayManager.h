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

#ifndef LIFEDISPLAYMGR_H_
#define LIFEDISPLAYMGR_H_

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BatteryManager.h"
#include <WiFi.h>

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 128
#endif

#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 64
#endif

#ifndef OLED_RESET_PIN
#define OLED_RESET_PIN -1
#endif

#ifndef OLED_ADDRESS
#define OLED_ADDRESS 0x3C
#endif

class BatteryManager;

class DisplayManager
{

public:
  void setup();

  void scanningScreen(uint8_t);
  void statusScreen();
  void wifiConnectScreen(uint8_t);

  static DisplayManager *instance();

private:
  DisplayManager();
  DisplayManager(DisplayManager const &) {};
  DisplayManager& operator=(DisplayManager const &) { };
  
  void drawProgress(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
    
  static DisplayManager *m_instance;
  Adafruit_SSD1306 *display;
  BatteryManager *batteryManager;
  uint8_t currentBattery;
};

#endif
