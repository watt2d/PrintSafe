#pragma once
#include "mqtt_client.h"
#define WIFI_SSID "IoT"
#define WIFI_PASS ""
//spare Watteussid alanmat76
#define MQTT_URI "mqtt://broker.hivemq.com:1883"
void mqtt_start(void);
void mqtt_SendTAndHR(void *arg);
