#include "many_soft_AS5600.h"
#include <Arduino.h>

// address
#define AS5600_write_address (0x36 << 1)
#define AS5600_read_address  ((0x36 << 1) + 1)
// angle
#define AS5600_raw_angle     0x0C
#define AS5600_angle         0x0E

// status reg
#define AS5600_status        0x0B
#define AS5600_agc           0x1A
#define AS5600_magnitude     0x1B
#define AS5600_burn          0xFF

#define I2C_DELAY() delayMicroseconds(10)

AS5600_soft_IIC_many::AS5600_soft_IIC_many() {
    numbers = 0;
}

AS5600_soft_IIC_many::~AS5600_soft_IIC_many() {
    if (numbers > 0) {
        delete[] IO_SDA;
        delete[] IO_SCL;
        delete[] error;
        delete[] raw_angle;
        delete[] data;
        delete[] online;
        delete[] magnet_stu;
    }
}

void AS5600_soft_IIC_many::init(uint8_t *GPIO_SCL, uint8_t *GPIO_SDA, int num) {
    numbers = num;

    IO_SCL = new uint8_t[numbers];
    IO_SDA = new uint8_t[numbers];
    error  = new int[numbers];
    raw_angle = new uint16_t[numbers];
    data = new uint16_t[numbers];
    online = new bool[numbers];
    magnet_stu = new _AS5600_magnet_stu[numbers];

    for (int i = 0; i < numbers; i++) {
        IO_SCL[i] = GPIO_SCL[i];
        IO_SDA[i] = GPIO_SDA[i];
        error[i] = 0;
        raw_angle[i] = 0;
        data[i] = 0;
        online[i] = false;
        magnet_stu[i] = offline;
    }

    init_iic();
    updata_stu();
}

void AS5600_soft_IIC_many::read_reg8(uint8_t reg)
{
    if (!numbers)
        return;
    clear_datas();
    start_iic(AS5600_write_address);
    write_iic(reg);
    start_iic(AS5600_read_address);
    read_iic(false);
}

void AS5600_soft_IIC_many::read_reg16(uint8_t reg)
{
    if (!numbers)
        return;
    clear_datas();
    start_iic(AS5600_write_address);
    write_iic(reg);
    start_iic(AS5600_read_address);
    read_iic(true);
    read_iic(false);
}

void AS5600_soft_IIC_many::clear_datas()
{
    for (int i = 0; i < numbers; i++)
    {
        error[i] = 0;
        data[i] = 0;
    }
}

void AS5600_soft_IIC_many::updata_stu()
{
    read_reg8(AS5600_status);
    for (int i = 0; i < numbers; i++)
    {
        if (error[i])
            online[i] = false;
        else
            online[i] = true;
        if (!(data[i] & 0x20))
        {
            magnet_stu[i] = offline;
        }
        else
        {
            if (data[i] & 0x10)
                magnet_stu[i] = low;
            else if (data[i] & 0x08)
                magnet_stu[i] = high;
            else
                magnet_stu[i] = normal;
        }
    }
}

void AS5600_soft_IIC_many::updata_angle()
{
    read_reg16(AS5600_raw_angle);
    for (auto i = 0; i < numbers; i++)
    {
        if (error[i] == 0)
        {
            raw_angle[i] = data[i];
            online[i] = true;
        }
        else
        {
            raw_angle[i] = 0;
            online[i] = false;
        }
    }
}

void AS5600_soft_IIC_many::init_iic() {
    for (int i = 0; i < numbers; i++) {
        pinMode(IO_SCL[i], OUTPUT);
        pinMode(IO_SDA[i], OUTPUT); // Open-drain auf ESP32 simuliert man mit OUTPUT + digitalWrite LOW für 0
        digitalWrite(IO_SCL[i], HIGH);
        digitalWrite(IO_SDA[i], HIGH);
        error[i] = 0;
    }
}

void AS5600_soft_IIC_many::SET_H(uint8_t *pins) {
    for (int i = 0; i < numbers; i++) {
        if (!error[i]) digitalWrite(pins[i], HIGH);
    }
}

void AS5600_soft_IIC_many::SET_L(uint8_t *pins) {
    for (int i = 0; i < numbers; i++) {
        if (!error[i]) digitalWrite(pins[i], LOW);
    }
}

void AS5600_soft_IIC_many::start_iic(uint8_t ADR) {
    I2C_DELAY();
    SET_H(IO_SDA);
    SET_H(IO_SCL);
    I2C_DELAY();
    SET_L(IO_SDA);
    I2C_DELAY();
    SET_L(IO_SCL);
    write_iic(ADR);
}

void AS5600_soft_IIC_many::stop_iic() {
    SET_L(IO_SCL);
    SET_L(IO_SDA);
    I2C_DELAY();
    SET_H(IO_SCL);
    I2C_DELAY();
    SET_H(IO_SDA);
    I2C_DELAY();
}

void AS5600_soft_IIC_many::write_iic(uint8_t byte) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
        I2C_DELAY();
        if (byte & mask) SET_H(IO_SDA);
        else SET_L(IO_SDA);
        SET_H(IO_SCL);
        I2C_DELAY();
        SET_L(IO_SCL);
    }
    wait_ack_iic();
}

void AS5600_soft_IIC_many::read_iic(bool ack) {
    for (int i = 0; i < numbers; i++) pinMode(IO_SDA[i], INPUT_PULLUP);

    for (int bit = 0; bit < 8; bit++) {
        I2C_DELAY();
        SET_H(IO_SCL);
        I2C_DELAY();
        for (int i = 0; i < numbers; i++) {
            data[i] <<= 1;
            if (digitalRead(IO_SDA[i])) data[i] |= 0x01;
        }
        SET_L(IO_SCL);
    }

    for (int i = 0; i < numbers; i++) pinMode(IO_SDA[i], OUTPUT);
    if (ack) SET_L(IO_SDA);
    else SET_H(IO_SDA);
    I2C_DELAY();
    SET_H(IO_SCL);
    I2C_DELAY();
    SET_L(IO_SCL);
    I2C_DELAY();
}

void AS5600_soft_IIC_many::wait_ack_iic() {
    for (int i = 0; i < numbers; i++) pinMode(IO_SDA[i], INPUT_PULLUP);
    I2C_DELAY();
    SET_H(IO_SCL);
    I2C_DELAY();
    for (int i = 0; i < numbers; i++) {
        if (digitalRead(IO_SDA[i])) error[i] = 1;
    }
    for (int i = 0; i < numbers; i++) pinMode(IO_SDA[i], OUTPUT);
    SET_L(IO_SCL);
}

// read/write registers bleiben unverändert, da sie nur write_iic/read_iic aufrufen
