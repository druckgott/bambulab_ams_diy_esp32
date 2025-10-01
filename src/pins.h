#pragma once

#include "main.h"

//UART PORT = sagt welcher UART im Chip (Controller) benutzt wird.
//TXD_PIN/RXD_PIN = sagt welche physischen GPIOs nach außen verbunden werden.
//Ohne die Kombination geht’s nicht:
//Nur UART_PORT → der ESP32 weiß nicht, auf welchen Pins er senden/empfangen soll.
//Nur TXD_PIN/RXD_PIN → du hast keine Hardware-UART ausgewählt.
//Bambulab Verbindung
//#define BAMBU_UART_BAUD   1250000
#define BAMBU_UART_BAUD 1228800 //anderer Wert, weil der esp genau die 1250000 nicht halten kann
//Der UART im ESP32 hat gröbere Teiler, d.h. er kann nicht jede Baudrate exakt treffen.
//Bei „krummen“ Raten wie 1 250 000 liegt der Fehler auf dem ESP32 bei mehreren Prozent – das reicht, damit CRC kaputtgeht, weil einzelne Bits kippen.
//👉 Die nächstbessere Baudrate, die der ESP32 exakt kann, ist 1 228 800 Bd.
//Das ist näher an 1.25 M als das, was er bei 1250000 tatsächlich erzeugt.

#define UART_PORT   UART_NUM_1 //USB --> UART_NUM_0, TX2/RX2 --> UART_NUM_1
// wähle passende Pins am 32-Pin-ESP32
#define TXD_PIN     GPIO_NUM_17 //TX Pin 17
#define RXD_PIN     GPIO_NUM_16 //RX Pin 16
//RS485 PIN
#define DE_PIN      GPIO_NUM_5 //PIN 5

// Testweise 8 LEDs
#define LED_NUM 2

// PWM Pins für Motion Control
#define PWM0_PIN_LED 25
#define PWM1_PIN_LED 26
#define PWM2_PIN_LED 27
#define PWM3_PIN_LED 14

//Mainboard PIN
#define PWM6_PIN_LED 13

// Beispiel GPIO-Zuordnung für 4 AS5600 I2C-Geräte
// AS5600
// Der AS5600 ist ein magnetischer Drehwinkelsensor (Hall-Sensor).
// Er misst den rotationalen Winkel eines Magneten über 12 Bit (0–4095), also sehr präzise.
// Typischer Einsatz: Motoren, Drehgeber, Positionserfassung.
#define AS5600_0_SCL 22
#define AS5600_1_SCL 21
#define AS5600_2_SCL 19
#define AS5600_3_SCL 18

#define AS5600_0_SDA 23
#define AS5600_1_SDA 12
#define AS5600_2_SDA 27
#define AS5600_3_SDA 33

// PWM-Kanäle (ESP32 hat max. 16 Kanäle, 0-15)
//vermutlich 8 Motoren? DC Motoren
//#define MOTOR_PWM_CH0 0
#define MOTOR_PWM_CH0 0
#define MOTOR_PWM_CH1 1
#define MOTOR_PWM_CH2 2
#define MOTOR_PWM_CH3 3

// PWM-Frequenz und Auflösung
#define PWM_FREQ 20000           // 20 kHz, passend für Motorsteuerung
#define PWM_RES  10              // 10 Bit Auflösung (0-1023)

// Pins definieren fur PWM Channel
#define PWM_CH0_PIN 32
#define PWM_CH1_PIN 2
#define PWM_CH2_PIN 4
#define PWM_CH3_PIN 15  
/*#define PWM_CH4_PIN 12   // statt 18 (GPIO2 vorsichtig, Boot-Pin)
#define PWM_CH5_PIN 4   
#define PWM_CH6_PIN 15  
#define PWM_CH7_PIN 35*/

// ADC-Pins für ESP32 (ADC1), gefilterten Spannungen
//Initialisiert 8 ADC-Kanäle des ESP32 mit 12-Bit Auflösung und ~3.3 V Messbereich.
//Liest auf Abruf (ADC_DMA_get_value()) alle 8 Kanäle je 256-mal aus.
//Bildet den Mittelwert (Noise-Filter).
//Gibt die gefilterten Werte in Volt zurück.

//aktuell deaktiviert in void MC_PULL_ONLINE_read (nur simuliert, immer optimaler Wert)
#define ADC1_CH0_PIN 36
#define ADC1_CH1_PIN 37
#define ADC1_CH2_PIN 38
#define ADC1_CH3_PIN 39
#define ADC1_CH4_PIN 40
#define ADC1_CH5_PIN 33
#define ADC1_CH6_PIN 34
#define ADC1_CH7_PIN 35

#define Debug_log_on
#define Debug_log_baudrate 115200

//OTA Task
#define OTA_TASK_DELAY_MS 100  // Verzögerung für OTA-Task in Millisekunden

// Globale Variablen (nur Deklaration!)
extern uint8_t BambuBus_AMS_num;
extern uint8_t AMS_humidity_wet;

#define Bambubus_flash_addr        0x000   // Start der storage-Partition
#define Motion_control_flash_addr  0x200   // direkt danach, genug Platz

