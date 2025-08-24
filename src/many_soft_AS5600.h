#pragma once
#include <Arduino.h>

class AS5600_soft_IIC_many
{
public:
    AS5600_soft_IIC_many();
    ~AS5600_soft_IIC_many();

    // ESP32-kompatibel: uint8_t für Pins
    void init(uint8_t *GPIO_SCL, uint8_t *GPIO_SDA, int num);

    void updata_angle();   // <-- verschoben nach public
    void updata_stu();     // <-- auch falls gebraucht

    bool *online;
    enum _AS5600_magnet_stu
    {
        low = 1,
        high = 2,
        offline = -1,
        normal = 0
    } * magnet_stu;

    uint16_t *raw_angle;
    uint16_t *data;
    int numbers;

private:
    int *error;
    uint8_t *IO_SDA;   // Pin-Nummern
    uint8_t *IO_SCL;

    void init_iic();
    void start_iic(unsigned char ADR);
    void stop_iic();
    void write_iic(uint8_t byte);
    void read_iic(bool ack);
    void wait_ack_iic();
    void clear_datas();
    void read_reg8(uint8_t reg);
    void read_reg16(uint8_t reg);
    //uint8_t read_reg8(uint8_t reg, int idx);
    //void read_reg16(uint8_t reg);

    // Hilfsfunktionen für SET_H / SET_L
    void SET_H(uint8_t *pins);
    void SET_L(uint8_t *pins);
};
