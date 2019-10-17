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

#include <CircularBuffer.h>
#include "lifeblue.h"

BatteryManager *batteryManager;
BLEScan *bleScanner;
bool scanning = false;

/**
 * Called after a BLE scan is complete where we look through the results
 * and pull any batteries we find out. We add them to the Battery Manager
 * as devices, which it then uses to connect to one at a time and get the
 * data from.
 */
void onBLEScanComplete(BLEScanResults results)
{
   batteryManager->reset();
      
   for(int i = 0; i < results.getCount(); i++) {
     
      if(!results.getDevice(i).haveServiceUUID()) {
       continue;
      }

      if(!results.getDevice(i).isAdvertisingService(serviceUUID)) {
       continue;
      }

      batteryManager->addBattery(new BLEAdvertisedDevice(results.getDevice(i)));
            
   }
   
   Serial.println("- Scan Complete");
   scanning = false;
   delete bleScanner;
}

/**
 * Simple helper function to configure a BLE scan to start
 */
void startDeviceScan() 
{
  Serial.println("- Starting BLE Device Scan...");
  
  scanning = true;
  
  bleScanner = BLEDevice::getScan();

  bleScanner->setInterval(1349);
  bleScanner->setWindow(449);
  bleScanner->setActiveScan(true);
  bleScanner->start(10, onBLEScanComplete, false);
  
}

/**
 * Initialize the program and start scanning for our batteries!
 */
void setup() {
  Serial.begin(115200);

  for(int i = 0; (i < 50000) && !Serial; i++) {
    delay(1);
  }

  Serial.println("LiFeBlue ESP32 Battery Monitor");
  Serial.println("(c) 2019 Internet Technology Solutions / This Smart House");
  Serial.println("http://www.thissmarthouse.com/lifeblue");
  Serial.printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n");

  batteryManager = BatteryManager::instance(MAX_BATTERIES, CELLS_PER_BATTERY);
  
  BLEDevice::init(CLIENT_DEVICE_NAME);

  Serial.println("- Initialized BLE Client");
  
  startDeviceScan();
  
}

/**
 * This function is called over and over again every cycle and where the bulk of the
 * program actually does it's real work. In our case we call the BatteryManager's loop
 * to update the status of all of our batteries and then we will transmit those statuses
 * via MQTT
 */
void loop() {

  if(scanning) {
    return;
  }

  batteryManager->loop();
}
