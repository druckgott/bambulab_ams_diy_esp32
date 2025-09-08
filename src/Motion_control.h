#pragma once

#include "main.h"

struct alignas(4) Motion_control_save_struct
{
    int Motion_control_dir[4];
    int check;
};

extern Motion_control_save_struct Motion_control_data_save;

extern void Motion_control_init();
extern void Motion_control_set_PWM(uint8_t CHx, int PWM);
extern void Motion_control_run(int error);