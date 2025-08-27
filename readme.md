## 🖼️ Links/Quellen mit dem der Übertrag auf einem ESP32 stattgefunden hat bzw. findet:

Dieser Code wurde als Basis verwendet und auf einem ESP32 übertragen:
https://github.com/krrr/BMCU370

Es gibt einen Branch mit OTA und Webserial zur einfacheren analyse:
https://github.com/druckgott/bambulab_ams_diy_esp32_test/tree/ota_webserial

Sonstige Infos:

https://wiki.bambulab.com/en/x1/troubleshooting/AMS_is_not_detected_by_the_printer

https://github.com/Bambu-Research-Group/Bambu-Bus

https://github.com/09lab/BambuBus-Sniffer

# 📡 ESP32 ↔ MAX485 ↔ Bambulab AMS

Dieses Projekt zeigt, wie man ein **ESP32 DevKit (Wemos, 32 Pins)** zusammen mit einem **MAX485-Modul** mit dem **Bambulab AMS (Auxiliary Material System)** verbindet.  
Damit kann die serielle RS485-Kommunikation zwischen dem AMS und dem ESP32 hergestellt werden.

---

## 🔌 Pinbelegung ESP32 ↔ MAX485

| ESP32 Pin | MAX485 Pin | Beschreibung            |
|-----------|------------|-------------------------|
| GPIO 16   | RO         | RS485 → ESP32 (RX)      |
| GPIO 17   | DI         | ESP32 → RS485 (TX)      |
| GPIO 5    | DE & RE    | Richtung steuern        |
| GND       | GND        | Masse                   |
| VIN (5 V) | VCC        | Versorgung des MAX485   |

---

## 🔌 MAX485 ↔ AMS 6-Pin-Kabel

Laut [Bambulab Wiki – AMS Connector Pinout](https://wiki.bambulab.com/en/x1/troubleshooting/AMS_is_not_detected_by_the_printer):

| AMS Kabel (6-Pin) | Signal | MAX485 Pin | Hinweis                       |
|-------------------|--------|------------|-------------------------------|
| Pin 1             | B      | B          | RS485 differenzielles Signal  |
| Pin 2             | A      | A          | RS485 differenzielles Signal  |
| Pin 3             | GND    | GND        | Masse                         |
| Pin 4             | 24 V   | ❌ **Nicht anschließen** |
| Pin 5             | NC     | ❌ Nicht belegt            |
| Pin 6             | NC     | ❌ Nicht belegt            |

---

## ⚠️ Hinweise

- **Stromversorgung:** Das MAX485-Modul wird **nicht** über die 24 V vom AMS versorgt. Stattdessen wird es vom **VIN-Pin des ESP32 (5 V)** gespeist.  
- **Richtungspins:** DE und RE am MAX485 **gemeinsam** auf GPIO 5 des ESP32 legen.  
- **Level:** Der MAX485 arbeitet mit 5 V, die GPIOs des ESP32 sind 3.3 V tolerant → das passt, da der MAX485 TTL-Pegel versteht.  

---

## 🖼️ Anschluss-Schema

```text
 Bambulab AMS (6-Pin)              MAX485                ESP32 (Wemos 32-Pin)
 ──────────────────────           ────────              ─────────────────────
 Pin 1 (A)  ─────────────────────► A
 Pin 2 (B)  ─────────────────────► B
 Pin 3 (GND) ────────────────────► GND ────────────────► GND
 Pin 4 (24V) ── ✗ NICHT VERWENDEN
 Pin 5 (NC)  ── ✗
 Pin 6 (NC)  ── ✗

                                   RO ────────────────► GPIO16 (RX)
                                   DI ────────────────► GPIO17 (TX)
                               DE+RE ────────────────► GPIO5
                                   VCC ───────────────► VIN (5V)


