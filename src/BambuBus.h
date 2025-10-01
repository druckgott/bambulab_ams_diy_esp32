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

    struct _filament
    {
        char ID[8];
        uint8_t color_R;
        uint8_t color_G;
        uint8_t color_B;
        uint8_t color_A;
        int16_t temperature_min;
        int16_t temperature_max;
        char name[20];

        float meters;
        uint64_t meters_virtual_count;
        AMS_filament_stu statu;
        AMS_filament_motion motion_set;
        uint16_t pressure;
    };

    struct alignas(4) flash_save_struct
    {
        _filament filament[4];
        int BambuBus_now_filament_num;
        uint8_t filament_use_flag;
        uint32_t version;
        uint32_t check;
    };

    struct ram_core_struct {
        uint32_t heartbeat;           // Heartbeat-Zähler
        uint64_t last_heartbeat_time; // Zeitpunkt des letzten Pakets
        uint8_t  last_heartbeat_buf[16]; // oder eine passende Größe für dein Paket
        int      last_heartbeat_len;      // Länge des letzten Pakets
    };

    extern bool bambuBusDebugMode;
    extern flash_save_struct data_save;
    extern ram_core_struct ram_core;
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