#pragma once

#include "main.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebSerial.h>
#include "wifimanager.h" //https://registry.platformio.org/libraries/martinverges/ESP32%20Wifi%20Manager/installation

extern void init_ota_webserial();