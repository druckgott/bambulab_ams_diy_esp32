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
    msg += "<li><a href=\"/api/storage\">AMS Speicher (json)</a></li>";
    msg += "<li><a href=\"/debug\">AMS Debug Switch</a></li>";
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

  server.on("/api/storage", HTTP_GET, [](AsyncWebServerRequest *request){
      String json = "{";
      json += "\"BambuBus_now_filament_num\":" + String(data_save.BambuBus_now_filament_num) + ",";
      json += "\"filament_use_flag\":" + String(data_save.filament_use_flag) + ",";
      json += "\"version\":" + String(data_save.version) + ",";
      json += "\"check\":" + String(data_save.check) + ",";  // als Dezimalwert
      
      json += "\"filaments\":[";
      for (int i = 0; i < 4; i++) {
          json += "{";
          json += "\"ID\":\"" + String(data_save.filament[i].ID) + "\",";
          json += "\"name\":\"" + String(data_save.filament[i].name) + "\",";
          json += "\"temperature_min\":" + String(data_save.filament[i].temperature_min) + ",";
          json += "\"temperature_max\":" + String(data_save.filament[i].temperature_max) + ",";
          json += "\"color_R\":" + String(data_save.filament[i].color_R) + ",";
          json += "\"color_G\":" + String(data_save.filament[i].color_G) + ",";
          json += "\"color_B\":" + String(data_save.filament[i].color_B) + ",";
          json += "\"color_A\":" + String(data_save.filament[i].color_A) + ",";
          json += "\"meters\":" + String(data_save.filament[i].meters, 2) + ",";
          json += "\"statu\":" + String((int)data_save.filament[i].statu) + ",";
          json += "\"motion_set\":" + String((int)data_save.filament[i].motion_set);
          json += "}";
          if (i < 3) json += ",";
      }
      json += "],";  // ← Komma nach Array unbedingt
      
      json += "\"Motion_check\":" + String(Motion_control_data_save.check) + ",";  // Dezimalwert
      json += "\"Motion_dir\":[";
      for (int i = 0; i < 4; i++) {
          json += String(Motion_control_data_save.Motion_control_dir[i]);
          if (i < 3) json += ",";
      }
      json += "],";
      /*json += "]";
      json += "}";*/

      // --- neu: heartbeat-Daten ---
      json += "\"heartbeat\":" + String(ram_core.heartbeat) + ",";
      json += "\"last_heartbeat_time\":" + String(ram_core.last_heartbeat_time) + ",";
      json += "\"last_heartbeat_len\":" + String(ram_core.last_heartbeat_len) + ",";
      
      // Paketinhalt als Array
      json += "\"last_heartbeat_buf\":[";
      for (int i = 0; i < ram_core.last_heartbeat_len; i++) {
          json += String(ram_core.last_heartbeat_buf[i]);
          if (i < ram_core.last_heartbeat_len - 1) json += ",";
      }
      json += "]";
      
      json += "}";

      request->send(200, "application/json", json);
  });

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

    // Bambubus Tabelle
    page += "<h2>🧵 Bambubus</h2>";
    page += "<table id='bambuBusTable'><tr><th>Field</th><th>Value</th></tr>";
    page += "<tr><td>Now Filament Num</td><td id='bb_now_filament_num'>" + String(data_save.BambuBus_now_filament_num) + "</td></tr>";
    page += "<tr><td>Filament Use Flag</td><td id='bb_filament_use_flag'>" + String(data_save.filament_use_flag) + "</td></tr>";
    page += "<tr><td>Version</td><td id='bb_version'>" + String(data_save.version) + "</td></tr>";
    page += "<tr><td>Check</td><td id='bb_check'>0x" + String(data_save.check, HEX) + "</td></tr>";
    page += "</table>";

    // Filament Tabelle
    page += "<h3>Filaments</h3>";
    page += "<table id='filamentTable'><tr><th>#</th><th>ID</th><th>Name</th><th>Temp Min</th><th>Temp Max</th><th>Color (RGBA)</th><th>Meters</th><th>Status</th><th>Motion</th></tr>";
    for (int i = 0; i < 4; i++) {
        page += "<tr>";
        page += "<td>" + String(i) + "</td>";
        page += "<td id='f_" + String(i) + "_ID'>" + String(data_save.filament[i].ID) + "</td>";
        page += "<td id='f_" + String(i) + "_name'>" + String(data_save.filament[i].name) + "</td>";
        page += "<td id='f_" + String(i) + "_temp_min'>" + String(data_save.filament[i].temperature_min) + "</td>";
        page += "<td id='f_" + String(i) + "_temp_max'>" + String(data_save.filament[i].temperature_max) + "</td>";
        page += "<td id='f_" + String(i) + "_color'>R:" + String(data_save.filament[i].color_R) + " G:" + String(data_save.filament[i].color_G) + " B:" + String(data_save.filament[i].color_B) + " A:" + String(data_save.filament[i].color_A) + "</td>";
        page += "<td id='f_" + String(i) + "_meters'>" + String(data_save.filament[i].meters,2) + "</td>";
        page += "<td id='f_" + String(i) + "_status'>" + String((int)data_save.filament[i].statu) + "</td>";
        page += "<td id='f_" + String(i) + "_motion'>" + String((int)data_save.filament[i].motion_set) + "</td>";
        page += "</tr>";
    }
    page += "</table>";

    // Motion Control
    page += "<h2>🎛️ Motion Control</h2>";
    page += "<table id='motionTable'><tr><th>Field</th><th>Value</th></tr>";
    page += "<tr><td>Check</td><td id='m_check'>0x" + String(Motion_control_data_save.check, HEX) + "</td></tr>";
    page += "</table>";

    // Motion Control Dir
    page += "<h3>Motion Control Dir</h3>";
    page += "<table id='motionDirTable'><tr><th>Index</th><th>Value</th></tr>";
    for(int i=0;i<4;i++){
        page += "<tr><td>" + String(i) + "</td><td id='md_" + String(i) + "'>" + String(Motion_control_data_save.Motion_control_dir[i]) + "</td></tr>";
    }
    page += "</table>";

    page += "<p><a href='/'>⬅️ Back to main page</a></p>";

    // JS
    page += "<script>";
    page += "async function updateStorage() {";
    page += "  const res = await fetch('/api/storage');";
    page += "  const data = await res.json();";

    page += "  document.getElementById('bb_now_filament_num').innerText = data.BambuBus_now_filament_num;";
    page += "  document.getElementById('bb_filament_use_flag').innerText = data.filament_use_flag;";
    page += "  document.getElementById('bb_version').innerText = data.version;";
    page += "  document.getElementById('bb_check').innerText = data.check;";

    page += "  for(let i=0;i<4;i++){";
    page += "    const f = data.filaments[i];";
    page += "    document.getElementById('f_'+i+'_ID').innerText = f.ID;";
    page += "    document.getElementById('f_'+i+'_name').innerText = f.name;";
    page += "    document.getElementById('f_'+i+'_temp_min').innerText = f.temperature_min;";
    page += "    document.getElementById('f_'+i+'_temp_max').innerText = f.temperature_max;";
    page += "    document.getElementById('f_'+i+'_color').innerHTML = '<span style=\"display:inline-block;width:20px;height:20px;margin-right:5px;background-color:rgba(' + f.color_R + ',' + f.color_G + ',' + f.color_B + ',' + (f.color_A/255) + ');\"></span>' + 'R:'+f.color_R+' G:'+f.color_G+' B:'+f.color_B+' A:'+f.color_A;";
    page += "    let statusColor='', statusStr='';";
    page += "    switch(f.statu){ case 0: statusColor='red'; statusStr='offline'; break; case 1: statusColor='green'; statusStr='online'; break; case 2: statusColor='orange'; statusStr='NFC_waiting'; break; }";
    page += "    document.getElementById('f_'+i+'_status').innerHTML = '<span style=\"display:inline-block;width:20px;height:20px;margin-right:5px;background-color:' + statusColor + ';\"></span> ' + f.statu + ' (' + statusStr + ')';";
    page += "    let motionStr='';";
    page += "    switch(f.motion_set){ case 0: motionStr='before_pull_back'; break; case 1: motionStr='need_pull_back'; break; case 2: motionStr='need_send_out'; break; case 3: motionStr='on_use'; break; case 4: motionStr='idle'; break; }";
    page += "    document.getElementById('f_'+i+'_motion').innerText = f.motion_set + ' (' + motionStr + ')';";
    page += "  }";

    page += "  document.getElementById('m_check').innerText = data.Motion_check;";
    page += "  for(let i=0;i<4;i++){ document.getElementById('md_'+i).innerText = data.Motion_dir[i]; }";

    page += "}";
    page += "setInterval(updateStorage, 500);";
    page += "updateStorage();";
    page += "</script>";

    page += "</body></html>";
    request->send(200, "text/html", page);
  });

  // === Debug-Webseite ===
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request){
      String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
      page += "<title>ESP32 Debug Control</title>";
      page += "<style>";
      page += "body { font-family: Arial; margin:20px; }";
      page += "h1 { color:#333; }";
      page += "label { display:block; margin-top:10px; }";
      page += "select, input[type=number], input[type=checkbox] { margin-top:5px; padding:5px; }";
      page += "button { margin-top:15px; padding:10px 15px; background:#4CAF50; color:white; border:none; cursor:pointer; }";
      page += "button:hover { background:#45a049; }";
      page += "</style></head><body>";

      page += "<h1>🎛️ Debug Control</h1>";
      page += "<form action='/api/setdebug' method='get'>";

      // Checkbox für Bambubus Debug Mode
      page += "<label><input type='checkbox' name='bambuDebug' value='1'";
      if (bambuBusDebugMode) page += " checked";
      page += "> Bambubus Debug Mode (immer online)</label>";

      // Checkbox Debug
      page += "<label><input type='checkbox' name='debug' value='1'";
      if (debugMotionEnabled) page += " checked";
      page += "> Debug aktivieren</label>";

      // Filament Nummer
      page += "<label>Filament Nummer (0-3):</label>";
      page += "<input type='number' name='num' min='0' max='3' value='" + String(currentdebugNum) + "'>";

      // Motion Auswahl
      page += "<label>Motion Modus:</label>";
      page += "<select name='mode'>";
      String modes[] = {"before_pull_back","need_pull_back","need_send_out","on_use","idle"};
      int enumVals[] = {0,1,2,3,4};
      for (int i=0;i<5;i++) {
          page += "<option value='" + modes[i] + "'";
          if ((int)currentdebugMotion == enumVals[i]) page += " selected";
          page += ">" + modes[i] + "</option>";
      }
      page += "</select>";

      page += "<br><button type='submit'>✅ Anwenden</button>";
      page += "</form>";

      page += "<p><a href='/'>⬅️ Zurück zur Hauptseite</a></p>";
      page += "</body></html>";

      request->send(200, "text/html", page);
  });

  // === API zum Setzen der Debug-Werte ===
  server.on("/api/setdebug", HTTP_GET, [](AsyncWebServerRequest *request){
      
      if (request->hasArg("bambuDebug")) {
          bambuBusDebugMode = (request->arg("bambuDebug") == "1");
      } else {
          bambuBusDebugMode = false;
      }
      if (request->hasArg("debug")) {
          debugMotionEnabled = (request->arg("debug") == "1");
      } else {
          debugMotionEnabled = false;
      }
      if (request->hasArg("num")) {
          int n = request->arg("num").toInt();
          if (n >= 0 && n < 4) currentdebugNum = n;
      }
      if (request->hasArg("mode")) {
          String m = request->arg("mode");
          if (m == "before_pull_back") currentdebugMotion = AMS_filament_motion::before_pull_back;
          else if (m == "need_pull_back") currentdebugMotion = AMS_filament_motion::need_pull_back;
          else if (m == "need_send_out") currentdebugMotion = AMS_filament_motion::need_send_out;
          else if (m == "on_use") currentdebugMotion = AMS_filament_motion::on_use;
          else if (m == "idle") currentdebugMotion = AMS_filament_motion::idle;
      }
      String res = "Debug Motion gesetzt: debug=" + String(debugMotionEnabled) +
                  ", num=" + String(currentdebugNum) +
                  ", mode=" + String((int)currentdebugMotion);
      request->send(200, "text/plain", res);
  });

   ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    })
    .onEnd([]() {
      // nach OTA wieder freigeben
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
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
  webSerial.onMessage([](const std::string& msg) { Serial.println(msg.c_str()); });
  webSerial.begin(&server);
  webSerial.setBuffer(100);

  // Async Webserver starten
  server.begin();

}


