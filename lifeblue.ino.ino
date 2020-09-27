/*
  +----------------------------------------------------------------------+
  | LiFeBlue Bluetooth Interface                                         |
  +----------------------------------------------------------------------+
  | Copyright (c) 2017-2020 Internet Technology Solutions, LLC,          |
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
#include <Wire.h>
#include <BLEDevice.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "lifeblue.h" 
#include "BatteryManager.h"
#include "DisplayManager.h"

#include "hex_dump.h"

//#define DUMP_HEX_BATTERY_BUFFER  // Comment this out to stop outputting the buffer in hex / ascii -- JR


BatteryManager *batteryManager;
BLEScan *bleScanner;
bool scanning = false;
DisplayManager *displayManager;
PubSubClient *mqttClient = NULL;
WiFiClient *wifiClient = NULL;

uint8_t wifiConnectAttempts = 0;

hw_timer_t *scanTimer = NULL;
portMUX_TYPE scanTimerMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool scanTimerTick = false;
uint8_t scanTotalInterrupts;

/**
 * Called after a BLE scan is complete where we look through the results
 * and pull any batteries we find out. We add them to the Battery Manager
 * as devices, which it then uses to connect to one at a time and get the
 * data from.
 */
void onBLEScanComplete(BLEScanResults results)
{
   //batteryManager->reset();
      
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

void IRAM_ATTR onScanTimer()
{
  portENTER_CRITICAL_ISR(&scanTimerMux);
  scanTimerTick = true;
  portEXIT_CRITICAL_ISR(&scanTimerMux);
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

  scanTimerTick = false;
  scanTotalInterrupts = 0;
  
  timerAlarmEnable(scanTimer);
  displayManager->scanningScreen(0);
  
  bleScanner->start(10, onBLEScanComplete, false);
  
}

void connectMqtt()
{
  uint8_t counter = 0;
  String clientId;
  
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("- Cannot connect to MQTT: WiFi disconnected");
    return;
  }

  if(mqttClient->connected()) {
    return;
  }

  Serial.printf("- Attempting to connect to MQTT: %s\n", mqttServer);

  for(counter = 0; (counter <= 60) && !mqttClient->connected(); counter++) {
    
  if (!mqttClient->connect(mqttClientId, mqttUser, mqttPassword)) {
      Serial.print(".");
      delay(2000);
    } else {
      Serial.printf("- Connected to %s\n", mqttServer);
      return;
    }
  }
}

void connectWiFi()
{
  uint8_t counter = 0;

  displayManager->wifiConnectScreen(0);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, wifiPassword);

  Serial.printf("Attempting to connect to '%s'", SSID);
  
  for(counter = 0; (counter <= 60) && (WiFi.status() != WL_CONNECTED); counter++) {
    Serial.print('.');
    delay(500);  

    displayManager->wifiConnectScreen((uint8_t)((float)counter / 60));
  }

  Serial.println("");
  
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi AP");  
  } else {
    Serial.printf("Successfully connected to %s, (ip: %s)\n", SSID, WiFi.localIP().toString().c_str());
    wifiConnectAttempts = 0;
  }
}

/**
 * Initialize the program and start scanning for our batteries!
 */
void setup() {
  
  Serial.begin(SERIAL_BAUD);

  for(int i = 0; (i < 50000) && !Serial; i++) {
    delay(1);
  }
  
  Serial.println("LiFeBlue ESP32 Battery Monitor");
  Serial.println("(c) 2019-2020 Internet Technology Solutions / This Smart House");
  Serial.println("http://www.thissmarthouse.com/lifeblue");
  Serial.printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n");

  Serial.println("- Initializing Battery Manager");
  
  batteryManager = BatteryManager::instance(MAX_BATTERIES, CELLS_PER_BATTERY);

  Serial.println("- Battery Manager Initialized");

  scanTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(scanTimer, &onScanTimer, true);
  timerAlarmWrite(scanTimer, 1000000, true);

  Serial.println("- Initializing BLE Client");
  
  BLEDevice::init(CLIENT_DEVICE_NAME);
  
  Serial.println("- Initialized BLE Client");
  
  Serial.println("- Initializing Display");
  
  displayManager = DisplayManager::instance();
  displayManager->setup();
  
  Serial.println("- Initialized Display");

  Serial.println("- Initializing WiFI and MQTT");
  
  wifiClient = new WiFiClient();
  mqttClient = new PubSubClient(*wifiClient);
  mqttClient->setServer(mqttServer, 1883);
  mqttClient->setBufferSize(512);
  
  Serial.println("- Initialized WiFI and MQTT");
    
  startDeviceScan();

  randomSeed(micros());
}

void publishToMqtt(batteryInfo_t *battery)
{
  char buffer[1024] = {NULL};
  uint8_t cellsPerBattery;
  DynamicJsonDocument doc(MQTT_OBJECT_SIZE);
  JsonArray cells;
  JsonObject status;
  size_t outputSize;
  char *topicBuffer;
    
  Serial.printf("\n\n ============ MQTT Publish Battery ========== \nIs battery buffer valid: %s\n", battery->is_valid ? "Yes" : "No");

  if (battery->is_valid) {
#ifdef DUMP_HEX_BATTERY_BUFFER
    hex_dump((char *)battery->buffer, 512, "MQTT -- batteryInfo_t");
#endif
    Serial.println("Valid Battery buffer == processing MQTT publish");

  // mqttTopic includes '%s' so we count that, and the address it 17 chars (+ null) 
  topicBuffer = (char *)os_zalloc(strlen(mqttTopic) - 2 + 17 + 1);
  
  sprintf(topicBuffer, mqttTopic, (char *)battery->id);
  
  Serial.println("MQTT Publish printing battery info:");
  Serial.printf("==== %s RSSI value: %d ====\n", (char *)battery->bname, battery->device->getRSSI());
  Serial.printf("- Publishing %s [%s] to %s\n", (char *)battery->bname, (char *)battery->id, topicBuffer);

  
  cellsPerBattery = batteryManager->getTotalCells();

  cells = doc.createNestedArray("cells");
  status = doc.createNestedObject("status");

  doc["battery_name"] = (char *)battery->bname;
  doc["RSSI"] = battery->device->getRSSI();
  doc["battery_id"] = (char *)battery->id;
  doc["voltage"] = battery->voltage;
  doc["current"] = battery->current;
  doc["soc"] = battery->soc;
  doc["temp"] = battery->temp;
  doc["cycles"] = battery->cycleCount;
  doc["ampHrs"] = battery->ampHrs;

  for(int i = 0; i < cellsPerBattery; i++) {
    cells.add(battery->cells[i]);
  }
  
  status["cell_high_voltage"] = battery->cell_high_voltage;
  status["cell_low_voltage"] = battery->cell_low_voltage;
  status["over_current_when_charge"] = battery->over_current_when_charge;
  status["over_current_when_discharge"] = battery->over_current_when_discharge;
  status["low_temp_when_charge"] = battery->low_temp_when_charge;
  status["low_temp_when_discharge"] = battery->low_temp_when_discharge;
  status["high_temp_when_charge"] = battery->high_temp_when_charge;
  status["high_temp_when_discharge"] = battery->high_temp_when_discharge;
  status["short_circuited"] = battery->short_circuited;

  outputSize = serializeJson(doc, buffer);  

  if(!mqttClient->connected()) {
    Serial.printf("- MQTT Client not connected");
  }
  
  if(!mqttClient->publish(topicBuffer, buffer)) {
    Serial.printf("- FAILED: Could not publish to MQTT\n");
  }
  
  free(topicBuffer);

  } else {
    Serial.println("Skipping MQTT Publish, battery buffer is not valid.");
  }
}

/**
 * This function is called over and over again every cycle and where the bulk of the
 * program actually does it's real work. In our case we call the BatteryManager's loop
 * to update the status of all of our batteries and then we will transmit those statuses
 * via MQTT
 */
void loop() {
  
  if(scanning) {
  
    if(scanTimerTick) {
      portENTER_CRITICAL(&scanTimerMux);
      scanTimerTick = false;
      portEXIT_CRITICAL(&scanTimerMux);
  
      scanTotalInterrupts++;
  
      displayManager->scanningScreen(scanTotalInterrupts * 10);
            
      if(scanTotalInterrupts == 10) {
        timerAlarmDisable(scanTimer);
      }
      
    }
        
    return;
  } 

  if(WiFi.status() != WL_CONNECTED) {
    
    if(wifiConnectAttempts < WIFI_CONNECT_ATTEMPTS) {
      wifiConnectAttempts++;
      connectWiFi();
      return;
    }       
    
  } else {
    if(!mqttClient->connected()) {
      connectMqtt();
    }
  }
  
  displayManager->statusScreen();

  batteryManager->loop(); // Give the batteries a chance to update

  if(mqttClient->connected()) {
    for(int i = 0; i < batteryManager->getTotalBatteries(); i++) {
      publishToMqtt(batteryManager->getBattery(i));
    }

    mqttClient->loop();
  }
  
  delay(5000);
}
