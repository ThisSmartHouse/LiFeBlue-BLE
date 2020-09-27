#ifndef __LIFEBLUE_WIFI_CFG
#define __LIFEBLUE_WIFI_CFG

/**
 * Set your network configuration here
 */

static const char *SSID = "YOURSSID";
static const char *wifiPassword = "YourPassword";
static const char *mqttClientId = "lifeblue-mon";
static const char *mqttServer = "broker.hivemq.com";
static const char *mqttTopic = "/rv/sensors/batteries/%s";
static const char *mqttUser = "YourMQTTBrokerUserId";
static const char *mqttPassword = "YourMQTTBrokerPassword";

#endif
