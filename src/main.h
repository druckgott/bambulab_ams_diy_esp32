#pragma once
#include <Arduino.h>
#include "stdlib.h"
#include "FlashStorage.h"
#include "many_soft_AS5600.h"
#include "Motion_control.h"
#include "BambuBus.h"
#include "time64.h"
#include "ADC_DMA.h"
#include "OTA_webserial.h"
#include "pins.h"
#include "Debug_log.h"

#define delay_any_us(time)                                                 \
    {                                                                      \
        const uint64_t _delay_any_div_time = (uint64_t)(8000000.0 / time); \
        SysTick->SR &= ~(1 << 0);                                          \
        SysTick->CMP = SystemCoreClock / _delay_any_div_time;              \
        SysTick->CTLR |= (1 << 5) | (1 << 4) | (1 << 0);                   \
                                                                           \
        while (!(SysTick->SR & 1))                                         \
            ;                                                              \
        SysTick->CTLR &= ~(1 << 0);                                        \
    }

#define delay_any_ms(time)                                               \
    {                                                                    \
        const uint64_t _delay_any_div_time = (uint64_t)(80000.0 / time); \
        SysTick->SR &= ~(1 << 0);                                        \
        SysTick->CMP = SystemCoreClock / _delay_any_div_time;            \
        SysTick->CTLR |= (1 << 5) | (1 << 4) | (1 << 0);                 \
                                                                         \
        while (!(SysTick->SR & 1))                                       \
            ;                                                            \
        SysTick->CTLR &= ~(1 << 0);                                      \
    }
extern void Set_MC_RGB(uint8_t channel, int num, uint8_t R, uint8_t G, uint8_t B);
#define MC_STU_RGB_set(channel, R, G, B) Set_MC_RGB(channel, 0, R, G, B)
#define MC_PULL_ONLINE_RGB_set(channel, R, G, B) Set_MC_RGB(channel, 1, R, G, B)
// #include "AMCU.h"
extern uint8_t channel_colors[4][4];
extern bool MC_STU_ERROR[4];
