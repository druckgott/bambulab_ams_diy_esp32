#pragma once

// PWM Pins für Motion Control
#define PWM0_PIN 25
#define PWM1_PIN 26
#define PWM2_PIN 27
#define PWM3_PIN 14

#define PWM6_PIN 99

// Beispiel GPIO-Zuordnung für 4 AS5600 I2C-Geräte
#define AS5600_0_SCL 22
#define AS5600_1_SCL 21
#define AS5600_2_SCL 19
#define AS5600_3_SCL 18

#define AS5600_0_SDA 23
#define AS5600_1_SDA 5
#define AS5600_2_SDA 17
#define AS5600_3_SDA 16

// PWM-Kanäle (ESP32 hat max. 16 Kanäle, 0-15)
#define PWM_CH0 0
#define PWM_CH1 1
#define PWM_CH2 2
#define PWM_CH3 3

// PWM-Frequenz und Auflösung
#define PWM_FREQ 20000           // 20 kHz, passend für Motorsteuerung
#define PWM_RES  10              // 10 Bit Auflösung (0-1023)

// Pins definieren
#define PWM_CH0_PIN 15
#define PWM_CH1_PIN 2
#define PWM_CH2_PIN 4
#define PWM_CH3_PIN 5
#define PWM_CH4_PIN 18
#define PWM_CH5_PIN 19
#define PWM_CH6_PIN 21
#define PWM_CH7_PIN 22