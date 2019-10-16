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

extern "C" {
  void _bm_char_callback(BLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify)
  {
    char *stream;
    BatteryManager *batteryManager;
    batteryInfo_t *currentBattery;
    
    batteryManager = BatteryManager::instance(NULL);
    currentBattery = batteryManager->getBatteryByCharacteristic(characteristic->getHandle());

    if(currentBattery == NULL) {
      Serial.println("Failed to get current battery in notification callback");
      return;
    }
   
    stream = (char *)data;
       
    for(int i = 0; i < length; i++) {
      currentBattery->buffer->push(stream[i]);
    }

    if(currentBattery->buffer->first() == (char) 0x87) {
      Serial.printf("Captured Full data packet for battery (%ld bytes)\n", currentBattery->buffer->size());
      Serial.printf("Disconnecting from %s\n", currentBattery->client->getPeerAddress().toString().c_str());
      
      currentBattery->characteristicHandle = 0;

      if(currentBattery->client->isConnected()) {
        currentBattery->client->disconnect();
      }
      
      //delete currentBattery->client;

      currentBattery->client = NULL;
      currentBattery->buffer->clear();
      batteryManager->setPolling(false);
      batteryManager->setCurrentBattery(NULL);
      
    }
  }
}

BatteryManager *BatteryManager::m_instance = NULL;

void BatteryManagerClientCallbacks::onConnect(BLEClient *client)
{
  Serial.printf("Client for %s connected\n", client->getPeerAddress().toString().c_str());
}

void BatteryManagerClientCallbacks::onDisconnect(BLEClient *client)
{
  Serial.printf("Client for %s disconnected\n", client->getPeerAddress().toString().c_str());
}

void BatteryManager::setCurrentBattery(batteryInfo_t *b) {
  currentBattery = b;
}

void BatteryManager::setPolling(bool b) {
  isPolling = b;
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

BatteryManagerClientCallbacks::BatteryManagerClientCallbacks(BatteryManager *b)
{
  batteryManager = b;
}

BatteryManager::BatteryManager(uint8_t mb)
{
  maxBatteries = mb;
  totalBatteries = 0;
  clientCallbacks = new BatteryManagerClientCallbacks(this);

  batteryData = (batteryInfo_t **)os_zalloc(maxBatteries * sizeof(batteryInfo_t *));

  for(int i = 0; i < maxBatteries; i++) {
    batteryData[i] = (batteryInfo_t *)os_zalloc(sizeof(batteryInfo_t));
    batteryData[i]->buffer = new CircularBuffer<char, 200>();
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
      batteryData[i]->buffer = new CircularBuffer<char, 200>();
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

bool BatteryManager::pollBattery()
{
  BLERemoteService *remoteService;
  BLERemoteCharacteristic *characteristic;

  isPolling = true;
  
  if(!currentBattery) {
    isPolling = false;  
    return false;
  }

  Serial.printf(" - Polling %s\n", currentBattery->device->getAddress().toString().c_str());
  
  currentBattery->client = BLEDevice::createClient();
  currentBattery->client->setClientCallbacks(clientCallbacks);
  
  if(!currentBattery->client->connect(currentBattery->device)) {
    isPolling = false;
    delete currentBattery->client;
    currentBattery->client = NULL;
    return false;  
  }
  
  Serial.println(" - Connected to Battery");
  
  remoteService = currentBattery->client->getService(serviceUUID);
  if(remoteService == nullptr) {
    Serial.println(" - FAILURE: Could not find service UUID");
    
    currentBattery->client->disconnect();
    delete currentBattery->client;
    currentBattery->client = NULL;
    isPolling = false;
    return false;
  }

  characteristic = remoteService->getCharacteristic(charUUID);

  if(characteristic == nullptr) {
    Serial.println(" - FAILURE: Could not find characteristic UUID");
    
    currentBattery->client->disconnect();
    delete currentBattery->client;
    currentBattery->client = NULL;
    isPolling = false;
    return false;
  }

  if(!characteristic->canNotify()) {
    Serial.println(" - FAILURE: characteristic UUID cannot notify");

    currentBattery->client->disconnect();
    delete currentBattery->client;
    currentBattery->client = NULL;
    isPolling = false;
    return false;
  }
  
  currentBattery->characteristicHandle = characteristic->getHandle();
  characteristic->registerForNotify(_bm_char_callback);
}

void BatteryManager::loop()
{
  if(pollingQueue.isEmpty() && !isPolling) {
    Serial.println("Queue empty, adding batteries back to be polled");
    for(int i = 0; i < totalBatteries; i++) {
      pollingQueue.push(batteryData[i]);
    }
  }

  if(!pollingQueue.isEmpty() && !isPolling) {
    currentBattery = pollingQueue.pop();
    pollBattery();
  }
  
  delay(2000);
}
