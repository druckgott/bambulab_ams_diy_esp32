#pragma once
#include <Arduino.h>
#include "main.h"

// ADC-Pins für ESP32 (ADC1)
#define ADC1_CH0_PIN 36
#define ADC1_CH1_PIN 37
#define ADC1_CH2_PIN 38
#define ADC1_CH3_PIN 39
#define ADC1_CH4_PIN 32
#define ADC1_CH5_PIN 33
#define ADC1_CH6_PIN 34
#define ADC1_CH7_PIN 35

extern void ADC_DMA_init();
extern float *ADC_DMA_get_value();
