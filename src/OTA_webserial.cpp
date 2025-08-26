#include "OTA_webserial.h"

// Compiler-Zeitstempel
const char COMPILE_DATE[] = __DATE__;
const char COMPILE_TIME[] = __TIME__;

WIFIMANAGER WifiManager;
// Async Webserver auf Port 80
AsyncWebServer server(80);
WebSerial webSerial;

void otaTask(void *pvParameters) {
  for (;;) {
    ArduinoOTA.handle();
    vTaskDelay(OTA_TASK_DELAY_MS / portTICK_PERIOD_MS); // alle 100 ms prüfen
  }
}

void init_ota_webserial() {

  WifiManager.startBackgroundTask();        // Run the background task to take care of our Wifi
  WifiManager.fallbackToSoftAp(true);       // Run a SoftAP if no known AP can be reached
  WifiManager.attachWebServer(&server);  // Attach our API to the HTTP Webserver 
  WifiManager.attachUI();                   // Attach the UI to the Webserver

  // Root-Seite "/" anzeigen (AsyncServer-Version)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String msg = "<!DOCTYPE html><html><head><title>ESP32 Bambulab Fake AMS</title></head><body>";
    msg += "<h1>Hi! Ich bin dein ESP32.</h1>";
    msg += "<p>Firmware kompiliert am: ";
    msg += COMPILE_DATE;
    msg += " um ";
    msg += COMPILE_TIME;
    msg += "</p>";
    msg += "<ul>";
    msg += "<li><a href=\"/webserial\" target=\"_blank\">Zur WebSerial-Konsole</a></li>";
    msg += "<br>";
    msg += "<li><a href=\"/wifi\">WiFi Configuration Panel</a></li>";
    msg += "<li><a href=\"/api/wifi/status\">WiFi Status (JSON API)</a></li>";
    msg += "<li><a href=\"/api/wifi/configlist\">Saved Networks (JSON API)</a></li>";
    msg += "<br>";
    msg += "<li><a href=\"/reboot\">ESP neu starten</a></li>";
    msg += "</ul>";

    msg += "</body></html>";
    request->send(200, "text/html", msg);
  });  
  
  // Reboot-Handler
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", "ESP wird neu gestartet...");
      // Restart mit Verzögerung
      esp_register_shutdown_handler([]() {
          ESP.restart();
      });
  });

   ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      //Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      //Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      //Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  // OTA in eigenen Task packen
  xTaskCreate(
    otaTask,           // Funktion
    "OTA Task",        // Name
    4096,              // Stackgröße
    NULL,              // Parameter
    1,                 // Priorität
    NULL               // Task Handle
  );

  // WebSerial starten
  //WebSerial.begin(&server);
  webSerial.onMessage([](const std::string& msg) { Serial.println(msg.c_str()); });
  webSerial.begin(&server);
  webSerial.setBuffer(100);

  // Async Webserver starten
  server.begin();

}


