#include "ADC_DMA.h"

#define ADC_FILTER_N_POW 8
#define ADC_FILTER_N (1 << ADC_FILTER_N_POW)  // 256

static float ADC_V[8]; // Gefilterte Spannungen

// ADC-Pins array
static const uint8_t adc_pins[8] = {
    ADC1_CH0_PIN,
    ADC1_CH1_PIN,
    ADC1_CH2_PIN,
    ADC1_CH3_PIN,
    ADC1_CH4_PIN,
    ADC1_CH5_PIN,
    ADC1_CH6_PIN,
    ADC1_CH7_PIN
};

void ADC_DMA_init()
{
    analogReadResolution(12); // 12-Bit Auflösung
    for (int i = 0; i < 8; i++) {
        analogSetPinAttenuation(adc_pins[i], ADC_11db); // voller Bereich ~3.3V
        analogRead(adc_pins[i]); // initial stabilisieren
    }
}

float *ADC_DMA_get_value()
{
    for (int i = 0; i < 8; i++) {
        uint32_t sum = 0;
        for (int j = 0; j < ADC_FILTER_N; j++) {
            sum += analogRead(adc_pins[i]);
        }
        uint32_t avg = sum >> ADC_FILTER_N_POW;
        ADC_V[i] = ((float)avg / 4095.0f) * 3.3f;
    }
    return ADC_V;
}