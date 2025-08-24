#pragma once
#include <Arduino.h>
#include "main.h"
#include "pins.h"

extern void ADC_DMA_init();
extern float *ADC_DMA_get_value();
