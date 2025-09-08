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
    msg += "<li><a href=\"/storage\">AMS Speicher</a></li>";
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

  // Handler für "/storage"
  server.on("/storage", HTTP_GET, [](AsyncWebServerRequest *request) {
      String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
      page += "<title>ESP32 Storage Data</title>";
      page += "<style>";
      page += "body { font-family: Arial, sans-serif; margin: 20px; }";
      page += "h1 { color: #333; }";
      page += "table { border-collapse: collapse; width: 100%; margin-bottom: 20px; }";
      page += "th, td { border: 1px solid #ccc; padding: 8px; text-align: left; }";
      page += "th { background-color: #f2f2f2; }";
      page += "</style></head><body>";
      
      page += "<h1>📦 ESP32 Storage Data</h1>";
      page += "<p>Firmware built at: ";
      page += COMPILE_DATE;
      page += " ";
      page += COMPILE_TIME;
      page += "</p>";

      // ---- Bambubus Daten ----
      page += "<h2>🧵 Bambubus</h2>";
      page += "<table><tr><th>Field</th><th>Value</th></tr>";
      page += "<tr><td>Now Filament Num</td><td>" + String(data_save.BambuBus_now_filament_num) + "</td></tr>";
      page += "<tr><td>Filament Use Flag</td><td>" + String(data_save.filament_use_flag) + "</td></tr>";
      page += "<tr><td>Version</td><td>" + String(data_save.version) + "</td></tr>";
      page += "<tr><td>Check</td><td>0x" + String(data_save.check, HEX) + "</td></tr>";
      page += "</table>";

      // Filament Array
      page += "<h3>Filaments</h3><table>";
      page += "<tr><th>#</th><th>ID</th><th>Name</th><th>Temp Min</th><th>Temp Max</th><th>Color (RGBA)</th><th>Meters</th><th>Status</th><th>Motion</th></tr>";
      for (int i = 0; i < 4; i++) {
          page += "<tr>";
          page += "<td>" + String(i) + "</td>";
          page += "<td>" + String(data_save.filament[i].ID) + "</td>";
          page += "<td>" + String(data_save.filament[i].name) + "</td>";
          page += "<td>" + String(data_save.filament[i].temperature_min) + "</td>";
          page += "<td>" + String(data_save.filament[i].temperature_max) + "</td>";
          page += "<td>R:" + String(data_save.filament[i].color_R) + 
                  " G:" + String(data_save.filament[i].color_G) + 
                  " B:" + String(data_save.filament[i].color_B) + 
                  " A:" + String(data_save.filament[i].color_A) + "</td>";
          page += "<td>" + String(data_save.filament[i].meters, 2) + "</td>";

          // Status – Zahl + Name
          String statusStr;
          switch (data_save.filament[i].statu) {
              case AMS_filament_stu::offline: statusStr = "offline"; break;
              case AMS_filament_stu::online:  statusStr = "online"; break;
              case AMS_filament_stu::NFC_waiting: statusStr = "NFC_waiting"; break;
          }
          page += "<td>" + String((int)data_save.filament[i].statu) + " (" + statusStr + ")</td>";

          // Motion – Zahl + Name
          String motionStr;
          switch (data_save.filament[i].motion_set) {
              case AMS_filament_motion::before_pull_back: motionStr = "before_pull_back"; break;
              case AMS_filament_motion::need_pull_back:   motionStr = "need_pull_back"; break;
              case AMS_filament_motion::need_send_out:    motionStr = "need_send_out"; break;
              case AMS_filament_motion::on_use:           motionStr = "on_use"; break;
              case AMS_filament_motion::idle:             motionStr = "idle"; break;
          }
          page += "<td>" + String((int)data_save.filament[i].motion_set) + " (" + motionStr + ")</td>";

          page += "</tr>";
      }
      page += "</table>";

      // ---- Motion Control Daten ----
      page += "<h2>🎛️ Motion Control</h2>";
      page += "<table><tr><th>Field</th><th>Value</th></tr>";
      page += "<tr><td>Check</td><td>0x" + String(Motion_control_data_save.check, HEX) + "</td></tr>";
      page += "</table>";

      // Motion_control_dir Array
      page += "<h3>Motion Control Dir</h3>";
      page += "<table><tr><th>Index</th><th>Value</th></tr>";
      for (int i = 0; i < 4; i++) {
          page += "<tr><td>" + String(i) + "</td><td>" + String(Motion_control_data_save.Motion_control_dir[i]) + "</td></tr>";
      }
      page += "</table>";

      page += "<p><a href='/'>⬅️ Back to main page</a></p>";
      page += "</body></html>";

      request->send(200, "text/html", page);
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


