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

#ifndef LIFEBATTERYMGR_H_
#define LIFEBATTERYMGR_H_

#include "BLEAdvertisedDevice.h"
#include "Arduino.h"
#include "lifeblue.h"
#include "os.h"
#include <BLEDevice.h>
#include <CircularBuffer.h>

#ifndef MAX_BATTERY_CELLS
#define MAX_BATTERY_CELLS 16
#endif

/**
   This is the struct that holds the "raw" data from the battery
   that gets updated as we receive new stats.
*/
typedef struct batteryInfo_t {
  
  BLEAdvertisedDevice *device = NULL;
  uint16_t  characteristicHandle;
  CircularBuffer<char, 512> *buffer = NULL;
  
  uint32_t voltage; // voltage in mV
  uint32_t current; // current in mA
  uint32_t ampHrs;  // ampHrs in mAh
  uint16_t cycleCount; // cycles
  uint16_t soc; // State of Charge (%)
  uint16_t temp; // Temperature in C
  uint16_t status; // Status Bitmask
  uint16_t afeStatus; // afeStatus Bitmask
  uint16_t cells[MAX_BATTERY_CELLS];
  
};

#define DEBUG_DUMP_BATTERYINFO(_i) \
   Serial.println("BatteryInfo for %d", _i); \
   Serial.println("-=-=-=-=-=-=-=-=-=-"); \
   Serial.printf("Has Device: %s\n", (batteryData[_i]->device) ? "Y" : "N"); \
   Serial.printf("Has Client: %s\n", (batteryData[_i]->client) ? "Y" : "N"); \
   Serial.printf("Handle: %d\n", batteryData[_i]->handle); \
   Serial.printf("Voltage: %ld\n", batteryData[_i]->voltage); \
   Serial.printf("Current: %ld\n", batteryData[_i]->current); \
   Serial.printf("Amp Hrs: %ld\n", batteryData[_i]->ampHrs); \
   Serial.printf("Cycles: %ld\n", batteryData[_i]->cycleCount); \
   Serial.printf("SoC: %d%%\n", batteryData[_i]->soc); \
   Serial.printf("Temp: %d (C)\n", batteryData[_i]->temp);

extern "C" {
  void _bm_char_callback(BLERemoteCharacteristic *, uint8_t *, size_t, bool);
}

class BatteryManager
{
  
public:
    
    ~BatteryManager();
    bool addBattery(BLEAdvertisedDevice *);
    void reset();
    void loop();
    
    void setCurrentBattery(batteryInfo_t *);
    batteryInfo_t *getBatteryByCharacteristic(uint16_t);
    batteryInfo_t *getCurrentBattery();
    BLEClient *getBLEClient();
    void processBuffer();
    
    static BatteryManager *instance(uint8_t, uint8_t);
    static BatteryManager *instance();
    
private:
    BatteryManager(uint8_t, uint8_t);
    BatteryManager(BatteryManager const &) {};
    BatteryManager& operator=(BatteryManager const &) {};

    uint32_t convertBufferStringToValue(uint8_t);
    
    BLEClient *client = NULL;
           
    uint8_t maxBatteries = 0;
    uint8_t totalBatteries = 0;
    uint8_t totalCells = 0;
    
    batteryInfo_t **batteryData = NULL;
    batteryInfo_t *currentBattery = NULL;
            
    CircularBuffer<batteryInfo_t *, 50> pollingQueue;
    
    static BatteryManager *m_instance;
};

#endif
