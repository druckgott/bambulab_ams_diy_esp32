#include <Arduino.h>
#include "main.h"

#include "BambuBus.h"
#include "Adafruit_NeoPixel.h"

extern void debug_send_run();

// RGB-Objekte für die Kanäle, strip_channel[Chx], 0–4 sind PA11/PA8/PB1/PB0
Adafruit_NeoPixel strip_channel[4] = {
    Adafruit_NeoPixel(LED_NUM, PWM0_PIN_LED, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_NUM, PWM1_PIN_LED, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_NUM, PWM2_PIN_LED, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(LED_NUM, PWM3_PIN_LED, NEO_GRB + NEO_KHZ800)
};
// Hauptplatine 5050 RGB
Adafruit_NeoPixel strip_PD1(LED_NUM, PWM6_PIN_LED, NEO_GRB + NEO_KHZ800);

void RGB_Set_Brightness() {
    // Helligkeitswert 0–255
    // Helligkeit Hauptplatine
    strip_PD1.setBrightness(35);
    // Kanal 1 RGB
    strip_channel[0].setBrightness(15);
    // Kanal 2 RGB
    strip_channel[1].setBrightness(15);
    // Kanal 3 RGB
    strip_channel[2].setBrightness(15);
    // Kanal 4 RGB
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

// Speichert die RGB-Farben des Filaments für 4 Kanäle
uint8_t channel_colors[4][4] = {
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}
};

// Speichert die RGB-Farben für 4 Kanäle, um Kommunikationsfehler durch zu häufiges Aktualisiere
uint8_t channel_runs_colors[4][2][3] = {
    // R,G,B  ,, R,G,B
    {{1, 2, 3}, {1, 2, 3}}, // Kanal 1
    {{3, 2, 1}, {3, 2, 1}}, // Kanal 2
    {{1, 2, 3}, {1, 2, 3}}, // Kanal 3
    {{3, 2, 1}, {3, 2, 1}}  // Kanal 4
};

extern void BambuBUS_UART_Init();
extern void send_uart(const unsigned char *data, uint16_t length);

void setup()
{
    //ota Webserial init
    init_ota_webserial();
    delay(10000); //for startup of webserial and have time to connect
    DEBUG_init();
    
    // RGB-LEDs initialisieren
    RGB_init();
    // RGB-Anzeige aktualisieren
    RGB_show_data();
    // RGB-Helligkeit einstellen. Das bedeutet, die Farbverhältnisse bleiben erhalten, aber der Maximalwert wird begrenzt.
    RGB_Set_Brightness();

    Flash_init();
    BambuBus_init();
    
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
            channel_runs_colors[channel][num][colors] = set_colors[colors]; // Neue Farbe speichern
            is_new_colors = true; // Farbe wurde geändert
        }
    }
    // Jeden Kanal prüfen, wenn sich etwas geändert hat, aktualisieren.
    if (is_new_colors) {
        strip_channel[channel].setPixelColor(num, strip_channel[channel].Color(R, G, B));
        strip_channel[channel].show(); // Neue Farbe anzeigen
        is_new_colors = false;
    }
}

bool MC_STU_ERROR[4] = {false, false, false, false};
void Show_SYS_RGB(int BambuBUS_status)
{
    // RGB-LED der Hauptplatine aktualisieren
    if (BambuBUS_status == -1) // Offline
    {
        strip_PD1.setPixelColor(0, strip_PD1.Color(8, 0, 0)); // Rot
        strip_PD1.show();
    }
    else if (BambuBUS_status == 0) // Online
    {
        strip_PD1.setPixelColor(0, strip_PD1.Color(8, 9, 9)); // Weiß
        strip_PD1.show();
    }
    // Fehlerhafte Kanäle aktualisieren → rote LED einschalte
    for (int i = 0; i < 4; i++)
    {
        if (MC_STU_ERROR[i])
        {
            // Rot
            strip_channel[i].setPixelColor(0, strip_channel[i].Color(255, 0, 0));
            strip_channel[i].show(); // Neue Farbe anzeigen
        }
    }
}

/*
unsigned long last_debug_time = 0; // für 1‑Sekunden-Ticker
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
        if (stu != BambuBus_package_type::NONE) // Daten vorhanden / offline
        {
            motion_can_run = true;
            if (stu == BambuBus_package_type::ERROR) // Offline
            {
                error = -1;
                // Offline → rote LED
            }
            else // Daten vorhanden
            {
                error = 0;
                // if (stu == BambuBus_package_type::heartbeat)
                // {
                // Normaler Betrieb → weiße LED
                // }
            }
            // LEDs alle 3 Sekunden aktualisieren
            static unsigned long last_sys_rgb_time = 0;
            unsigned long now = get_time64();
            if (now - last_sys_rgb_time >= 3000) {
                Show_SYS_RGB(error);
                last_sys_rgb_time = now;
            }
        }
        else
        {
        } // Auf Daten warten
        // Log-Ausgabe
        if (is_first_run != stu)
        {
            is_first_run = stu;
            if (stu == BambuBus_package_type::ERROR)
            {
                DEBUG_MY("BambuBus_error"); // error
            }
            else if (stu == BambuBus_package_type::heartbeat)
            {
                DEBUG_MY("BambuBus_online"); // Online
            }
            else if (device_type == BambuBus_AMS_lite)
            {
                DEBUG_MY("Run_To_AMS_lite"); // Online AMS lite
            }
            else if (device_type == BambuBus_AMS)
            {
                DEBUG_MY("Run_To_AMS"); // Online AMS
            }
            else
            {
                DEBUG_MY("Running Unknown ???");
            }
        }

        if (motion_can_run)
        {
            Motion_control_run(error);
        }

        // **Neues: 1‑Sekunden-Ticker für WebSerial**
        unsigned long now_millis = millis();
        if (now_millis - last_debug_time >= 1000) {
            last_debug_time = now_millis;
            DEBUG_MY("Debug: WebSerial alive");
        }
    }
}*/

BambuBus_package_type is_first_run = BambuBus_package_type::NONE;
unsigned long last_sys_rgb_time = 0;    // 3‑Sekunden-Ticker für LEDs

void loop()
{
    // BambuBus ausführen
    BambuBus_package_type stu = BambuBus_run();
    static int error = 0;
    bool motion_can_run = false;
    uint16_t device_type = get_now_BambuBus_device_type();

    if (stu != BambuBus_package_type::NONE) // Daten vorhanden / offline
    {
        motion_can_run = true;
        if (stu == BambuBus_package_type::ERROR) // Offline
        {
            error = -1;
            // Offline → rote LED
        }
        else // Daten vorhanden
        {
            error = 0;
            // Normaler Betrieb → weiße LED
        }

        // LEDs alle 3 Sekunden aktualisieren
        unsigned long now = millis();
        if (now - last_sys_rgb_time >= 3000) {
            Show_SYS_RGB(error);
            last_sys_rgb_time = now;
        }
    }

    // Log-Ausgabe, wenn Pakettyp sich ändert
    if (is_first_run != stu)
    {
        is_first_run = stu;
        if (stu == BambuBus_package_type::ERROR)
        {
            DEBUG_MY("BambuBus_error"); // error
        }
        else if (stu == BambuBus_package_type::heartbeat)
        {
            DEBUG_MY("BambuBus_online"); // Online
        }
        else if (device_type == BambuBus_AMS_lite)
        {
            DEBUG_MY("Run_To_AMS_lite"); // Online AMS lite
        }
        else if (device_type == BambuBus_AMS)
        {
            DEBUG_MY("Run_To_AMS"); // Online AMS
        }
        else
        {
            DEBUG_MY("Running Unknown ???");
        }
    }

    // Motion ausführen
    if (motion_can_run)
    {
        Motion_control_run(error);
    }

    // Kurze Pause, damit AsyncServer & WebSerial arbeiten können
    delay(1);
}
