#include "BambuBus.h"
#include "CRC16.h"
#include "CRC8.h"
#include <stdio.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "pins.h"

CRC16 crc_16;
CRC8 crc_8;

uint8_t BambuBus_data_buf[1000];
int BambuBus_have_data = 0;
uint16_t BambuBus_address = 0;

struct _filament
{
    // AMS statu
    char ID[8] = "GFG00";
    uint8_t color_R = 0xFF;
    uint8_t color_G = 0xFF;
    uint8_t color_B = 0xFF;
    uint8_t color_A = 0xFF;
    int16_t temperature_min = 220;
    int16_t temperature_max = 240;
    char name[20] = "PETG";

    float meters = 0;
    uint64_t meters_virtual_count = 0;
    AMS_filament_stu statu = AMS_filament_stu::online;
    // printer_set
    AMS_filament_motion motion_set = AMS_filament_motion::idle;
    uint16_t pressure = 0xFFFF;
};

#define use_flash_addr ((uint32_t)0x0800F000)

struct alignas(4) flash_save_struct
{
    _filament filament[4];
    int BambuBus_now_filament_num = 0xFF;
    uint8_t filament_use_flag = 0x00;
    uint32_t version = Bambubus_version;
    uint32_t check = 0x40614061;
} data_save;

bool Bambubus_read()
{
    flash_save_struct *ptr = (flash_save_struct *)(use_flash_addr);
    if ((ptr->check == 0x40614061) && (ptr->version == Bambubus_version))
    {
        memcpy(&data_save, ptr, sizeof(data_save));
        return true;
    }
    return false;
}
bool Bambubus_need_to_save = false;
void Bambubus_set_need_to_save()
{
    Bambubus_need_to_save = true;
}
void Bambubus_save()
{
    //Flash_saves(&data_save, sizeof(data_save), use_flash_addr);
}

int get_now_filament_num()
{
    return data_save.BambuBus_now_filament_num;
}
uint16_t get_now_BambuBus_device_type()
{
    return BambuBus_address;
}

void reset_filament_meters(int num)
{
    if (num < 4)
        data_save.filament[num].meters = 0;
}
void add_filament_meters(int num, float meters)
{
    if (num < 4)
    {
        if ((data_save.filament[num].motion_set == AMS_filament_motion::on_use) || (data_save.filament[num].motion_set == AMS_filament_motion::need_pull_back))
            data_save.filament[num].meters += meters;
    }
}
float get_filament_meters(int num)
{
    if (num < 4)
        return data_save.filament[num].meters;
    else
        return 0;
}
void set_filament_online(int num, bool if_online)
{
    if (num < 4)
    {
        if (if_online)
        {
            data_save.filament[num].statu = AMS_filament_stu::online;
        }
        else
        {
            data_save.filament[num].statu = AMS_filament_stu::offline;
            set_filament_motion(num, AMS_filament_motion::idle);
        }
    }
    else
    {
    }
}

bool get_filament_online(int num)
{
    if (num < 4)
    {
        if (data_save.filament[num].statu == AMS_filament_stu::offline)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        return false;
    }
}
void set_filament_motion(int num, AMS_filament_motion motion)
{
    if (num < 4)
    {
        _filament *filament = &(data_save.filament[num]);
        filament->motion_set = motion;
        if (motion == AMS_filament_motion::on_use)
            switch (motion)
            {
            case AMS_filament_motion::on_use:
            case AMS_filament_motion::before_pull_back:
                data_save.filament_use_flag = 0x04;
                break;
            case AMS_filament_motion::need_send_out:
                data_save.filament_use_flag = 0x02;
                break;
            case AMS_filament_motion::need_pull_back:
                data_save.filament_use_flag = 0x00;
                break;
            case AMS_filament_motion::idle:
                data_save.filament_use_flag = 0x00;
                break;
            }
    }
}
AMS_filament_motion get_filament_motion(int num)
{
    if (num < 4)
        return data_save.filament[num].motion_set;
    else
        return AMS_filament_motion::idle;
}
bool BambuBus_if_on_print()
{
    bool on_print = false;
    for (int i = 0; i < 4; i++)
    {
        if (data_save.filament[i].motion_set != AMS_filament_motion::idle)
        {
            on_print = true;
        }
    }
    return on_print;
}
uint8_t buf_X[1000];
CRC8 _RX_IRQ_crcx(0x39, 0x66, 0x00, false, false);
void inline RX_IRQ(unsigned char _RX_IRQ_data)
{
    static int _index = 0;
    static int length = 999;
    static uint8_t data_length_index;
    static uint8_t data_CRC8_index;
    unsigned char data = _RX_IRQ_data;

    if (_index == 0) // waitting for first data
    {
        if (data == 0x3D) // 0x3D-start
        {
            BambuBus_data_buf[0] = 0x3D;
            _RX_IRQ_crcx.restart();       // reset CRC8
            _RX_IRQ_crcx.add(0x3D);       // add 0x3D in CRC8
            data_length_index = 4;        // unknow package type,init length data to 4
            length = data_CRC8_index = 6; // unknow package length,,init package length to 6
            _index = 1;
        }
        return;
    }
    else // have 0x3D,normal data
    {
        BambuBus_data_buf[_index] = data;
        if (_index == 1) // package type byte
        {
            if (data & 0x80) // short head package
            {
                data_length_index = 2;
                data_CRC8_index = 3;
            }
            else // long head package
            {
                data_length_index = 4;
                data_CRC8_index = 6;
            }
        }
        if (_index == data_length_index) // the length byte
        {
            length = data;
        }
        if (_index < data_CRC8_index) // before CRC8 byte,add data
        {
            _RX_IRQ_crcx.add(data);
        }
        else if (_index == data_CRC8_index) // the CRC8 byte,check
        {
            if (data != _RX_IRQ_crcx.calc()) // check error,return to waiting 0x3D
            {
                _index = 0;
                return;
            }
        }
        ++_index;
        if (_index >= length) // recv over,copy package data
        {
            _index = 0;
            memcpy(buf_X, BambuBus_data_buf, length);
            BambuBus_have_data = length;
        }
        if (_index >= 999) // recv error,reset
        {
            _index = 0;
        }
    }
}

// Event-Queue für UART-Interrupts
static QueueHandle_t uart_queue;

// Forward-Deklaration deiner eigenen RX-Callback-Funktion
extern void RX_IRQ(uint16_t data);

void send_uart(const uint8_t *data, size_t length)
{
    gpio_set_level(DE_PIN, 1);   // RS485 DE einschalten -> Senden
    uart_write_bytes(UART_PORT, (const char *)data, length);
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100)); // Warten bis fertig
    gpio_set_level(DE_PIN, 0);   // RS485 DE aus -> zurück auf Empfang
}

//simple methode:
/*void send_uart(const uint8_t *data, size_t length)
{
    gpio_set_level(DE_PIN, 1);                 // Senden starten
    uart_write_bytes(UART_PORT, (const char *)data, length);
    gpio_set_level(DE_PIN, 0);                 // Senden fertig
}*/

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t data[2];

    for (;;) {
        if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                int len = uart_read_bytes(UART_PORT, data, sizeof(data), 20 / portTICK_PERIOD_MS);
                for (int i = 0; i < len; i++) RX_IRQ(data[i]);
            }
        }
    }
}

void BambuBUS_UART_Init()
{
    // UART Konfiguration (Arduino-ESP32)
    const uart_config_t uart_config = {
        .baud_rate = BAMBU_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,     // 9-Bit UART geht hier nicht
        .parity    = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
        // source_clk fällt weg
    };

    // Parameter anwenden
    uart_param_config(UART_PORT, &uart_config);

    // Pins setzen
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Treiber installieren: RX-Puffer 2kB, kein TX-Puffer, mit Queue für Events
    uart_driver_install(UART_PORT, 2048, 0, 20, &uart_queue, 0);

    // RS485 DE-Pin konfigurieren
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DE_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(DE_PIN, 0); // Anfangszustand: Empfang

    // Task starten, die UART-Events (Interrupts) behandelt
    xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 12, NULL);
}

void BambuBus_init()
{
    bool _init_ready = Bambubus_read();
    crc_8.reset(0x39, 0x66, 0, false, false);
    crc_16.reset(0x1021, 0x913D, 0, false, false);

    if (_init_ready)
    {
        for (int i = 0; i < 4; i++)
        {
            channel_colors[i][0] = data_save.filament[i].color_R;
            channel_colors[i][1] = data_save.filament[i].color_G;
            channel_colors[i][2] = data_save.filament[i].color_B;
            channel_colors[i][3] = data_save.filament[i].color_A;
        }
    }
    else
    {
        data_save.filament[0].color_R = 0xFF;
        data_save.filament[0].color_G = 0x00;
        data_save.filament[0].color_B = 0x00;
        data_save.filament[1].color_R = 0x00;
        data_save.filament[1].color_G = 0xFF;
        data_save.filament[1].color_B = 0x00;
        data_save.filament[2].color_R = 0x00;
        data_save.filament[2].color_G = 0x00;
        data_save.filament[2].color_B = 0xFF;
        data_save.filament[3].color_R = 0x88;
        data_save.filament[3].color_G = 0x88;
        data_save.filament[3].color_B = 0x88;
    }
    for (auto &j : data_save.filament)
    {
#ifdef _Bambubus_DEBUG_mode_
        j.statu = AMS_filament_stu::online;
#else
        j.statu = AMS_filament_stu::offline;
#endif // DEBUG

        j.motion_set = AMS_filament_motion::idle;
        j.meters = 0;
    }
    data_save.BambuBus_now_filament_num = 0xFF;

    BambuBUS_UART_Init();
}

bool package_check_crc16(uint8_t *data, int data_length)
{
    crc_16.restart();
    data_length -= 2;
    for (auto i = 0; i < data_length; i++)
    {
        crc_16.add(data[i]);
    }
    uint16_t num = crc_16.calc();
    if ((data[(data_length)] == (num & 0xFF)) && (data[(data_length + 1)] == ((num >> 8) & 0xFF)))
        return true;
    return false;
}
bool need_debug = false;
void package_send_with_crc(uint8_t *data, int data_length)
{

    crc_8.restart();
    if (data[1] & 0x80)
    {
        for (auto i = 0; i < 3; i++)
        {
            crc_8.add(data[i]);
        }
        data[3] = crc_8.calc();
    }
    else
    {
        for (auto i = 0; i < 6; i++)
        {
            crc_8.add(data[i]);
        }
        data[6] = crc_8.calc();
    }
    crc_16.restart();
    data_length -= 2;
    for (auto i = 0; i < data_length; i++)
    {
        crc_16.add(data[i]);
    }
    uint16_t num = crc_16.calc();
    data[(data_length)] = num & 0xFF;
    data[(data_length + 1)] = num >> 8;
    data_length += 2;
    send_uart(data, data_length);
    if (need_debug)
    {
        memcpy(buf_X + BambuBus_have_data, data, data_length);
        DEBUG_num(buf_X, BambuBus_have_data + data_length);
        need_debug = false;
    }
}

uint8_t packge_send_buf[1000];

#pragma pack(push, 1) // Strukturen auf 1-Byte-Grenze ausrichten
struct long_packge_data
{
    uint16_t package_number;
    uint16_t package_length;
    uint8_t crc8;
    uint16_t target_address;
    uint16_t source_address;
    uint16_t type;
    uint8_t *datas;
    uint16_t data_length;
};
#pragma pack(pop) // Standard-Ausrichtung wiederherstellen

void Bambubus_long_package_send(long_packge_data *data)
{
    packge_send_buf[0] = 0x3D;
    packge_send_buf[1] = 0x00;
    data->package_length = data->data_length + 15;
    memcpy(packge_send_buf + 2, data, 11);
    memcpy(packge_send_buf + 13, data->datas, data->data_length);
    package_send_with_crc(packge_send_buf, data->data_length + 15);
}

void Bambubus_long_package_analysis(uint8_t *buf, int data_length, long_packge_data *data)
{
    memcpy(data, buf + 2, 11);
    data->datas = buf + 13;
    data->data_length = data_length - 15; // +2byte CRC16
}

long_packge_data printer_data_long;
BambuBus_package_type get_packge_type(unsigned char *buf, int length)
{
    if (package_check_crc16(buf, length) == false)
    {
        return BambuBus_package_type::NONE;
    }
    if (buf[1] == 0xC5)
    {

        switch (buf[4])
        {
        case 0x03:
            return BambuBus_package_type::filament_motion_short;
        case 0x04:
            return BambuBus_package_type::filament_motion_long;
        case 0x05:
            return BambuBus_package_type::online_detect;
        case 0x06:
            return BambuBus_package_type::REQx6;
        case 0x07:
            return BambuBus_package_type::NFC_detect;
        case 0x08:
            return BambuBus_package_type::set_filament_info;
        case 0x20:
            return BambuBus_package_type::heartbeat;
        default:
            return BambuBus_package_type::ETC;
        }
    }
    else if (buf[1] == 0x05)
    {
        Bambubus_long_package_analysis(buf, length, &printer_data_long);
        if (printer_data_long.target_address == BambuBus_AMS)
        {
            BambuBus_address = BambuBus_AMS;
        }
        else if (printer_data_long.target_address == BambuBus_AMS_lite)
        {
            BambuBus_address = BambuBus_AMS_lite;
        }

        switch (printer_data_long.type)
        {
        case 0x21A:
            return BambuBus_package_type::MC_online;
        case 0x211:
            return BambuBus_package_type::read_filament_info;
        case 0x218:
            return BambuBus_package_type::set_filament_info_type2;
        case 0x103:
            return BambuBus_package_type::version;
        case 0x402:
            return BambuBus_package_type::serial_number;
        default:
            return BambuBus_package_type::ETC;
        }
    }
    return BambuBus_package_type::NONE;
}
uint8_t package_num = 0;

uint8_t get_filament_left_char()
{
    uint8_t data = 0;
    for (int i = 0; i < 4; i++)
    {
        if (data_save.filament[i].statu == AMS_filament_stu::online)
        {
            data |= (0x1 << i) << i; // 1<<(2*i)
            if (BambuBus_address == BambuBus_AMS)
                if (data_save.filament[i].motion_set != AMS_filament_motion::idle)
                {
                    data |= (0x2 << i) << i; // 2<<(2*i)
                }
        }
    }
    return data;
}

void set_motion_res_datas(unsigned char *set_buf, unsigned char read_num)
{
    float meters = 0;
    uint16_t pressure = 0xFFFF;
    if ((read_num != 0xFF) && (read_num < 4))
    {
        meters = data_save.filament[read_num].meters;
        if (BambuBus_address == BambuBus_AMS_lite)
        {
            meters = -meters;
        }
        pressure = data_save.filament[read_num].pressure;
    }
    set_buf[0] = BambuBus_AMS_num;
    set_buf[1] = 0x00;
    set_buf[2] = data_save.filament_use_flag;
    set_buf[3] = read_num; // filament number or maybe using number
    memcpy(set_buf + 4, &meters, sizeof(float));
    memcpy(set_buf + 8, &pressure, sizeof(uint16_t));
    set_buf[24] = get_filament_left_char();
}
bool set_motion(unsigned char read_num, unsigned char statu_flags, unsigned char fliment_motion_flag)
{
    static uint64_t time_last = 0;
    uint64_t time_now = get_time64();
    uint64_t time_used = time_now - time_last;
    time_last = time_now;
    if (BambuBus_address == BambuBus_AMS) // AMS08
    {
        if (read_num < 4)
        {
            if ((statu_flags == 0x03) && (fliment_motion_flag == 0x00)) // 03 00
            {
                if (data_save.BambuBus_now_filament_num != read_num) // on change
                {
                    if (data_save.BambuBus_now_filament_num < 4)
                    {
                        data_save.filament[data_save.BambuBus_now_filament_num].motion_set = AMS_filament_motion::idle;
                        data_save.filament_use_flag = 0x00;
                        data_save.filament[data_save.BambuBus_now_filament_num].pressure = 0xFFFF;
                    }
                    data_save.BambuBus_now_filament_num = read_num;
                }
                data_save.filament[read_num].motion_set = AMS_filament_motion::need_send_out;
                data_save.filament_use_flag = 0x02;
                data_save.filament[read_num].pressure = 0x4700;
            }
            else if ((statu_flags == 0x09)) // 09 A5 / 09 3F
            {
                if (data_save.filament[read_num].motion_set == AMS_filament_motion::need_send_out)
                {
                    data_save.filament[read_num].motion_set = AMS_filament_motion::on_use;
                    data_save.filament_use_flag = 0x04;
                    data_save.filament[read_num].meters_virtual_count = 0;
                }
                else if (data_save.filament[read_num].meters_virtual_count < 10000) // 10s virtual data
                {
                    data_save.filament[read_num].meters += (float)time_used / 300000; // 3.333mm/s
                    data_save.filament[read_num].meters_virtual_count += time_used;
                }
                data_save.filament[read_num].pressure = 0x2B00;
            }
            else if ((statu_flags == 0x07) && (fliment_motion_flag == 0x7F)) // 07 7F
            {
                data_save.filament[read_num].motion_set = AMS_filament_motion::on_use;
                data_save.filament_use_flag = 0x04;
                data_save.filament[read_num].pressure = 0x2B00;
            }
        }
        else if ((read_num == 0xFF))
        {
            if ((statu_flags == 0x03) && (fliment_motion_flag == 0x00)) // 03 00(FF)
            {
                _filament *filament = &(data_save.filament[data_save.BambuBus_now_filament_num]);
                if (data_save.BambuBus_now_filament_num < 4)
                {
                    if (filament->motion_set == AMS_filament_motion::on_use)
                    {
                        filament->motion_set = AMS_filament_motion::need_pull_back;
                        data_save.filament_use_flag = 0x02;
                    }
                    filament->pressure = 0x4700;
                }
            }
            else
            {
                for (auto i = 0; i < 4; i++)
                {
                    data_save.filament[i].motion_set = AMS_filament_motion::idle;
                    data_save.filament[i].pressure = 0xFFFF;
                }
            }
        }
    }
    else if (BambuBus_address == BambuBus_AMS_lite) // AMS lite
    {
        if (read_num < 4)
        {
            if ((statu_flags == 0x03) && (fliment_motion_flag == 0x3F)) // 03 3F
            {
                data_save.filament[read_num].motion_set = AMS_filament_motion::need_pull_back;
                data_save.filament_use_flag = 0x00;
            }
            else if ((statu_flags == 0x03) && (fliment_motion_flag == 0xBF)) // 03 BF
            {
                data_save.BambuBus_now_filament_num = read_num;
                if (data_save.filament[read_num].motion_set != AMS_filament_motion::need_send_out)
                {
                    for (int i = 0; i < 4; i++)
                    {
                        data_save.filament[i].motion_set = AMS_filament_motion::idle;
                    }
                }
                data_save.filament[read_num].motion_set = AMS_filament_motion::need_send_out;
                data_save.filament_use_flag = 0x02;
            }
            else if ((statu_flags == 0x07) && (fliment_motion_flag == 0x00)) // 07 00
            {
                data_save.BambuBus_now_filament_num = read_num;

                if ((data_save.filament[read_num].motion_set == AMS_filament_motion::need_send_out) || (data_save.filament[read_num].motion_set == AMS_filament_motion::idle))
                {
                    data_save.filament[read_num].motion_set = AMS_filament_motion::on_use;
                    data_save.filament[read_num].meters_virtual_count = 0;
                }
                else if (data_save.filament[read_num].motion_set == AMS_filament_motion::before_pull_back)
                {
                }
                else if (data_save.filament[read_num].meters_virtual_count < 10000) // 10s virtual data
                {
                    data_save.filament[read_num].meters += (float)time_used / 300000; // 3.333mm/s
                    data_save.filament[read_num].meters_virtual_count += time_used;
                }
                if (data_save.filament[read_num].motion_set == AMS_filament_motion::on_use)
                {
                    data_save.filament_use_flag = 0x04;
                }
                /*if (data_save.filament[read_num].motion_set == need_pull_back)
                {
                    data_save.filament[read_num].motion_set = idle;
                    data_save.filament_use_flag = 0x00;
                }*/
            }
            else if ((statu_flags == 0x07) && (fliment_motion_flag == 0x66)) // 07 66 printer ready return back filament
            {
                data_save.filament[read_num].motion_set = AMS_filament_motion::before_pull_back;
            }
            else if ((statu_flags == 0x07) && (fliment_motion_flag == 0x26)) // 07 26 printer ready pull in filament
            {
                data_save.filament_use_flag = 0x04;
            }
        }
        else if ((read_num == 0xFF) && (statu_flags == 0x01))
        {
            AMS_filament_motion motion = data_save.filament[data_save.BambuBus_now_filament_num].motion_set;
            if (motion != AMS_filament_motion::on_use)
            {
                for (int i = 0; i < 4; i++)
                {
                    data_save.filament[i].motion_set = AMS_filament_motion::idle;
                }
                data_save.filament_use_flag = 0x00;
            }
        }
    }
    else if (BambuBus_address == BambuBus_none) // none
    {
        /*if ((read_num != 0xFF) && (read_num < 4))
        {
            if ((statu_flags == 0x07) && (fliment_motion_flag == 0x00)) // 07 00
            {
                data_save.BambuBus_now_filament_num =  read_num;
                data_save.filament[read_num].motion_set = on_use;
            }
        }*/
    }
    else
        return false;
    return true;
}
// 3D E0 3C 12 04 00 00 00 00 09 09 09 00 00 00 00 00 00 00
// 02 00 E9 3F 14 BF 00 00 76 03 6A 03 6D 00 E5 FB 99 14 2E 19 6A 03 41 F4 C3 BE E8 01 01 01 01 00 00 00 00 64 64 64 64 0A 27
// 3D E0 2C C9 03 00 00
// 04 01 79 30 61 BE 00 00 03 00 44 00 12 00 FF FF FF FF 00 00 44 00 54 C1 F4 EE E7 01 01 01 01 00 00 00 00 FA 35
#define C_test 0x00, 0x00, 0x00, 0x00, \
               0x00, 0x00, 0x80, 0xBF, \
               0x00, 0x00, 0x00, 0x00, \
               0x36, 0x00, 0x00, 0x00, \
               0x00, 0x00, 0x00, 0x00, \
               0x00, 0x00, 0x27, 0x00, \
               0x55,                   \
               0xFF, 0xFF, 0xFF, 0xFF, \
               0x01, 0x01, 0x01, 0x01,
/*#define C_test 0x00, 0x00, 0x02, 0x02, \
               0x00, 0x00, 0x00, 0x00, \
               0x00, 0x00, 0x00, 0xC0, \
               0x36, 0x00, 0x00, 0x00, \
               0xFC, 0xFF, 0xFC, 0xFF, \
               0x00, 0x00, 0x27, 0x00, \
               0x55,                   \
               0xC1, 0xC3, 0xEC, 0xBC, \
               0x01, 0x01, 0x01, 0x01,
00 00 02 02 EB 8F CA 3F 49 48 E7 1C 97 00 E7 1B F3 FF F2 FF 00 00 90 00 75 F8 EE FC F0 B6 B8 F8 B0 00 00 00 00 FF FF FF FF*/
/*
#define C_test 0x00, 0x00, 0x02, 0x01, \
                0xF8, 0x65, 0x30, 0xBF, \
                0x00, 0x00, 0x28, 0x03, \
                0x2A, 0x03, 0x6F, 0x00, \
                0xB6, 0x04, 0xFC, 0xEC, \
                0xDF, 0xE7, 0x44, 0x00, \
                0x04, \
                0xC3, 0xF2, 0xBF, 0xBC, \
                0x01, 0x01, 0x01, 0x01,*/
unsigned char Cxx_res[] = {0x3D, 0xE0, 0x2C, 0x1A, 0x03,
                           C_test 0x00, 0x00, 0x00, 0x00,
                           0x90, 0xE4};
void send_for_motion_short(unsigned char *buf, int length)
{
    Cxx_res[1] = 0xC0 | (package_num << 3);
    unsigned char AMS_num = buf[5];
    if (AMS_num != BambuBus_AMS_num)
        return;
    // unsigned char statu_flags = buf[6];
    unsigned char read_num = buf[7];
    // unsigned char fliment_motion_flag = buf[8];

    // if (!set_motion(AMS_num, read_num, statu_flags, fliment_motion_flag))
    //     return;

    set_motion_res_datas(Cxx_res + 5, read_num);
    package_send_with_crc(Cxx_res, sizeof(Cxx_res));
    if (package_num < 7)
        package_num++;
    else
        package_num = 0;
}
/*
0x00, 0x00, 0x00, 0xFF, // 0x0C...
0x00, 0x00, 0x80, 0xBF, // distance
0x00, 0x00, 0x00, 0xC0,
0x00, 0xC0, 0x5D, 0xFF,
0xFE, 0xFF, 0xFE, 0xFF, // 0xFE, 0xFF, 0xFE, 0xFF,
0x00, 0x44, 0x00, 0x00,
0x10,
0xC1, 0xC3, 0xEC, 0xBC,
0x01, 0x01, 0x01, 0x01,
*/
unsigned char Dxx_res[] = {0x3D, 0xE0, 0x3C, 0x1A, 0x04,
                           0x00, //[5]AMS num
                           0x00,
                           0x00,
                           1,                      // humidity wet
                           0x04, 0x04, 0x04, 0xFF, // flags
                           0x00, 0x00, 0x00, 0x00,
                           C_test 0x00, 0x00, 0x00, 0x00,
                           0x64, 0x64, 0x64, 0x64,
                           0x90, 0xE4};

bool need_res_for_06 = false;
uint8_t res_for_06_num = 0xFF;
int last_detect = 0;
uint8_t filament_flag_detected = 0;

void send_for_motion_long(unsigned char *buf, int length)
{
    unsigned char filament_flag_on = 0x00;
    unsigned char filament_flag_NFC = 0x00;
    unsigned char AMS_num = buf[5];
    unsigned char statu_flags = buf[6];
    unsigned char fliment_motion_flag = buf[7];
    unsigned char read_num = buf[9];
    if (AMS_num != BambuBus_AMS_num)
        return;
    for (auto i = 0; i < 4; i++)
    {
        // filament[i].meters;
        if (data_save.filament[i].statu == AMS_filament_stu::online)
        {
            filament_flag_on |= 1 << i;
        }
        else if (data_save.filament[i].statu == AMS_filament_stu::NFC_waiting)
        {
            filament_flag_on |= 1 << i;
            filament_flag_NFC |= 1 << i;
        }
    }
    if (!set_motion(read_num, statu_flags, fliment_motion_flag))
        return;
    /*if (need_res_for_06)
    {
        Dxx_res2[1] = 0xC0 | (package_num << 3);
        Dxx_res2[9] = filament_flag_on;
        Dxx_res2[10] = filament_flag_on - filament_flag_NFC;
        Dxx_res2[11] = filament_flag_on - filament_flag_NFC;
        Dxx_res[19] = motion_flag;
        Dxx_res[20] = Dxx_res2[12] = res_for_06_num;
        Dxx_res2[13] = filament_flag_NFC;
        Dxx_res2[41] = get_filament_left_char();
        package_send_with_crc(Dxx_res2, sizeof(Dxx_res2));
        need_res_for_06 = false;
    }
    else*/

    {
        Dxx_res[1] = 0xC0 | (package_num << 3);
        Dxx_res[5] = BambuBus_AMS_num;
        Dxx_res[8] = AMS_humidity_wet;
        Dxx_res[9] = filament_flag_on;
        Dxx_res[10] = filament_flag_on - filament_flag_NFC;
        Dxx_res[11] = filament_flag_on - filament_flag_NFC;
        Dxx_res[12] = read_num;
        Dxx_res[13] = filament_flag_NFC;

        set_motion_res_datas(Dxx_res + 17, read_num);
    }
    /*if (last_detect != 0)//本用于模拟NFC探测过程
    {
        if (last_detect > 10)
        {
            Dxx_res[19] = 0x01;
        }
        else
        {
            Dxx_res[12] = filament_flag_detected;
            Dxx_res[19] = 0x01;
            Dxx_res[20] = filament_flag_detected;
        }
        last_detect--;
    }*/
    package_send_with_crc(Dxx_res, sizeof(Dxx_res));
    if (package_num < 7)
        package_num++;
    else
        package_num = 0;
}
unsigned char REQx6_res[] = {0x3D, 0xE0, 0x3C, 0x1A, 0x06,
                             0x00, 0x00, 0x00, 0x00,
                             0x04, 0x04, 0x04, 0xFF, // flags
                             0x00, 0x00, 0x00, 0x00,
                             C_test 0x00, 0x00, 0x00, 0x00,
                             0x64, 0x64, 0x64, 0x64,
                             0x90, 0xE4};
void send_for_REQx6(unsigned char *buf, int length)
{
    /*
        unsigned char filament_flag_on = 0x00;
        unsigned char filament_flag_NFC = 0x00;
        for (auto i = 0; i < 4; i++)
        {
            if (data_save.filament[i].statu == online)
            {
                filament_flag_on |= 1 << i;
            }
            else if (data_save.filament[i].statu == NFC_waiting)
            {
                filament_flag_on |= 1 << i;
                filament_flag_NFC |= 1 << i;
            }
        }
        REQx6_res[1] = 0xC0 | (package_num << 3);
        res_for_06_num = buf[7];
        REQx6_res[9] = filament_flag_on;
        REQx6_res[10] = filament_flag_on - filament_flag_NFC;
        REQx6_res[11] = filament_flag_on - filament_flag_NFC;
        Dxx_res2[12] = res_for_06_num;
        Dxx_res2[12] = res_for_06_num;
        package_send_with_crc(REQx6_res, sizeof(REQx6_res));
        need_res_for_06 = true;
        if (package_num < 7)
            package_num++;
        else
            package_num = 0;*/
}

void NFC_detect_run()
{
    /*uint64_t time = GetTick();
    return;
    if (time > last_detect + 3000)
    {
        filament_flag_detected = 0;
    }*/
}
uint8_t online_detect_res[29] = {
    0x3D, 0xC0, 0x1D, 0xB4, 0x05, 0x01, 0x00,
    0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x33, 0xF0};
bool have_registered = false;
void online_detect_init()
{
    have_registered = false;
}
void send_for_online_detect(unsigned char *buf, int length)
{
    if ((buf[5] == 0x00)) // Wird zur Registrierung der AMS-Nummer verwendet
    {
        if (have_registered == true)
            return;
        int i = BambuBus_AMS_num;
        while (i--)
        {
            delay(1); // Trennt AMS-Datenpakete mit unterschiedlichen Nummern
        }
        online_detect_res[0] = 0x3D;             // Frame-Kopf
        online_detect_res[1] = 0xC0;             // Flag
        online_detect_res[2] = 29;               // Datenlänge – 29 Bytes
        online_detect_res[3] = 0xB4;             // CRC8
        online_detect_res[4] = 0x05;             // Befehlsnummer
        online_detect_res[5] = 0x00;             // Befehlsnummer
        online_detect_res[6] = BambuBus_AMS_num; // AMS-Nummer

        online_detect_res[7] = BambuBus_AMS_num; // Eigentlich eine Seriennummer, hier durch AMS-Nummer ersetzt
        online_detect_res[8] = BambuBus_AMS_num; // Eigentlich eine Seriennummer, hier durch AMS-Nummer ersetzt

        package_send_with_crc(online_detect_res, sizeof(online_detect_res));
    }

    if ((buf[5] == 0x01) && (buf[6] == BambuBus_AMS_num))
    {

        online_detect_res[0] = 0x3D;                                         // Frame-Kopf
        online_detect_res[1] = 0xC0;                                         // Flag
        online_detect_res[2] = 29;                                           // Datenlänge – 29 Bytes
        online_detect_res[3] = 0xB4;                                         // CRC8
        online_detect_res[4] = 0x05;                                         // Befehlsnummer
        online_detect_res[5] = 0x01;                                         // Befehlsnummer
        online_detect_res[6] = BambuBus_AMS_num;                             // AMS-Nummer
        memcpy(online_detect_res + 7, buf + 7, 20);                          // AMS-Registrierungsnummer kopieren
        package_send_with_crc(online_detect_res, sizeof(online_detect_res)); // Datenpaket mit CRC senden

        if (have_registered == false)
            if (memcmp(online_detect_res + 7, buf + 7, 20) == 0)
            {
                have_registered = true;
            }
    }
}
// 3D C5 0D F1 07 00 00 00 00 00 00 CE EC
// 3D C0 0D 6F 07 00 00 00 00 00 00 9A 70

unsigned char NFC_detect_res[] = {0x3D, 0xC0, 0x0D, 0x6F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0xE8};
void send_for_NFC_detect(unsigned char *buf, int length)
{
    last_detect = 20;
    filament_flag_detected = 1 << buf[6];
    NFC_detect_res[6] = buf[6];
    NFC_detect_res[7] = buf[7];
    package_send_with_crc(NFC_detect_res, sizeof(NFC_detect_res));
}

unsigned char long_packge_MC_online[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
void send_for_long_packge_MC_online(unsigned char *buf, int length)
{
    long_packge_data data;
    uint8_t AMS_num = printer_data_long.datas[0];
    if (AMS_num != BambuBus_AMS_num)
        return;
    Bambubus_long_package_analysis(buf, length, &printer_data_long);
    if (printer_data_long.target_address == 0x0700)
    {
    }
    else if (printer_data_long.target_address == 0x1200)
    {
    }
    /*else if(printer_data_long.target_address==0x0F00)
    {

    }*/
    else
    {
        return;
    }

    data.datas = long_packge_MC_online;
    data.datas[0] = BambuBus_AMS_num;
    data.data_length = sizeof(long_packge_MC_online);

    data.package_number = printer_data_long.package_number;
    data.type = printer_data_long.type;
    data.source_address = printer_data_long.target_address;
    data.target_address = printer_data_long.source_address;
    Bambubus_long_package_send(&data);
}
unsigned char long_packge_filament[] =
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x47, 0x46, 0x42, 0x30, 0x30, 0x00, 0x00, 0x00,
        0x41, 0x42, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xDD, 0xB1, 0xD4, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x18, 0x01, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
void send_for_long_packge_filament(unsigned char *buf, int length)
{
    long_packge_data data;
    Bambubus_long_package_analysis(buf, length, &printer_data_long);

    uint8_t AMS_num = printer_data_long.datas[0];
    uint8_t filament_num = printer_data_long.datas[1];
    if (AMS_num != BambuBus_AMS_num)
        return;
    long_packge_filament[0] = BambuBus_AMS_num;
    long_packge_filament[1] = filament_num;
    memcpy(long_packge_filament + 19, data_save.filament[filament_num].ID, sizeof(data_save.filament[filament_num].ID));
    memcpy(long_packge_filament + 27, data_save.filament[filament_num].name, sizeof(data_save.filament[filament_num].name));

    // Globale Farbvariablen aktualisieren
    channel_colors[filament_num][0] = data_save.filament[filament_num].color_R;
    channel_colors[filament_num][1] = data_save.filament[filament_num].color_G;
    channel_colors[filament_num][2] = data_save.filament[filament_num].color_B;
    channel_colors[filament_num][3] = data_save.filament[filament_num].color_A;

    long_packge_filament[59] = data_save.filament[filament_num].color_R;
    long_packge_filament[60] = data_save.filament[filament_num].color_G;
    long_packge_filament[61] = data_save.filament[filament_num].color_B;
    long_packge_filament[62] = data_save.filament[filament_num].color_A;
    memcpy(long_packge_filament + 79, &data_save.filament[filament_num].temperature_max, 2);
    memcpy(long_packge_filament + 81, &data_save.filament[filament_num].temperature_min, 2);

    data.datas = long_packge_filament;
    data.data_length = sizeof(long_packge_filament);

    data.package_number = printer_data_long.package_number;
    data.type = printer_data_long.type;
    data.source_address = printer_data_long.target_address;
    data.target_address = printer_data_long.source_address;
    Bambubus_long_package_send(&data);
}
unsigned char serial_number[] = {"STUDY0ONLY"};
unsigned char long_packge_version_serial_number[] = {9, // length
                                                     'S', 'T', 'U', 'D', 'Y', 'O', 'N', 'L', 'Y', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // serial_number#2
                                                     0x30, 0x30, 0x30, 0x30,
                                                     0xFF, 0xFF, 0xFF, 0xFF,
                                                     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBB, 0x44, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

void send_for_long_packge_serial_number(unsigned char *buf, int length)
{
    long_packge_data data;
    Bambubus_long_package_analysis(buf, length, &printer_data_long);
    uint8_t AMS_num = printer_data_long.datas[33];
    if (AMS_num != BambuBus_AMS_num)
        return;
    if ((printer_data_long.target_address != BambuBus_AMS) && (printer_data_long.target_address != BambuBus_AMS_lite))
    {
        return;
    }

    long_packge_version_serial_number[0] = sizeof(serial_number);
    memcpy(long_packge_version_serial_number + 1, serial_number, sizeof(serial_number));
    data.datas = long_packge_version_serial_number;
    data.data_length = sizeof(long_packge_version_serial_number);
    data.datas[65] = BambuBus_AMS_num;

    data.package_number = printer_data_long.package_number;
    data.type = printer_data_long.type;
    data.source_address = printer_data_long.target_address;
    data.target_address = printer_data_long.source_address;
    Bambubus_long_package_send(&data);
}

unsigned char long_packge_version_version_and_name_AMS_lite[] = {0x03, 0x02, 0x01, 0x00, // version number (00.01.02.03)
                                                                 0x41, 0x4D, 0x53, 0x5F, 0x46, 0x31, 0x30, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};  // AMS_F102 hw code name 
unsigned char long_packge_version_version_and_name_AMS08[] = {0x31, 0x06, 0x00, 0x00, // version number (00.00.06.49)
                                                              0x41, 0x4D, 0x53, 0x30, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void send_for_long_packge_version(unsigned char *buf, int length)
{
    long_packge_data data;
    Bambubus_long_package_analysis(buf, length, &printer_data_long);
    uint8_t AMS_num = printer_data_long.datas[0];
    if (AMS_num != BambuBus_AMS_num)
        return;
    unsigned char *long_packge_version_version_and_name;

    if (printer_data_long.target_address == BambuBus_AMS)
    {
        long_packge_version_version_and_name = long_packge_version_version_and_name_AMS08;
    }
    else if (printer_data_long.target_address == BambuBus_AMS_lite)
    {
        long_packge_version_version_and_name = long_packge_version_version_and_name_AMS_lite;
    }
    else
    {
        return;
    }

    data.datas = long_packge_version_version_and_name;
    data.data_length = sizeof(long_packge_version_version_and_name_AMS08);
    data.datas[20] = BambuBus_AMS_num;

    data.package_number = printer_data_long.package_number;
    data.type = printer_data_long.type;
    data.source_address = printer_data_long.target_address;
    data.target_address = printer_data_long.source_address;
    Bambubus_long_package_send(&data);
}
unsigned char s = 0x01;

unsigned char Set_filament_res[] = {0x3D, 0xC0, 0x08, 0xB2, 0x08, 0x60, 0xB4, 0x04};
void send_for_set_filament(unsigned char *buf, int length)
{
    uint8_t read_num = buf[5];
    uint8_t AMS_num = read_num & 0xF0;
    if (AMS_num != BambuBus_AMS_num)
        return;
    read_num = read_num & 0x0F;
    memcpy(data_save.filament[read_num].ID, buf + 7, sizeof(data_save.filament[read_num].ID));
    data_save.filament[read_num].color_R = buf[15];
    data_save.filament[read_num].color_G = buf[16];
    data_save.filament[read_num].color_B = buf[17];
    data_save.filament[read_num].color_A = buf[18];

    memcpy(&data_save.filament[read_num].temperature_min, buf + 19, 2);
    memcpy(&data_save.filament[read_num].temperature_max, buf + 21, 2);
    memcpy(data_save.filament[read_num].name, buf + 23, sizeof(data_save.filament[read_num].name));
    package_send_with_crc(Set_filament_res, sizeof(Set_filament_res));
    Bambubus_set_need_to_save();
}
unsigned char Set_filament_res_type2[] = {0x00, 0x00, 0x00};
void send_for_set_filament_type2(unsigned char *buf, int length)
{
    long_packge_data data;
    Bambubus_long_package_analysis(buf, length, &printer_data_long);
    uint8_t AMS_num = printer_data_long.datas[0];
    if (AMS_num != BambuBus_AMS_num)
        return;
    uint8_t read_num = printer_data_long.datas[1];
    memcpy(data_save.filament[read_num].ID, printer_data_long.datas + 2, sizeof(data_save.filament[read_num].ID));

    data_save.filament[read_num].color_R = printer_data_long.datas[10];
    data_save.filament[read_num].color_G = printer_data_long.datas[11];
    data_save.filament[read_num].color_B = printer_data_long.datas[12];
    data_save.filament[read_num].color_A = printer_data_long.datas[13];

    memcpy(&data_save.filament[read_num].temperature_min, printer_data_long.datas + 14, 2);
    memcpy(&data_save.filament[read_num].temperature_max, printer_data_long.datas + 16, 2);
    memcpy(data_save.filament[read_num].name, printer_data_long.datas + 18, 16);
    Bambubus_set_need_to_save();

    Set_filament_res_type2[0]=BambuBus_AMS_num;
    Set_filament_res_type2[1]=read_num;
    Set_filament_res_type2[2]=0x00;
    data.datas = Set_filament_res_type2;
    data.data_length = sizeof(Set_filament_res_type2);

    data.package_number = printer_data_long.package_number;
    data.type = printer_data_long.type;
    data.source_address = printer_data_long.target_address;
    data.target_address = printer_data_long.source_address;
    Bambubus_long_package_send(&data);
    
}

BambuBus_package_type BambuBus_run()
{
    BambuBus_package_type stu = BambuBus_package_type::NONE;
    static uint64_t time_set = 0;
    static uint64_t time_motion = 0;

    uint64_t timex = get_time64();

    /*for (auto i : data_save.filament)
    {
        i->motion_set = idle;
    }*/

    if (BambuBus_have_data)
    {
        int data_length = BambuBus_have_data;
        BambuBus_have_data = 0;
        need_debug = false;
        delay(1);
        stu = get_packge_type(buf_X, data_length); // have_data
        switch (stu)
        {
        case BambuBus_package_type::heartbeat:
            time_set = timex + 1000;
            break;
        case BambuBus_package_type::filament_motion_short:
            send_for_motion_short(buf_X, data_length);
            break;
        case BambuBus_package_type::filament_motion_long:
            // need_debug=true;
            send_for_motion_long(buf_X, data_length);
            time_motion = timex + 1000;
            break;
        case BambuBus_package_type::online_detect:
            send_for_online_detect(buf_X, data_length);
            break;
        case BambuBus_package_type::REQx6:
            // send_for_REQx6(buf_X, data_length);
            break;
        case BambuBus_package_type::MC_online:
            send_for_long_packge_MC_online(buf_X, data_length);
            break;
        case BambuBus_package_type::read_filament_info:
            send_for_long_packge_filament(buf_X, data_length);
            break;
        case BambuBus_package_type::version:
            send_for_long_packge_version(buf_X, data_length);
            break;
        case BambuBus_package_type::serial_number:
            send_for_long_packge_serial_number(buf_X, data_length);
            break;
        case BambuBus_package_type::NFC_detect:
            // send_for_NFC_detect(buf_X, data_length);
            break;
        case BambuBus_package_type::set_filament_info:
            send_for_set_filament(buf_X, data_length);
            break;
        case BambuBus_package_type::set_filament_info_type2:
            send_for_set_filament_type2(buf_X,data_length);
            break;
        default:
            break;
        }
    }
    if (timex > time_set)
    {
        stu = BambuBus_package_type::ERROR; // offline
    }
    if (timex > time_motion)
    {
        // set_filament_motion(get_now_filament_num(),idle);
        /*for(auto i:data_save.filament)
        {
            i->motion_set=idle;
        }*/
    }
    if (Bambubus_need_to_save)
    {
        Bambubus_save();
        time_set = get_time64() + 1000;
        Bambubus_need_to_save = false;
    }

    // NFC_detect_run();
    return stu;
}

