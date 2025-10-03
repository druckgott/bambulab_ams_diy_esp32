#pragma once

#include "main.h"

struct Motion_SensorData {
    bool online;
    int raw_angle;
    int magnet_stu;
};

struct alignas(4) Motion_control_save_struct
{
    int Motion_control_dir[4];
    int check;
};

extern Motion_SensorData Motion_sensors[4];
extern Motion_control_save_struct Motion_control_data_save;
extern int16_t Motor_PWM_value[4];

extern void Motion_control_init();
extern void Motion_control_set_PWM(uint8_t CHx, int PWM);
extern void Motion_control_run(int error);