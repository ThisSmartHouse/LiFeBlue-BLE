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

  /**
   * This callback is what is called when the battery we are connected to sends use a Bluetooth Notification
   * via the proper characteristic. Each notification is only a fragment of the total data packet.
   * 
   * So what we have to do here is store the data as it comes in as chuncks until we hit the magic 0x87
   * character. This character indicates the start of the data stream and we know we've captured a full packet!
   * 
   * Although it's probably not strictly necessary and a bit of a resource waste, this implementation uses
   * a separate buffer for each battery. I may refactor it someday to use a single buffer since we never maintain
   * a connection to multiple batteries at once (I found the ESP32 BLE library pretty buggy when I tried).
   * 
   * Once we have a full packet we call processBuffer(), which extracts the data and stores it with that
   * particular device.
   */
  void _bm_char_callback(BLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool isNotify)
  {
    char *stream;
    BLEClient *client;
    BatteryManager *batteryManager;
    batteryInfo_t *currentBattery;
    
    batteryManager = BatteryManager::instance();
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
  }
}

/**
 * Initialize our m_instance variable to NULL so we create an instance of our BatteryManager singleton
 * when we first start.
 */
BatteryManager *BatteryManager::m_instance = NULL;

/**
 * Process the current battery's buffer. The battery's transmit their data as ASCII hexadecimal values
 * in big endian format. The format of the buffer is as follows:
 * 
 * V_byte1H, Vbyte1L, V_byte2H, V_byte2L, V_byte3H, V_byte3L, V_byte4H, V_byte4L  // Voltage (in mV)
 * C_byte1H, C_byte1L, C_byte2H, C_byte2L, C_byte3H, C_byte3L, C_byte4H, C_byte4L // Current Draw (in mA)
 * A_byte1H, A_byte1L, A_byte2H, A_byte2L, A_byte3H, A_byte3L, A_byte4H, A_byte4L // AmpHrs (in mAh)
 * CY_byte1H, CY_byte1L, CY_byte2H, CY_byte2L // Total Cycles
 * S_byte1H, S_byte1L, S_byte2H, S_byte2L // State of Charge (% remaining)
 * T_byte1H, T_byte1L, T_byte2H, T_byte2L // Temperature (need to subtract 2731) i.e. (2931 - 2731) / 10 = 20.0C
 * BS_byte1H, BS_byte1L, BS_byte2H, BS_byte2L // Status of Battery (bitmask)
 * AFE_byte1H, AFE_byte1L, AFE_byte2H, AFE_byte2L // AFE status (bitmask)
 * 
 * Following this is the data for each individual cell of the battery in the format:
 * 
 * CELL_byte1H, CELL_byte1L, CELL_byte2H, CELL_byte2L // Cell voltage (in mV)
 * 
 * for as many cells as the battery has (mine has 4)
 * 
 * Finally we have the checksum:
 * 
 * SUM_byte1H, SUM_byte1L, SUM_byte2H, SUM_byte2L
 * 
 * At the moment I don't try to confirm the values we've read with the checksum, maybe later.
 * 
 * For the status fields (status and afeStatus) these are bitmasks. I don't know what all of the bits represent
 * but I know a good portion of them which I have pulled out as booleans in the status and provided matching
 * defines for.
 */
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
  currentBattery->status = convertBufferStringToValue(4);
  currentBattery->afeStatus = convertBufferStringToValue(4);

  for(int i = 0; i < totalCells; i++) {
    currentBattery->cells[i] = convertBufferStringToValue(4);
  }

  currentBattery->cell_high_voltage = (currentBattery->status & LIFE_CELL_HIGH_VOLTAGE);
  currentBattery->cell_low_voltage = (currentBattery->status  & LIFE_CELL_LOW_VOLTAGE);
  currentBattery->over_current_when_charge = (currentBattery->status & LIFE_OVER_CURRENT_WHEN_CHARGE);
  currentBattery->over_current_when_discharge = (currentBattery->status & LIFE_OVER_CURRENT_WHEN_DISCHARGE);
  currentBattery->low_temp_when_discharge = (currentBattery->status & LIFE_LOW_TEMP_WHEN_DISCHARGE);
  currentBattery->low_temp_when_charge = (currentBattery->status & LIFE_LOW_TEMP_WHEN_CHARGE);
  currentBattery->high_temp_when_discharge = (currentBattery->status & LIFE_HIGH_TEMP_WHEN_DISCHARGE);
  currentBattery->high_temp_when_charge = (currentBattery->status & LIFE_HIGH_TEMP_WHEN_CHARGE);
  
  currentBattery->short_circuited = (currentBattery->afeStatus & LIFE_SHORT_CIRCUITED);
  
  DEBUG_DUMP_BATTERYINFO(currentBattery);
}

/**
 * Helper method. Given a length of ASCII data (in bytes) from the buffer,
 * it extracts the numeric value it represents. Since the data comes across
 * to use in big endian format we take advantage of the __builtin_bswap* family
 * of functions available in GCC here. Normally I'd avoid using compiler-specific
 * things but we're not going to be compiling anything for ESP32s using anything
 * but a GCC compiler here.
 */
uint32_t BatteryManager::convertBufferStringToValue(uint8_t len)
{
  char buf[9] = {NULL};

  if((len != 8) && (len != 4)) {
    return -1;
  }

  for(int i = 0; (i < len) && !currentBattery->buffer->isEmpty(); i++) {
    buf[i] = currentBattery->buffer->shift();
  }

  switch(strlen(buf)) {
    case 4:
      return __builtin_bswap16(strtoul(buf, NULL, 16));
    case 8:
      return __builtin_bswap32(strtoul(buf, NULL, 16));
  }

  return -1;
}

uint8_t BatteryManager::getTotalCells()
{
  return totalCells;
}

batteryInfo_t *BatteryManager::getBattery(uint8_t idx)
{
  if(idx >= totalBatteries) {
    return NULL;
  }

  return batteryData[idx];
}

uint8_t BatteryManager::getTotalBatteries()
{
  return totalBatteries;
}

/**
 * Returns the current battery being processed
 */
batteryInfo_t *BatteryManager::getCurrentBattery()
{
  return currentBattery;
}

/**
 * Returns the instance of BLEClient we are presently using
 */
BLEClient *BatteryManager::getBLEClient()
{
  return client;
}

/**
 * Sets the current battery being processed, or NULL
 */
void BatteryManager::setCurrentBattery(batteryInfo_t *b) {
  currentBattery = b;
}

/**
 * Private constructor. There can be only one instance of this class
 * so it's implemented as a singleton. First parameter is maximum batteries
 * to keep track of, the second is the number of cells for each battery.
 * 
 * Note: We do not support a mixture of batteries with different numbers of
 * cells in each.
 */
BatteryManager::BatteryManager(uint8_t mb, uint8_t tc)
{
  maxBatteries = mb;
  totalBatteries = 0;
  totalCells = (tc > MAX_BATTERY_CELLS) ? MAX_BATTERY_CELLS : tc;
  
  batteryData = (batteryInfo_t **)os_zalloc(maxBatteries * sizeof(batteryInfo_t *));

  for(int i = 0; i < maxBatteries; i++) {
    batteryData[i] = (batteryInfo_t *)os_zalloc(sizeof(batteryInfo_t));
    batteryData[i]->buffer = new CircularBuffer<char, 512>();
  }
}

/**
 * Retrieve a (new) instance of the BatteryManager
 */
BatteryManager *BatteryManager::instance(uint8_t mb, uint8_t tc)
{
  if(!m_instance) {
    m_instance = new BatteryManager(mb, tc);
  }

  return m_instance;
}

/**
 * Retrieve the existing instance of the battery manager
 */
BatteryManager *BatteryManager::instance()
{
  return instance(NULL, NULL);
}

/**
 * Reset the battery manager internal data structures
 */
void BatteryManager::reset()
{
  Serial.println("- Resetting BatteryManager");
    
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
}

/**
 * Add another battery device to our monitoring, we get the
 * device as a result of a BLE scan in the main codebase.
 */
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

/**
 * The main loop function. The way this works is the manager has a list
 * of batteries it's monitoring and uses a stack to do them one at a time. If
 * for some reason we can't connect to the battery right away (happens from time to time),
 * we push the battery back on the stack to be re-processed. Otherwise, we pop the next battery
 * off the stack and process it until there are no batteries left to process. At that point,
 * we loop through all of the batteries and push them all back on the stack.
 * 
 */
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
    for(int i = 0; i < totalBatteries; i++) {
      pollingQueue.push(batteryData[i]);
    }
  }

  if(!pollingQueue.isEmpty()) {
    currentBattery = pollingQueue.pop();

    Serial.printf("\n- Connecting to Battery: %s\n", currentBattery->device->getAddress().toString().c_str());
    
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
