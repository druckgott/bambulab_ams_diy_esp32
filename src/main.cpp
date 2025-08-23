#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebSerial.h>
#include "wifimanager.h" //https://registry.platformio.org/libraries/martinverges/ESP32%20Wifi%20Manager/installation

#define AMS_NUMBER      2
#define AMS_VERSION     0x31
#define AMS_HW_NAME     "AMS_F102"
#define AMS_SERIAL      "AMS-F3-0823-0427"

#define RS485_DE_RE_PIN 5
#define AMS_RX_PIN 16
#define AMS_TX_PIN 17

// Compiler-Zeitstempel
const char COMPILE_DATE[] = __DATE__;
const char COMPILE_TIME[] = __TIME__;

WIFIMANAGER WifiManager;
// Async Webserver auf Port 80
AsyncWebServer server(80);

HardwareSerial amsSerial(2); // UART2

// Hilfsfunktionen für RS485
void rs485SendEnable()   { digitalWrite(RS485_DE_RE_PIN, HIGH); Serial.println("[MAX485] Sende-Modus aktiviert"); }
void rs485ReceiveEnable(){ digitalWrite(RS485_DE_RE_PIN, LOW);  Serial.println("[MAX485] Empfangs-Modus aktiviert"); }

void recvMsg(uint8_t *data, size_t len){
  WebSerial.println("Received Data...");
  String d = "";
  for(size_t i = 0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
}

void printPacket(const char* prefix, const uint8_t* buf, int len) {
  // Serial-Ausgabe
  Serial.print(prefix);
  WebSerial.print(prefix);

  for (int i = 0; i < len; i++) {
    if (buf[i] < 16) {
      Serial.print("0");
      WebSerial.print("0");
    }
    Serial.print(buf[i], HEX);
    WebSerial.print(buf[i], HEX);
    Serial.print(" ");
    WebSerial.print(" ");
  }
  Serial.println();
  WebSerial.println();

  // ASCII-Ausgabe
  Serial.print("[ASCII] ");
  WebSerial.print("[ASCII] ");
  for (int i = 0; i < len; i++) {
    char c = (buf[i] >= 32 && buf[i] <= 126) ? buf[i] : '.';
    Serial.print(c);
    WebSerial.print(c);
  }
  Serial.println();
  WebSerial.println();
}

void sendHeartbeat() {
  uint8_t packet[8] = {0x2F, 0x2F, 0xFE, (uint8_t)AMS_NUMBER, 0x00, 0x00, 0x00, 0x00};
  Serial.println("[AMS] Sende Heartbeat");
  printPacket("  Sende: ", packet, sizeof(packet));
  rs485SendEnable();
  delayMicroseconds(10);
  amsSerial.write(packet, sizeof(packet));
  amsSerial.flush();
  delayMicroseconds(10);
  rs485ReceiveEnable();
}

void sendVersion() {
  uint8_t packet[24] = {0x2F, 0x2F, 0xFE, (uint8_t)AMS_NUMBER, 0x10, 0x00,
                        AMS_VERSION, 0x00, // Version-Bytes
  };
  for(uint8_t i=0; i<16; i++) {
    packet[8+i] = (i < strlen(AMS_HW_NAME)) ? AMS_HW_NAME[i] : 0x00;
  }
  Serial.println("[AMS] Sende Version");
  printPacket("  Sende: ", packet, sizeof(packet));
  rs485SendEnable();
  delayMicroseconds(10);
  amsSerial.write(packet, sizeof(packet));
  amsSerial.flush();
  delayMicroseconds(10);
  rs485ReceiveEnable();
}

void sendOnlineDetect() {
  uint8_t packet[8] = {0x2F, 0x2F, 0xFE, (uint8_t)AMS_NUMBER, 0x20, 0x01, 0x01, 0x00};
  Serial.println("[AMS] Sende Online-Detect");
  printPacket("  Sende: ", packet, sizeof(packet));
  rs485SendEnable();
  delayMicroseconds(10);
  amsSerial.write(packet, sizeof(packet));
  amsSerial.flush();
  delayMicroseconds(10);
  rs485ReceiveEnable();
}

void setup() {

  Serial.begin(115200);
  Serial.println("Booting");

  Serial.println("Starting WiFi Manager...");

  WifiManager.startBackgroundTask();        // Run the background task to take care of our Wifi
  WifiManager.fallbackToSoftAp(true);       // Run a SoftAP if no known AP can be reached
  WifiManager.attachWebServer(&server);  // Attach our API to the HTTP Webserver 
  WifiManager.attachUI();                   // Attach the UI to the Webserver

  // Root-Seite "/" anzeigen (AsyncServer-Version)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String msg = "<!DOCTYPE html><html><head><title>ESP32</title></head><body>";
    msg += "<h1>Hi! Ich bin dein ESP32.</h1>";
    msg += "<p>Firmware kompiliert am: ";
    msg += COMPILE_DATE;
    msg += " um ";
    msg += COMPILE_TIME;
    msg += "</p>";
    msg += "<p><a href=\"/webserial\">Zur WebSerial-Konsole</a></p>";

    msg += "<ul>";
    msg += "<li><a href=\"/wifi\">WiFi Configuration Panel</a></li>";
    msg += "<li><a href=\"/api/wifi/status\">WiFi Status (JSON API)</a></li>";
    msg += "<li><a href=\"/api/wifi/configlist\">Saved Networks (JSON API)</a></li>";
    msg += "</ul>";

    msg += "</body></html>";
    request->send(200, "text/html", msg);
  });  
  
   ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  Serial.println("OTA bereit");

  // WebSerial starten
  WebSerial.begin(&server);
  WebSerial.msgCallback(recvMsg);

  // Async Webserver starten
  server.begin();
  Serial.println("Webserver bereit");

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  pinMode(RS485_DE_RE_PIN, OUTPUT);
  rs485ReceiveEnable();

  amsSerial.begin(1250000, SERIAL_8E1, AMS_RX_PIN, AMS_TX_PIN);

  Serial.println("AMS-Fake ESP32 gestartet!");
  Serial.print("AMS Nummer: "); Serial.println(AMS_NUMBER);
  Serial.print("AMS Version: "); Serial.println(AMS_VERSION, HEX);
  Serial.print("AMS Name: "); Serial.println(AMS_HW_NAME);
  Serial.print("AMS Serial: "); Serial.println(AMS_SERIAL);

}

void loop() {
  // OTA prüfen
  ArduinoOTA.handle();

  // Prüfe, ob Daten vom AMS verfügbar sind
  if (amsSerial.available()) {
      uint8_t rxbuf[128]; // Puffer auf 128 Bytes vergrößert
      int len = amsSerial.readBytes(rxbuf, sizeof(rxbuf)); // bis zu 128 Bytes lesen
      // Hex-Ausgabe
      String hexLine = "";
      for (int i = 0; i < len; i++) {
          if (rxbuf[i] < 16) hexLine += "0";
          hexLine += String(rxbuf[i], HEX) + " ";
      }
      WebSerial.println(hexLine);
  }
}
