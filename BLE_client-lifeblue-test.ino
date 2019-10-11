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
#include "BLEDevice.h"
#include "CircularBuffer.h"

// The service UUID of the LiFeBlue battery
static BLEUUID serviceUUID((uint16_t)0xffe0);
// The notification characteristic that streams the battery data
static BLEUUID    charUUID((uint16_t)0xffe4);

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;


/**
 * This is the struct that holds the "raw" data from the battery
 * that gets updated as we receive new stats.
 */
typedef struct batteryInfo_t {
  unsigned long voltage; // voltage in mV
  unsigned long current; // current in mA
  unsigned long ampHrs;  // ampHrs in mAh
  unsigned int cycleCount; // cycles
  unsigned int soc; // State of Charge (%)
  unsigned int temp; // Temperature in C 
  unsigned long status; // Status Bitmask
  unsigned long afeStatus; // afeStatus Bitmask
};

batteryInfo_t batteryInfo;

// Buffer to store incoming data from the battery
CircularBuffer<char, 200> notifyBuffer;

/**
 * Helper functions to convert the data stream we get from the battery
 * (ASCII values sent as hex) and convert them to either unsigned longs
 * or uninsigned int values. This requires first converting the two ASCII
 * bytes into a 0-255 value, then shifting everything around to produce
 * the current unsigned long or unsigned int value as shown:
 * 
 * For example: 
 * 
 *  58920100 (directly taken from bluetooth stream)
 *  5892 0100 (split into bytes)
 *  9258 0001 (swap the bits within each byte)
 *  0001 9258 (swap the bytes with each other)
 *  0x19258 = 103000 (hex) = 103000mAh
 */
inline byte hexToByte(char* hex){
  byte high = hex[0];
  byte low = hex[1];
  
  if (high > '9'){
    high -= (('A'-'0')-0xA); 
  }
  if (low > '9'){
    low -= (('A'-'0')-0xA); 
  }
  
  return (((high & 0xF)<<4) | (low & 0xF));
}

inline unsigned int hexToInt(char* hex){
  return ((hexToByte(hex)) | (hexToByte(hex+2) << 8));
}

inline unsigned long hexToLong(char* hex){
  return ((((unsigned long)hexToInt(hex))) | (hexToInt(hex+4) << 16));
}

/**
 * Grab a chunk of the buffer and convert it into
 * either a unsigned int or unsigned long
 */
int16_t getBufferValue(uint8_t bytes) {
   char valueBuf[9] = {NULL};

   if(bytes > 8) {
      return -1;
   }
   
   for(int i = 0; (i < bytes) && !notifyBuffer.isEmpty(); i++) {
    valueBuf[i] = notifyBuffer.shift();
   }
   
   switch(bytes) {
      case 4:
        return (int16_t)hexToInt(valueBuf);
      case 8:
        return (int16_t)hexToLong(valueBuf);
   }
   
   return 0;
}

/**
 * This is called once we have encountered the 0x87 byte in the buffer, which
 * indicates the start of a new data block. Starting from there it reads what
 * follows to get the values into our struct
 */
void processBuffer()
{
   char valueBuf[9]; 
   
   notifyBuffer.shift(); // Get rid of 0x87
   
   batteryInfo.voltage = getBufferValue(8);
   batteryInfo.current = getBufferValue(8);
   batteryInfo.ampHrs = getBufferValue(8);
   batteryInfo.cycleCount = getBufferValue(4);
   batteryInfo.soc = getBufferValue(4);
   batteryInfo.temp = getBufferValue(4) - 2731;
   batteryInfo.status = getBufferValue(8);
   batteryInfo.afeStatus = getBufferValue(8);
   
   Serial.printf("Voltage: %lu (V)\nCurrent: %lu (A)\nAmp Hrs: %lu\nCycles: %u\nSOC: %u (%%)\nTemp: %u (C)\n", 
                 batteryInfo.voltage, batteryInfo.current, batteryInfo.ampHrs,
                 batteryInfo.cycleCount, batteryInfo.soc, batteryInfo.temp);
   

   notifyBuffer.clear();
}

/**
 * Callback when there is new notification data from the battery, which will consist of some bytes that
 * are a portion of a total data block. Stick them into a buffer until we get the full block.
 */
static void notifyCallback( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) 
{
    char *stream = (char *)pData;

    for(int i = 0; i < length; i++) {
      notifyBuffer.push(stream[i]);
    }
    
    if(notifyBuffer.first() == (char) 0x87) {
        processBuffer();
    }
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    doScan = true;
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);

    connected = true;
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } 
  } 
}; 


void setup() {
  Serial.begin(115200);
  Serial.println("Starting LiFeBlue BLE Client application...");
  BLEDevice::init("");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
} 


void loop() {

  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  if(doScan){
    BLEDevice::getScan()->start(0);
  }
  
  delay(1000);
} // End of loop
