#pragma once

#include "main.h"

#define Bambubus_version 5

#ifdef __cplusplus
extern "C"
{
#endif

    enum class AMS_filament_stu
    {
        offline,
        online,
        NFC_waiting
    };
    enum class AMS_filament_motion
    {
        before_pull_back,
        need_pull_back,
        need_send_out,
        on_use,
        idle
    };
    enum class BambuBus_package_type
    {
        ERROR = -1,
        NONE = 0,
        filament_motion_short,
        filament_motion_long,
        online_detect,
        REQx6,
        NFC_detect,
        set_filament_info,
        MC_online,
        read_filament_info,
        set_filament_info_type2,
        version,
        serial_number,
        heartbeat,
        ETC,
        __BambuBus_package_packge_type_size
    };
    enum BambuBus_device_type
    {
        BambuBus_none = 0x0000,
        BambuBus_AMS = 0x0700,
        BambuBus_AMS_lite = 0x1200,
    };
    extern void BambuBus_init();
    extern BambuBus_package_type BambuBus_run();
#define max_filament_num 4
    extern bool Bambubus_read();
    extern void Bambubus_set_need_to_save();
    extern int get_now_filament_num();
    extern uint16_t get_now_BambuBus_device_type();
    extern void reset_filament_meters(int num);
    extern void add_filament_meters(int num, float meters);
    extern float get_filament_meters(int num);
    extern void set_filament_online(int num, bool if_online);
    extern bool get_filament_online(int num);
    AMS_filament_motion get_filament_motion(int num);
    extern void set_filament_motion(int num, AMS_filament_motion motion);
    extern bool BambuBus_if_on_print();
#ifdef __cplusplus
}
#endif