Aktuell funktioniert es leider noch nicht, weil ein CRC Fehler beim leses des Buses zum ESP auftritt

Es gibt einen Branch mit OTA und Webserial zur einfacheren analyse.

## 🖼️ Links/Quellen mit dem der Übertrag auf einem ESP32 stattgefunden hat bzw. findet:

Dieser Code wurde als Basis verwendet und auf einem ESP32 übertragen:
https://github.com/krrr/BMCU370

Sonstige Infos:

https://wiki.bambulab.com/en/x1/troubleshooting/AMS_is_not_detected_by_the_printer

https://github.com/Bambu-Research-Group/Bambu-Bus


https://github.com/09lab/BambuBus-Sniffer

### 1. UART Initialisierung
- Baudrate: **1.250.000 Baud**  
- Datenbits: **8**  
- Parität: **Even**  
- Stopbits: **1**  
- RS485 Steuerung über `DE_PIN`:  
  - **0 → Empfang**  
  - **1 → Senden**  

Beim Start wird `DE_PIN` korrekt auf **0** gesetzt und ein Testbyte (`0xAA`) über UART gesendet.

---

### 2. Empfang (uart_event_task)
Wir haben zwei Varianten getestet:
- **Byteweise Weitergabe** (`uint8_t data[2]`) → funktioniert grundsätzlich, aber oft fragmentiert.  
- **Größerer Buffer + Debug-Ausgabe der Rohdaten** (`uint8_t data[128]`) → ermöglicht zu sehen, was wirklich am UART ankommt.  

---

### 3. Beobachtungen
- Teilweise werden korrekte **Startbytes (0x3D)** erkannt.  
- Pakete werden im `RX_IRQ` korrekt angefangen einzulesen.  
- Allerdings kommt es **immer wieder zu CRC-Fehlern**, d.h. das empfangene CRC stimmt nicht mit dem berechneten überein.  

---

### 4. Aktuelle Probleme
- In den meisten Fällen tritt **UART Event Type 5 (Framing Error)** auf → das bedeutet, dass auf der Leitung zwar Aktivität ist, der ESP32 die Bits aber nicht korrekt in Bytes dekodieren kann.  
- Ab und zu wird **Event Type 1 (Daten verfügbar)** gemeldet → aber auch hier oft fehlerhafte Bytes.  
- Debug-Ausgaben zeigen, dass der **DE_PIN manchmal fälschlicherweise auf 1** bleibt, wodurch der ESP nicht mehr empfängt.  
- Ohne sauberen UART-Empfang ist eine funktionierende **CRC-Prüfung unmöglich**.

---

## Fazit
- **RX_IRQ selbst funktioniert**: die Logik zum Einlesen und Paketaufbau wurde aus dem Python-Port übernommen und liefert Ausgaben.  
- **Das Hauptproblem liegt tiefer**:  
  - Falsche oder instabile UART-Parameter  
  - Probleme mit der MAX485-Beschaltung (DE/RE-Steuerung)  
  - Eventuell Signalqualität oder Timing auf dem BambuBus  

Solange der ESP32 nur **Framing Errors (UART_EVENT_TYPE 5)** empfängt,  
kann kein gültiges BambuBus-Paket dekodiert werden → **CRC-Fehler sind die Folge**.




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
| Pin 1             | A      | A          | RS485 differenzielles Signal  |
| Pin 2             | B      | B          | RS485 differenzielles Signal  |
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


