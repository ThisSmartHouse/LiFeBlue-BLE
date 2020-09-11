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
 * Various bit values for each flag in either the status or afeStatus variable
 */
#define LIFE_CELL_HIGH_VOLTAGE 0x80
#define LIFE_CELL_LOW_VOLTAGE 0x40
#define LIFE_OVER_CURRENT_WHEN_CHARGE 0x20
#define LIFE_OVER_CURRENT_WHEN_DISCHARGE 0x10
#define LIFE_LOW_TEMP_WHEN_DISCHARGE 0x8
#define LIFE_LOW_TEMP_WHEN_CHARGE 0x4
#define LIFE_HIGH_TEMP_WHEN_DISCHARGE 0x2
#define LIFE_HIGH_TEMP_WHEN_CHARGE 0x1
#define LIFE_SHORT_CIRCUITED 0x20  // changed from 0x10 to 0x20 per data sheet – JR

/**
 * This is the struct that holds the "raw" data from the battery
 * that gets updated as we receive new stats.
 */
struct batteryInfo_t {
  
  BLEAdvertisedDevice *device = NULL;
  uint16_t  characteristicHandle;
  CircularBuffer<char, 512> *buffer = NULL;
  
  uint32_t voltage; // voltage in mV
  int32_t  current; // current in mA  -- Needs a signed int to hold negative values – JR
  uint32_t ampHrs;  // ampHrs in mAh
  uint16_t cycleCount; // cycles
  uint16_t soc; // State of Charge (%)
  uint16_t temp; // Temperature in C
  uint16_t status; // Status Bitmask
  uint16_t afeStatus; // afeStatus Bitmask
  uint16_t cells[MAX_BATTERY_CELLS];

  bool cell_high_voltage = false;
  bool cell_low_voltage = false;
  bool over_current_when_charge = false;
  bool over_current_when_discharge = false;
  bool low_temp_when_discharge = false;
  bool low_temp_when_charge = false;
  bool high_temp_when_discharge = false;
  bool high_temp_when_charge = false;
  bool short_circuited = false;
  
};


// Handy debug dump function that dumps out our struct so we can see
#define DEBUG_DUMP_BATTERYINFO(_i) \
   Serial.printf("BatteryInfo for %s\n", _i->device->getAddress().toString().c_str()); \
   Serial.println("-=-=-=-=-=-=-=-=-=-"); \
   Serial.printf("Voltage: %.2fV\n", ((float)_i->voltage) / 1000); \
   Serial.printf("Current: %.2fA\n", ((float)_i->current) / 1000); \
   Serial.printf("Amp Hrs: %.2fAh\n", ((float)_i->ampHrs) / 1000); \
   Serial.printf("Cycles: %u\n", _i->cycleCount); \
   Serial.printf("SoC: %u%%\n", _i->soc); \
   Serial.printf("Temp: %.1f (C) %.2f (F)\n", ((float)_i->temp) / 10, (((float)_i->temp) / 10) * 1.8 + 32); \
   for(int i = 0; i < totalCells; i++) Serial.printf("%lu (mV) ", (unsigned long int)_i->cells[i]); \
   Serial.printf("\nCell High Voltage: %s\n", _i->cell_high_voltage ? "X" : "-"); \
   Serial.printf("Cell Low Voltage: %s\n", _i->cell_low_voltage ? "X" : "-"); \
   Serial.printf("Over Current When Charge: %s\n", _i->over_current_when_charge ? "X" : "-"); \
   Serial.printf("Over Current When Discharge: %s\n", _i->over_current_when_discharge ? "X" : "-"); \
   Serial.printf("Low Temp When Charge: %s\n", _i->low_temp_when_charge ? "X" : "-"); \
   Serial.printf("Low Temp When Discharge: %s\n", _i->low_temp_when_discharge ? "X" : "-"); \
   Serial.printf("High Temp When Charge: %s\n", _i->high_temp_when_charge ? "X" : "-"); \
   Serial.printf("High Temp When Discharge: %s\n", _i->high_temp_when_discharge ? "X" : "-"); \
   Serial.printf("Is Short Circuited: %s\n", _i->short_circuited ? "X" : "-"); \
   Serial.printf("");

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
    batteryInfo_t *getCurrentBattery();
    batteryInfo_t *getBattery(uint8_t);
    uint8_t getTotalBatteries();
    uint8_t getTotalCells();
    BLEClient *getBLEClient();
    void processBuffer();
    
    static BatteryManager *instance(uint8_t, uint8_t);
    static BatteryManager *instance();
    
private:
    BatteryManager(uint8_t, uint8_t);
    BatteryManager(BatteryManager const &) {};
    BatteryManager& operator=(BatteryManager const &) { };

    bool isValidChecksum();
      
    int32_t convertBufferStringToValue(uint8_t);  // Return a int32_t so we can capture the signed current value.
                                                  // Need to cast the Voltage and AmpHrs calls to (uint32_t)
    
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
