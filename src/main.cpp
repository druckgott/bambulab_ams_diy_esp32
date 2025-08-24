#include <Arduino.h>
#include "main.h"
#include "pins.h"

#include "BambuBus.h"
#include "Adafruit_NeoPixel.h"

extern void debug_send_run();
// 测试用 8灯珠
#define LED_NUM 2

// 通道RGB对象，strip_channel[Chx]，0~4为PA11/PA8/PB1/PB0
Adafruit_NeoPixel strip_channel[4] = {
    Adafruit_NeoPixel(LED_NUM, PWM0_PIN, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_NUM, PWM1_PIN, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_NUM, PWM2_PIN, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_NUM, PWM3_PIN, NEO_GRB + NEO_KHZ800)
};
// 主板 5050 RGB
Adafruit_NeoPixel strip_PD1(LED_NUM, PWM6_PIN, NEO_GRB + NEO_KHZ800);

void RGB_Set_Brightness() {
    // 亮度值 0-255
    // 主板亮度
    strip_PD1.setBrightness(35);
    // 通道1 RGB
    strip_channel[0].setBrightness(15);
    // 通道2 RGB
    strip_channel[1].setBrightness(15);
    // 通道3 RGB
    strip_channel[2].setBrightness(15);
    // 通道4 RGB
    strip_channel[3].setBrightness(15);
}

void RGB_init() {
    strip_PD1.begin();
    strip_channel[0].begin();
    strip_channel[1].begin();
    strip_channel[2].begin();
    strip_channel[3].begin();
}
void RGB_show_data() {
    strip_PD1.show();
    strip_channel[0].show();
    strip_channel[1].show();
    strip_channel[2].show();
    strip_channel[3].show();
}

// 存储4个通道的耗材丝RGB颜色
uint8_t channel_colors[4][4] = {
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}
};

// 存储4个通道的RGB颜色，避免频繁刷新颜色导致通讯失败
uint8_t channel_runs_colors[4][2][3] = {
    // R,G,B  ,, R,G,B
    {{1, 2, 3}, {1, 2, 3}}, // 通道1
    {{3, 2, 1}, {3, 2, 1}}, // 通道2
    {{1, 2, 3}, {1, 2, 3}}, // 通道3
    {{3, 2, 1}, {3, 2, 1}}  // 通道4
};

extern void BambuBUS_UART_Init();
extern void send_uart(const unsigned char *data, uint16_t length);

void setup()
{
    // 初始化RGB灯
    RGB_init();
    // 更新RGB显示
    RGB_show_data();
    // 设定RGB亮度 这意味着会保持颜色的比例同时限制最大值。
    RGB_Set_Brightness();

    BambuBus_init();
    DEBUG_init();
    Motion_control_init();
    delay(1);
}

void Set_MC_RGB(uint8_t channel, int num, uint8_t R, uint8_t G, uint8_t B)
{
    int set_colors[3] = {R, G, B};
    bool is_new_colors = false;

    for (int colors = 0; colors < 3; colors++)
    {
        if (channel_runs_colors[channel][num][colors] != set_colors[colors]) {
            channel_runs_colors[channel][num][colors] = set_colors[colors]; // 记录新颜色
            is_new_colors = true; // 颜色有更新
        }
    }
    // 检查每个通道，如果有改变，更新它。
    if (is_new_colors) {
        strip_channel[channel].setPixelColor(num, strip_channel[channel].Color(R, G, B));
        strip_channel[channel].show(); // 显示新颜色
        is_new_colors = false; // 重置状态
    }
}

bool MC_STU_ERROR[4] = {false, false, false, false};
void Show_SYS_RGB(int BambuBUS_status)
{
    // 更新主板RGB灯
    if (BambuBUS_status == -1) // 离线
    {
        strip_PD1.setPixelColor(0, strip_PD1.Color(8, 0, 0)); // 红色
        strip_PD1.show();
    }
    else if (BambuBUS_status == 0) // 在线
    {
        strip_PD1.setPixelColor(0, strip_PD1.Color(8, 9, 9)); // 白色
        strip_PD1.show();
    }
    // 更新错误通道，亮起红灯
    for (int i = 0; i < 4; i++)
    {
        if (MC_STU_ERROR[i])
        {
            // 红色
            strip_channel[i].setPixelColor(0, strip_channel[i].Color(255, 0, 0));
            strip_channel[i].show(); // 显示新颜色
        }
    }
}

BambuBus_package_type is_first_run = BambuBus_package_type::NONE;
void loop()
{
    while (1)
    {
        BambuBus_package_type stu = BambuBus_run();
        // int stu =-1;
        static int error = 0;
        bool motion_can_run = false;
        uint16_t device_type = get_now_BambuBus_device_type();
        if (stu != BambuBus_package_type::NONE) // have data/offline
        {
            motion_can_run = true;
            if (stu == BambuBus_package_type::ERROR) // offline
            {
                error = -1;
                // 离线-红色灯
            }
            else // have data
            {
                error = 0;
                // if (stu == BambuBus_package_type::heartbeat)
                // {
                // 正常工作-白色灯
                // }
            }
            // 每隔3秒刷新灯珠
            static unsigned long last_sys_rgb_time = 0;
            unsigned long now = get_time64();
            if (now - last_sys_rgb_time >= 3000) {
                Show_SYS_RGB(error);
                last_sys_rgb_time = now;
            }
        }
        else
        {
        } // wait for data
        // log 输出
        if (is_first_run != stu)
        {
            is_first_run = stu;
            if (stu == BambuBus_package_type::ERROR)
            {                                   // offline
                DEBUG_MY("BambuBus_offline\n"); // 离线
            }
            else if (stu == BambuBus_package_type::heartbeat)
            {
                DEBUG_MY("BambuBus_online\n"); // 在线
            }
            else if (device_type == BambuBus_AMS_lite)
            {
                DEBUG_MY("Run_To_AMS_lite\n"); // 在线
            }
            else if (device_type == BambuBus_AMS)
            {
                DEBUG_MY("Run_To_AMS\n"); // 在线
            }
            else
            {
                DEBUG_MY("Running Unknown ???\n");
            }
        }

        if (motion_can_run)
        {
            Motion_control_run(error);
        }
    }
}
