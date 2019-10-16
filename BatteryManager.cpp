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

#include "BatteryManager.h"
#include "lifeblue.h"

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
  return (unsigned long)(((hexToInt(hex))) | (hexToInt(hex + 4) << 16));
}

extern "C" {
  void _bm_char_callback(BLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify)
  {
    char *stream;
    BLEClient *client;
    BatteryManager *batteryManager;
    batteryInfo_t *currentBattery;
    static bool capturing = false;
    static bool fullPacket = false;
    
    batteryManager = BatteryManager::instance(NULL);
    currentBattery = batteryManager->getCurrentBattery();
    
    client = batteryManager->getBLEClient();
    
    if(currentBattery == NULL) {
      Serial.println(" - Failed to get current battery in notification callback");

      if(client->isConnected()) {
        client->disconnect();
      }
      
      return;
    }
   
    stream = (char *)data;
       
    for(int i = 0; i < length; i++) {
        
        currentBattery->buffer->push(stream[i]);
        
        if(currentBattery->buffer->first() == (char)0x87) {

          currentBattery->buffer->shift(); // get rid of 0x87
          currentBattery->characteristicHandle = 0;
                
          if(client->isConnected()) {
            client->disconnect();
          }

          Serial.println("");
          
          batteryManager->processBuffer();
          
          currentBattery->buffer->clear();
          batteryManager->setCurrentBattery(NULL);
            
          return;
        }

    }

    Serial.print('.');
  }
}

BatteryManager *BatteryManager::m_instance = NULL;

void BatteryManager::processBuffer()
{
  char buf[9];

  if(!currentBattery) {
    return;
  }

  currentBattery->voltage = convertBufferStringToValue(8);
  currentBattery->current = convertBufferStringToValue(8);
  currentBattery->ampHrs = convertBufferStringToValue(8);
  currentBattery->cycleCount = convertBufferStringToValue(4);
  currentBattery->soc = convertBufferStringToValue(4);
  currentBattery->temp = convertBufferStringToValue(4) - 2731;
  currentBattery->status = convertBufferStringToValue(8);
  currentBattery->afeStatus = convertBufferStringToValue(8);

  Serial.printf("Voltage: %lu (V)\nCurrent: %lu (A)\nAmp Hrs: %l\nCycles: %u\nSOC: %u (%%)\nTemp: %u (C)\n",
                currentBattery->voltage, currentBattery->current, currentBattery->ampHrs,
                currentBattery->cycleCount, currentBattery->soc, currentBattery->temp);
  
}

int16_t BatteryManager::convertBufferStringToValue(uint8_t len)
{
  char buf[9] = {NULL};

  if((len != 8) && (len != 4)) {
    return -1;
  }

  for(int i = 0; (i < len) && !currentBattery->buffer->isEmpty(); i++) {
    buf[i] = currentBattery->buffer->shift();
  }
 
  switch(len) {
    case 4:
      return (int16_t)hexToInt(buf);
    case 8:
      return (int16_t)hexToLong(buf);
  }

  return -1;
}

batteryInfo_t *BatteryManager::getCurrentBattery()
{
  return currentBattery;
}

BLEClient *BatteryManager::getBLEClient()
{
  return client;
}

void BatteryManager::setCurrentBattery(batteryInfo_t *b) {
  currentBattery = b;
}

batteryInfo_t *BatteryManager::getBatteryByCharacteristic(uint16_t handle)
{
  for(int i = 0; i < totalBatteries; i++) {
    if(batteryData[i]->characteristicHandle == handle) {
      return batteryData[i];
    }
  }

  return NULL;
}

BatteryManager::BatteryManager(uint8_t mb)
{
  maxBatteries = mb;
  totalBatteries = 0;
  
  batteryData = (batteryInfo_t **)os_zalloc(maxBatteries * sizeof(batteryInfo_t *));

  for(int i = 0; i < maxBatteries; i++) {
    batteryData[i] = (batteryInfo_t *)os_zalloc(sizeof(batteryInfo_t));
    batteryData[i]->buffer = new CircularBuffer<char, 512>();
  }
}

BatteryManager *BatteryManager::instance(uint8_t mb)
{
  if(!m_instance) {
    m_instance = new BatteryManager(mb);
  }

  return m_instance;
}


void BatteryManager::reset()
{
  Serial.println("Resetting batteries");
  Serial.printf("Max Batteries: %d\n", maxBatteries);
  
   if(batteryData) {
     for(int i = 0; i < maxBatteries; i++) {
       if(batteryData[i]->device) {
          delete batteryData[i]->device;
       }

       if(batteryData[i]->buffer) {
          delete batteryData[i]->buffer;
       }

       free(batteryData[i]);
     }
   }

   free(batteryData);
   
   batteryData = (batteryInfo_t **)os_zalloc(maxBatteries * sizeof(batteryInfo_t *));

   for(int i = 0; i < maxBatteries; i++) {
     batteryData[i] = (batteryInfo_t *)os_zalloc(sizeof(batteryInfo_t));
      batteryData[i]->buffer = new CircularBuffer<char, 512>();
   }
   
   totalBatteries = 0;

   Serial.println("All done reset");
}

bool BatteryManager::addBattery(BLEAdvertisedDevice *device)
{
  if(totalBatteries == maxBatteries) {
    Serial.printf("Cannot add battery, maximum of %d reached.\n", maxBatteries);
    return false;
  }

  batteryData[totalBatteries]->device = device;
  
  totalBatteries++;

  Serial.printf("- Added LiFeBlue battery (%s): %s\n", device->getAddress().toString().c_str(), device->getName().c_str());
  return true;
}

void BatteryManager::loop()
{
  BLERemoteService *remoteService;
  BLERemoteCharacteristic *characteristic;

  if(client) {
    if(client->isConnected()) {
      return;
    }
  }
  
  if(pollingQueue.isEmpty()) {
    Serial.println("- Queue empty, adding batteries back to be polled");
    for(int i = 0; i < totalBatteries; i++) {
      pollingQueue.push(batteryData[i]);
    }
  }

  if(!pollingQueue.isEmpty()) {
    currentBattery = pollingQueue.pop();

    Serial.printf(" - Connecting to Battery: %s\n", currentBattery->device->getAddress().toString().c_str());
    
    if(client) {
      delete client;
      client = NULL;
    }
    
    client = BLEDevice::createClient();
    
    if(!client->connect(currentBattery->device)) {
      Serial.println(" - Failed to connect to battery, requeuing");
      delete client;
      client = NULL;
      pollingQueue.push(currentBattery);
      return;
    }

    remoteService = client->getService(serviceUUID);

    if(remoteService == nullptr) {
      Serial.println(" - FAILURE: Could not find service UUID");
      client->disconnect();
      delete client;
      client = NULL;
      pollingQueue.push(currentBattery);
      return;
    }

    characteristic = remoteService->getCharacteristic(charUUID);

    if(characteristic == nullptr) {
      Serial.println(" - FAILURE: Could not find characteristic UUID");
      client->disconnect();
      delete client;
      client = NULL;
      pollingQueue.push(currentBattery);
      return;
    }

    if(!characteristic->canNotify()) {
      Serial.println(" - FAILURE: characteristic UUID cannot notify");
      client->disconnect();
      delete client;
      client = NULL;
      pollingQueue.push(currentBattery);  
      return;
    }
  
    currentBattery->characteristicHandle = characteristic->getHandle();
    characteristic->registerForNotify(_bm_char_callback);
   
  }
  
  delay(2000);
}
