#include "Motion_control.h"
#include <driver/ledc.h>  // Für ESP32 PWM-Funktionen

// Kanalzuordnung: 8 PWM-Kanäle
static const ledc_channel_t pwm_channels[4] = {
    LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3
};

static const uint8_t pwm_pins[4] = {
    PWM_CH0_PIN, PWM_CH1_PIN, PWM_CH2_PIN, PWM_CH3_PIN
};

AS5600_soft_IIC_many MC_AS5600;
uint8_t AS5600_SCL[] = { AS5600_0_SCL, AS5600_1_SCL, AS5600_2_SCL, AS5600_3_SCL };
uint8_t AS5600_SDA[] = { AS5600_0_SDA, AS5600_1_SDA, AS5600_2_SDA, AS5600_3_SDA };
#define AS5600_PI 3.1415926535897932384626433832795
#define speed_filter_k 100
float speed_as5600[4] = {0, 0, 0, 0};

int16_t Motor_PWM_value[4] = {0, 0, 0, 0};
Motion_SensorData Motion_sensors[4];

void MC_PULL_ONLINE_init()
{
    ADC_DMA_init();
}
float MC_PULL_stu_raw[4] = {0, 0, 0, 0};
int MC_PULL_stu[4] = {0, 0, 0, 0};
float MC_ONLINE_key_stu_raw[4] = {0, 0, 0, 0};
// 0 – offline, 1 – online (doppelte Mikroschalter-Auslösung), 
// 2 – externe Auslösung, 3 – interne Auslösung
int MC_ONLINE_key_stu[4] = {0, 0, 0, 0};

// Konstanten zur Spannungssteuerung
float PULL_voltage_up = 1.85f;   // Zustand: hoher Druck → rote LED
float PULL_voltage_down = 1.45f; // Zustand: niedriger Druck → blaue LED
#define PULL_VOLTAGE_SEND_MAX 1.7f
// Konstanten zur Steuerung der Mikroschalter-Auslösung
bool Assist_send_filament[4] = {false, false, false, false};
bool pull_state_old = false; // Letzter Auslösezustand — True: nicht ausgelöst, False: Vorschub abgeschlossen
bool is_backing_out = false;
uint64_t Assist_filament_time[4] = {0, 0, 0, 0};
uint64_t Assist_send_time = 1200; // Dauer des Vorschubs, nachdem nur der äußere Auslöser betätigt wurde
// Rückförderstrecke in Millimetern (MM)
float_t P1X_OUT_filament_meters = 200.0f;       // Eingebaut: 200 mm, extern: 700 mm
float_t last_total_distance[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // Initialisierte Entfernung beim Start des Rückzugs
// bool filament_channel_inserted[4]={false,false,false,false}; // Ob der Kanal eingesetzt ist
// Verwendung von doppelten Mikroschaltern
int32_t as5600_distance_save[4] = {0, 0, 0, 0};

#define is_two false

void MC_PULL_ONLINE_read()
{
    //testdata:
    //float test_data[8] = { 1.6, 1.7, 1.6, 1.7, 1.6, 1.7, 1.6, 1.7 }; // optimale Werte
    float test_data[8] = { 1.6, 1.8, 1.6, 1.8, 1.6, 1.8, 1.6, 1.8 }; // testwerte damit Motor laufen kann
    float *data = test_data;
    //float *data = ADC_DMA_get_value();

    MC_PULL_stu_raw[3] = data[0];
    MC_ONLINE_key_stu_raw[3] = data[1];
    MC_PULL_stu_raw[2] = data[2];
    MC_ONLINE_key_stu_raw[2] = data[3];
    MC_PULL_stu_raw[1] = data[4];
    MC_ONLINE_key_stu_raw[1] = data[5];
    MC_PULL_stu_raw[0] = data[6];
    MC_ONLINE_key_stu_raw[0] = data[7];

    for (int i = 0; i < 4; i++)
    {
        if (MC_PULL_stu_raw[i] > PULL_voltage_up) // Größer als 1,85 V → zu hoher Druck
        {
            MC_PULL_stu[i] = 1;
        }
        else if (MC_PULL_stu_raw[i] < PULL_voltage_down) // Kleiner als 1,45 V → zu niedriger Druck
        {
            MC_PULL_stu[i] = -1;
        }
        else // Zwischen 1,4 und 1,7, innerhalb des normalen Fehlers, keine Aktion erforderlich
        {
            MC_PULL_stu[i] = 0;
        }
        /* Online-Status */

        // Überprüfung, ob Verbrauchsmaterial online/aktiv ist
        if (is_two == false)
        {
            // Größer als 1,65 V: Verbrauchsmaterial ist online, hoher Pegel
            if (MC_ONLINE_key_stu_raw[i] > 1.65)
            {
                MC_ONLINE_key_stu[i] = 1;
            }
            else
            {
                MC_ONLINE_key_stu[i] = 0;
            }
        }
        else
        {
            // DEBUG_MY(MC_ONLINE_key_stu_raw);
            // Doppel-Mikroschalter
            if (MC_ONLINE_key_stu_raw[i] < 0.6f)
            { // Wenn kleiner, dann offline
                MC_ONLINE_key_stu[i] = 0;
            }
            else if ((MC_ONLINE_key_stu_raw[i] < 1.7f) & (MC_ONLINE_key_stu_raw[i] > 1.4f))
            { // Nur der äußere Mikroschalter wird ausgelöst, zusätzliche Zuführung erforderlich
                MC_ONLINE_key_stu[i] = 2;
            }
            else if (MC_ONLINE_key_stu_raw[i] > 1.7f)
            { // Beide Mikroschalter gleichzeitig ausgelöst, Online-Status
                MC_ONLINE_key_stu[i] = 1;
            }
            else if (MC_ONLINE_key_stu_raw[i] < 1.4f)
            { // Nur der innere Mikroschalter wird ausgelöst, es muss überprüft werden, ob Material fehlt oder nur ein Wackeln vorliegt
                MC_ONLINE_key_stu[i] = 3;
            }
        }
    }
}

#define PWM_lim 1000

Motion_control_save_struct Motion_control_data_save alignas(4) = {
    {0, 0, 0, 0},  // Motion_control_dir
    0x40614061      // check
};

bool Motion_control_read() {
    Motion_control_save_struct temp;

    // Lese die Daten über den globalen Flash_read (NVS-basiert)
    if (!Flash_read(&temp, sizeof(temp), Motion_control_flash_addr)) {
        printf("Motion_control_read: Fehler beim Lesen Motion_control Flash!");
        return false;
    }

    // Prüfen von Checksum
    if (temp.check == 0x40614061) {
        memcpy(&Motion_control_data_save, &temp, sizeof(Motion_control_save_struct));
        return true;
    }

    printf("Motion_control_read: Motion_control Flash-Daten ungültig!");
    return false;
}

void Motion_control_save()
{
    if (!Flash_saves(&Motion_control_data_save, sizeof(Motion_control_save_struct), Motion_control_flash_addr)) {
        const char msg[] = "Fehler: Motion_control Flash speichern fehlgeschlagen!";
        Debug_log_write(msg);
    }
}

class MOTOR_PID
{

    float P = 0;
    float I = 0;
    float D = 0;
    float I_save = 0;
    float E_last = 0;
    float pid_MAX = PWM_lim;
    float pid_MIN = -PWM_lim;
    float pid_range = (pid_MAX - pid_MIN) / 2;

public:
    MOTOR_PID()
    {
        pid_MAX = PWM_lim;
        pid_MIN = -PWM_lim;
        pid_range = (pid_MAX - pid_MIN) / 2;
    }
    MOTOR_PID(float P_set, float I_set, float D_set)
    {
        init_PID(P_set, I_set, D_set);
        pid_MAX = PWM_lim;
        pid_MIN = -PWM_lim;
        pid_range = (pid_MAX - pid_MIN) / 2;
    }
    void init_PID(float P_set, float I_set, float D_set) // Achtung: Es wird eine eigenständige PID-Berechnungsmethode verwendet,
    {                                                    // I und D sind standardmäßig bereits mit P multipliziert
        P = P_set;
        I = I_set;
        D = D_set;
        I_save = 0;
        E_last = 0;
    }
    float caculate(float E, float time_E)
    {
        I_save += I * E * time_E;
        if (I_save > pid_range) // Begrenzung für I
            I_save = pid_range;
        if (I_save < -pid_range)
            I_save = -pid_range;

        float ouput_buf;
        if (time_E != 0) // Verhindert zu schnelle Aufrufe
            ouput_buf = P * E + I_save + D * (E - E_last) / time_E;
        else
            ouput_buf = P * E + I_save;

        if (ouput_buf > pid_MAX)
            ouput_buf = pid_MAX;
        if (ouput_buf < pid_MIN)
            ouput_buf = pid_MIN;

        E_last = E;
        return ouput_buf;
    }
    void clear()
    {
        I_save = 0;
        E_last = 0;
    }
};

enum class filament_motion_enum
{
    filament_motion_send,
    filament_motion_redetect,
    filament_motion_slow_send,
    filament_motion_pull,
    filament_motion_stop,
    filament_motion_pressure_ctrl_on_use,
    filament_motion_pressure_ctrl_idle,
};
enum class pressure_control_enum
{
    less_pressure,
    all,
    over_pressure
};

class _MOTOR_CONTROL
{
public:
    filament_motion_enum motion = filament_motion_enum::filament_motion_stop;
    int CHx = 0;
    uint64_t motor_stop_time = 0;
    MOTOR_PID PID_speed = MOTOR_PID(2, 20, 0);
    MOTOR_PID PID_pressure = MOTOR_PID(1500, 0, 0);
    float pwm_zero = 500;
    float dir = 0;
    int x1 = 0;
    _MOTOR_CONTROL(int _CHx)
    {
        CHx = _CHx;
        motor_stop_time = 0;
        motion = filament_motion_enum::filament_motion_stop;
    }

    void set_pwm_zero(float _pwm_zero)
    {
        pwm_zero = _pwm_zero;
    }
    void set_motion(filament_motion_enum _motion, uint64_t over_time)
    {
        uint64_t time_now = get_time64();
        motor_stop_time = time_now + over_time;
        if (motion != _motion)
        {
            motion = _motion;
            PID_speed.clear();
        }
    }
    filament_motion_enum get_motion()
    {
        return motion;
    }
    float _get_x_by_pressure(float pressure_voltage, float control_voltage, float time_E, pressure_control_enum control_type)
    {
        float x=0;
        switch (control_type)
        {
        case pressure_control_enum::all: // Steuerung über den gesamten Bereich
        {
            x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - control_voltage, time_E);
            break;
        }
        case pressure_control_enum::less_pressure: // Nur Niederspannungssteuerung
        {
            if (pressure_voltage < control_voltage)
            {
                x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - control_voltage, time_E);
            }
            break;
        }
        case pressure_control_enum::over_pressure: // Nur Hochspannungssteuerung
        {
            if (pressure_voltage > control_voltage)
            {
                x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - control_voltage, time_E);
            }
            break;
        }
        }
        if (x > 0) // Wandelt die Steuerkraft in eine quadratische Verstärkung um, Quadratisierung eliminiert das Vorzeichen, daher muss überprüft werden
            x = x * x / 250;
        else
            x = -x * x / 250;
        return x;
    }
    void run(float time_E)
    {
        // Wenn sich das System im Rückführmodus befindet und Material zurückgeführt werden muss, wird die Laufstrecke zu Beginn aufgezeichnet
        if (is_backing_out){
            last_total_distance[CHx] += fabs(speed_as5600[CHx] * time_E);
        }
        float pid_val = 0;  // jetzt schon sichtbar im gesamten run()
        float speed_set = 0;
        float now_speed = speed_as5600[CHx];
        float x=0;

        uint16_t device_type = get_now_BambuBus_device_type();
        static uint64_t countdownStart[4] = {0};          // Countdown für die Hilfszuführung
        if (motion == filament_motion_enum::filament_motion_pressure_ctrl_idle) // Im Leerlaufzustand
        {
            // Wenn beide Mikroschalter freigegeben sind
            if (MC_ONLINE_key_stu[CHx] == 0)
            {
                Assist_send_filament[CHx] = true; // Die Hilfszuführung kann erst einmal ausgelöst werden, nachdem ein Kanal offline gegangen ist
                countdownStart[CHx] = 0;          // Countdown zurücksetzen
            }

            if (Assist_send_filament[CHx] && is_two)
            { // Zulässiger Status, Versuch der Hilfszuführung
                if (MC_ONLINE_key_stu[CHx] == 2)
                {                   // Äußeren Mikroschalter auslösen
                    x = -dir * 666; // Materialzuführung antreiben
                }
                if (MC_ONLINE_key_stu[CHx] == 1)
                { // Beide Mikroschalter gleichzeitig ausgelöst, Maschine bereit zum Stoppen
                    if (countdownStart[CHx] == 0)
                    { // Countdown starten
                        countdownStart[CHx] = get_time64();
                    }
                    uint64_t now = get_time64();
                    if (now - countdownStart[CHx] >= Assist_send_time) // Countdown
                    {
                        x = 0;                             // Motor stoppen
                        Assist_send_filament[CHx] = false; // Bedingung erfüllt, eine Runde Hilfszuführung abgeschlossen
                    }
                    else
                    {
                        // Materialzuführung antreiben
                        x = -dir * 666;
                    }
                }
            }
            else
            {
                // Bereits ausgelöst, oder Mikroschalter wird in einem anderen Zustand ausgelöst
                if (MC_ONLINE_key_stu[CHx] != 0 && MC_PULL_stu[CHx] != 0)
                { // Wenn der Schlitten manuell gezogen wird, entsprechende Reaktion ausführen
                    x = dir * PID_pressure.caculate(MC_PULL_stu_raw[CHx] - 1.65, time_E);
                }
                else
                { // Andernfalls Maschine gestoppt halten
                    x = 0;
                    PID_pressure.clear();
                }
            }
        }
        else if (MC_ONLINE_key_stu[CHx] != 0) // Kanal befindet sich im Betriebszustand und enthält Verbrauchsmaterial
        {
            if (motion == filament_motion_enum::filament_motion_pressure_ctrl_on_use) // Im Betriebszustand
            {
                if (pull_state_old) { // Beim ersten Eintritt in den Betriebszustand wird kein Rücklauf ausgelöst, das Spülen bringt den Puffer wieder in Ausgangsposition
                    if (MC_PULL_stu_raw[CHx] < 1.55){
                        pull_state_old = false; // Verbrauchsmaterial wird als Niederdruck erkannt
                    }
                } else {
                    if (MC_PULL_stu_raw[CHx] < 1.65)
                    {
                        x = _get_x_by_pressure(MC_PULL_stu_raw[CHx], 1.65, time_E, pressure_control_enum::less_pressure);
                    }
                    else if (MC_PULL_stu_raw[CHx] > 1.7)
                    {
                        x = _get_x_by_pressure(MC_PULL_stu_raw[CHx], 1.7, time_E, pressure_control_enum::over_pressure);
                    }
                }
            }
            else
            {
                if (motion == filament_motion_enum::filament_motion_stop) // Stoppanforderung
                {
                    PID_speed.clear();
                    Motion_control_set_PWM(CHx, 0);
                    return;
                }
                if (motion == filament_motion_enum::filament_motion_send) // Materialzuführung läuft
                {
                    if (device_type == BambuBus_AMS_lite)
                    {
                        if (MC_PULL_stu_raw[CHx] < PULL_VOLTAGE_SEND_MAX) // Druck aktiv bis zu dieser Position
                            speed_set = 30;
                        else
                            speed_set = 0; // Im Original war hier 10
                    }
                    else
                    {
                        speed_set = 50; // P-Anteil gibt volle Leistung
                    }
                }
                if (motion == filament_motion_enum::filament_motion_slow_send) // Langsame Materialzuführung anfordern
                {
                    speed_set = 3;
                }
                if (motion == filament_motion_enum::filament_motion_pull) // Rückzug / Zurückziehen
                {
                    speed_set = -50;
                }
                pid_val = PID_speed.caculate(now_speed - speed_set, time_E);
                x = dir * pid_val;
            }
        }
        else // Während des Betriebs ist das Verbrauchsmaterial aufgebraucht, Motorsteuerung muss gestoppt werden
        {
            x = 0;
        }

        if (x > 10)
            x += pwm_zero;
        else if (x < -10)
            x -= pwm_zero;
        else
            x = 0;

        if (x > PWM_lim)
        {
            x = PWM_lim;
        }
        if (x < -PWM_lim)
        {
            x = -PWM_lim;
        }

        // ===== DEBUG vor PWM setzen =====
        /*char dbg[256];
        sprintf(dbg,
            "CH=%u, motion=%d, dir=%d, now_speed=%.2f, speed_set=%.2f, PID_speed=%.2f, x=%.2f, pwm_zero=%.2f, MC_PULL_stu=%d, MC_ONLINE_key_stu=%d, last_total_distance=%.2f",
            CHx,
            (int)motion,
            (int)dir,
            now_speed,
            speed_set,
            pid_val,
            x,
            pwm_zero,
            MC_PULL_stu[CHx],
            MC_ONLINE_key_stu[CHx],
            last_total_distance[CHx]
        );
        DEBUG_MY(dbg);*/
        // ===== DEBUG Ende =====

        Motion_control_set_PWM(CHx, x);
    }
};
_MOTOR_CONTROL MOTOR_CONTROL[4] = {_MOTOR_CONTROL(0), _MOTOR_CONTROL(1), _MOTOR_CONTROL(2), _MOTOR_CONTROL(3)};

void Motion_control_set_PWM(uint8_t CHx, int PWM)
{

    Motor_PWM_value[CHx] = PWM;

    uint16_t value = constrain(abs(PWM), 0, 1023); // ESP32 10-Bit PWM

    switch (CHx)
    {
    case 0: ledcWrite(MOTOR_PWM_CH0, value); break;
    case 1: ledcWrite(MOTOR_PWM_CH1, value); break;
    case 2: ledcWrite(MOTOR_PWM_CH2, value); break;
    case 3: ledcWrite(MOTOR_PWM_CH3, value); break;
    }
}

void AS5600_distance_updata()// AS5600 auslesen, zugehörige Daten aktualisieren
{
    //DEBUG_MY("AS5600_distance_updata() aufgerufen");
    static uint64_t time_last = 0;
    uint64_t time_now;
    float T;
    do
    {
        time_now = get_time64();
    } while (time_now <= time_last); // T!=0
    T = (float)(time_now - time_last);
    MC_AS5600.updata_angle();
    for (int i = 0; i < 4; i++)
    {

        /*char dbg_status[64];
        sprintf(dbg_status, "AS5600 Kanal %d online=%d", i, MC_AS5600.online[i]);
        DEBUG_MY(dbg_status);*/

        Motion_sensors[i].online = MC_AS5600.online[i];
        Motion_sensors[i].raw_angle = MC_AS5600.raw_angle[i];
        Motion_sensors[i].magnet_stu = MC_AS5600.magnet_stu[i];

        if ((MC_AS5600.online[i] == false))
        {
            as5600_distance_save[i] = 0;
            speed_as5600[i] = 0;
            continue;
        }

        int32_t cir_E = 0;
        int32_t last_distance = as5600_distance_save[i];
        int32_t now_distance = MC_AS5600.raw_angle[i];
        float distance_E;
        if ((now_distance > 3072) && (last_distance <= 1024))
        {
            cir_E = -4096;
        }
        else if ((now_distance <= 1024) && (last_distance > 3072))
        {
            cir_E = 4096;
        }

        distance_E = -(float)(now_distance - last_distance + cir_E) * AS5600_PI * 7.5 / 4096; // D = 7,5 mm, Vorzeichen negativ, weil AS5600 direkt auf den Magneten zeigt
        as5600_distance_save[i] = now_distance;

        float speedx = distance_E / T * 1000;
        // T = speed_filter_k / (T + speed_filter_k);
        speed_as5600[i] = speedx; // * (1 - T) + speed_as5600[i] * T; // mm/s
        add_filament_meters(i, distance_E / 1000);

        // ===== DEBUG-Ausgabe AS5600 =====
        /*char dbg[128];
        snprintf(dbg, sizeof(dbg),
                 "AS5600 CH=%d: raw_angle=%d, last_angle=%d, distance_E=%.2f mm, speed=%.2f mm/s, online=%d",
                 i,
                 now_distance,
                 last_distance,
                 distance_E,
                 speed_as5600[i],
                 MC_AS5600.online[i]);
        DEBUG_MY(dbg);*/
        // ==================================
    }
    time_last = time_now;
}

enum filament_now_position_enum
{
    filament_idle,
    filament_sending_out,
    filament_using,
    filament_pulling_back,
    filament_redetect,
};
int filament_now_position[4];
bool wait = false;

bool Prepare_For_filament_Pull_Back(float_t OUT_filament_meters)
{
    bool wait = false;
    for (int i = 0; i < 4; i++)
    {
        if (filament_now_position[i] == filament_pulling_back)
        {
            // DEBUG_MY("last_total_distance: "); // Debug-Informationen ausgeben
            // Debug_log_write_float(last_total_distance[i], 5);
            if (last_total_distance[i] < OUT_filament_meters)
            {
                // Rückführung durchführen, solange Ziel nicht erreicht ist
                MOTOR_CONTROL[i].set_motion(filament_motion_enum::filament_motion_pull, 100); // Motor zum Rückführen des Materials antreiben
                // Sanfter Lichtwechsel / Übergangseffekt
                float npercent = (last_total_distance[i] / OUT_filament_meters) * 100.0f;
                MC_STU_RGB_set(i, 255 - ((255 / 100) * npercent), 125 - ((125 / 100) * npercent), (255 / 100) * npercent);
                // Rückführung noch nicht abgeschlossen, hat Vorrang
            }
            else
            {
                // Zielabstand erreicht
                is_backing_out = false; // Keine weitere Distanzaufzeichnung erforderlich
                MOTOR_CONTROL[i].set_motion(filament_motion_enum::filament_motion_stop, 100); // Motor stoppen
                filament_now_position[i] = filament_idle;               // Aktuelle Position als frei/Leerlauf festlegen
                set_filament_motion(i, AMS_filament_motion::idle);      // Erzwinge Leerlaufzustand
                last_total_distance[i] = 0;                             // Rückführdistanz zurücksetzen
                // Rückführung abgeschlossen
            }
            // Solange sich das System im Rückführmodus befindet, muss gewartet werden;
            // erst nach dem nächsten Zyklus, wenn nicht mehr im Rückführmodus, ist kein Warten erforderlich
            wait = true;
        }
    }
    return wait;
}
void motor_motion_switch() // Funktion zum Umschalten des Kanalstatus, steuert nur den aktuell verwendeten Kanal; andere Kanäle werden auf Stopp gesetzt
{
    int num = get_now_filament_num();                      // Aktuelle Kanalnummer
    uint16_t device_type = get_now_BambuBus_device_type(); // Gerätetyp
    for (int i = 0; i < 4; i++)
    {
        if (i != num)
        {
            filament_now_position[i] = filament_idle;
            MOTOR_CONTROL[i].set_motion(filament_motion_enum::filament_motion_pressure_ctrl_idle, 1000);
        }
        else if (MC_ONLINE_key_stu[num] == 1 || MC_ONLINE_key_stu[num] == 3) // Kanal enthält Verbrauchsmaterial (Draht/Faden)
        {
            switch (get_filament_motion(num)) // Status des Emulators prüfen
            {
            case AMS_filament_motion::need_send_out: // Materialzuführung erforderlich
                MC_STU_RGB_set(num, 00, 255, 00);
                filament_now_position[num] = filament_sending_out;
                MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_send, 100);
                break;
            case AMS_filament_motion::need_pull_back:
                pull_state_old = false; // Markierung zurücksetzen
                is_backing_out = true; // Kennzeichnet, dass ein Rückzug erfolgt
                filament_now_position[num] = filament_pulling_back;
                if (device_type == BambuBus_AMS_lite)
                {
                    MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_pull, 100);
                }
                // Prepare_For_filament_Pull_Back(OUT_filament_meters); // Über die Distanz steuern, ob die Rückführung abgeschlossen ist
                break;
            case AMS_filament_motion::before_pull_back:
            case AMS_filament_motion::on_use:
            {
                static uint64_t time_end = 0;
                uint64_t time_now = get_time64();
                if (filament_now_position[num] == filament_sending_out) // Wenn der Kanal gerade mit der Materialzuführung beginnt
                {
                    is_backing_out = false; // Einstellen, dass die Distanz nicht aufgezeichnet werden muss
                    pull_state_old = true; // Beim ersten Mal wird kein Rückzug ausgeführt,  
                                            // es wird auf das Auslösen des Niederspannungspegels gewartet,  
                                            // um zu vermeiden, dass das Material direkt nach dem Einzug wieder herausgezogen wird.  
                    filament_now_position[num] = filament_using; // Als „in Benutzung“ markieren
                    time_end = time_now + 1500;                  // Um ein Nicht-Eingreifen zu verhindern, kontinuierliche Zuführung für 1,5 Sekunden
                }
                else if (filament_now_position[num] == filament_using) // Bereits ausgelöst und im Benutzungszustand
                {
                    last_total_distance[i] = 0; // Rückführdistanz zurücksetzen
                    if (time_now > time_end)
                    {                                          // 1,5 Sekunden überschritten, Kanal in Benutzung, Materialnachführung starten
                        MC_STU_RGB_set(num, 255, 255, 255); // Weiß
                        MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_pressure_ctrl_on_use, 20);
                    }
                    else
                    {                                                                  // Wurde das Verbrauchsmaterial gerade erkannt?
                        MC_STU_RGB_set(num, 128, 192, 128);                         // Hellgrün
                        MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_slow_send, 100); // In den ersten 1,5 Sekunden langsame Materialzuführung, Hilfswerkzeug greift zu
                    }
                }
                break;
            }
            case AMS_filament_motion::idle:
                filament_now_position[num] = filament_idle;
                MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_pressure_ctrl_idle, 100);
                for (int i = 0; i < 4; i++)
                {
                    // Hardware in Ordnung
                    if (MC_ONLINE_key_stu[i] == 1 || MC_ONLINE_key_stu[i] == 0)
                    {   // Gleichzeitig ausgelöst oder kein Verbrauchsmaterial vorhanden
                        MC_STU_RGB_set(i, 0, 0, 255); // Blau
                    }
                    else if (MC_ONLINE_key_stu[i] == 2)
                    {   // Nur äußerer Auslöser
                        MC_STU_RGB_set(i, 255, 144, 0); // Nur äußerer Mikroschalter ausgelöst
                    }
                    else if (MC_ONLINE_key_stu[i] == 3)
                    {   // Nur innerer Auslöser
                        MC_STU_RGB_set(i, 0, 255, 255); // Cyan / Türkis
                    }
                }
                break;
            }
        }
        else if (MC_ONLINE_key_stu[num] == 0) 
        // 0: Auf jeden Fall kein Verbrauchsmaterial
        // 1: Bei gleichzeitiger Auslösung ist auf jeden Fall Verbrauchsmaterial vorhanden
        // 2: Nur äußerer Auslöser
        // 3: Nur innerer Auslöser
        // Hier ist eine Funktion zum Schutz gegen Unterbrechung/Offline enthalten
        {
            filament_now_position[num] = filament_idle;
            MOTOR_CONTROL[num].set_motion(filament_motion_enum::filament_motion_pressure_ctrl_idle, 100);
            // MC_STU_RGB_set(num, 0, 0, 255);
        }
    }
}
// Motorsteuerung basierend auf den Informationen des AMS-Emulators
void motor_motion_run(int error)
{
    uint64_t time_now = get_time64();
    static uint64_t time_last = 0;
    float time_E = time_now - time_last; // Zeitdifferenz abrufen (ms)
    time_E = time_E / 1000;              // In Sekunden umrechnen
    uint16_t device_type = get_now_BambuBus_device_type();
    if (!error) // Normalmodus
    {
        // Unterschiedliche Motorsteuerungslogik je nach Gerätetyp ausführen
        if (device_type == BambuBus_AMS_lite)
        {
            motor_motion_switch(); // Motor steuern / Motorbetrieb koordinieren
        }
        else if (device_type == BambuBus_AMS)
        {
            if (!Prepare_For_filament_Pull_Back(P1X_OUT_filament_meters)) // Negieren (gibt true zurück) bedeutet, dass die Rückführung nicht prioritär behandelt werden muss und die Motorsteuerung fortgesetzt wird
            {
                motor_motion_switch(); // Motor steuern / Motorbetrieb koordinieren
            }
        }
    }
    else // Fehler-Modus
    {
        for (int i = 0; i < 4; i++)
            MOTOR_CONTROL[i].set_motion(filament_motion_enum::filament_motion_stop, 100); // Motor ausschalten
    }

    for (int i = 0; i < 4; i++)
    {
        /*if (!get_filament_online(i)) // Wenn der Kanal offline ist, darf der Motor nicht arbeiten
            MOTOR_CONTROL[i].set_motion(filament_motion_stop, 100);*/
        MOTOR_CONTROL[i].run(time_E); // Motor basierend auf Statusinformationen steuern

        if (MC_PULL_stu[i] == 1)
        {
            MC_PULL_ONLINE_RGB_set(i, 255, 0, 0); // Zu hoher Druck, rote Lampe
        }
        else if (MC_PULL_stu[i] == 0)
        { // Normaler Druck
            if (MC_ONLINE_key_stu[i] == 1)
            { // Online und beide Mikroschalter ausgelöst
                int filament_colors_R = channel_colors[i][0];
                int filament_colors_G = channel_colors[i][1];
                int filament_colors_B = channel_colors[i][2];
                // Basierend auf der gespeicherten Farbe des Verbrauchsmaterials
                MC_PULL_ONLINE_RGB_set(i, filament_colors_R, filament_colors_G, filament_colors_B);
                // Weiße Lampe einschalten
                // MC_STU_RGB_set(i, 255, 255, 255);
            }
            else
            {
                MC_PULL_ONLINE_RGB_set(i, 0, 0, 0); // Kein Verbrauchsmaterial, Lampe aus
            }
        }
        else if (MC_PULL_stu[i] == -1)
        {
            MC_PULL_ONLINE_RGB_set(i, 0, 0, 255); // Zu niedriger Druck, blaue Lampe
        }
    }
    time_last = time_now;
}
// Bewegungssteuerungsfunktion
void Motion_control_run(int error)
{
    MC_PULL_ONLINE_read();

    AS5600_distance_updata();
    for (int i = 0; i < 4; i++)
    {
        if (MC_ONLINE_key_stu[i] == 0) {
            set_filament_online(i, false);
        } else if (MC_ONLINE_key_stu[i] == 1) {
            set_filament_online(i, true);
        } else if (MC_ONLINE_key_stu[i] == 3 && filament_now_position[i] == filament_using) {
            // Wenn nur der innere Mikroschalter ausgelöst wird und sich der Kanal im Benutzungszustand befindet, zunächst nicht offline gehen
            set_filament_online(i, true);
        } else if (filament_now_position[i] == filament_redetect || (filament_now_position[i] == filament_pulling_back)) {
            // Wenn sich das System im Rückführmodus oder beim Rückführen befindet, zunächst nicht offline gehen
            set_filament_online(i, true);
        } else {
            set_filament_online(i, false);
        }
    }
    /*
    Wenn der äußere Mikroschalter ausgelöst wird: Orange / Gold
    Wenn nur der innere Mikroschalter ausgelöst wird: Cyan
    Wenn beide gleichzeitig ausgelöst werden: Leerlauf = Blau,  
        gleichzeitig bedeutet dies, dass das Verbrauchsmaterial online ist, Blau + Weiß / gespeicherte Kanal-Farbe
    */

    if (error) // Error != 0
    {
        for (int i = 0; i < 4; i++)
        {
            set_filament_online(i, false);
            // filament_channel_inserted[i] = true; // Zum Testen / Für Testzwecke
            if (MC_ONLINE_key_stu[i] == 1)
            {                                        // Gleichzeitig ausgelöst
                MC_STU_RGB_set(i, 0, 0, 255); // Blau
            }
            else if (MC_ONLINE_key_stu[i] == 2)
            {                                        // Nur äußerer Auslöser
                MC_STU_RGB_set(i, 255, 144, 0); // Orange / Goldfarben
            }
            else if (MC_ONLINE_key_stu[i] == 3)
            {                                        // Nur innerer Auslöser
                MC_STU_RGB_set(i, 0, 255, 255); // Cyan / Türkis
            } else if (MC_ONLINE_key_stu[i] == 0)
            {   // Drucker nicht verbunden und kein Verbrauchsmaterial vorhanden
                MC_STU_RGB_set(i, 0, 0, 0); // Schwarz
            }
        }
    } else { // Normal mit dem Drucker verbunden
        // Wenn hier die Farbe eingestellt wird, erfolgt eine wiederholte Änderung
        for (int i = 0; i < 4; i++)
        {
            if ((MC_AS5600.online[i] == false) || (MC_AS5600.magnet_stu[i] == -1)) // AS5600 error
            {
                set_filament_online(i, false);
                MC_STU_ERROR[i] = true;
            }
        }
    }
    motor_motion_run(error);
}
void MC_PWM_init()
{
    // LEDC-Timer konfigurieren
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode       = LEDC_HIGH_SPEED_MODE;
    ledc_timer.duty_resolution  = (ledc_timer_bit_t)PWM_RES;
    ledc_timer.timer_num        = LEDC_TIMER_0;
    ledc_timer.freq_hz          = PWM_FREQ;
    ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
    ledc_timer_config(&ledc_timer);

    // Alle PWM-Kanäle konfigurieren
    for (int ch = 0; ch < 4; ch++) {
        ledc_channel_config_t ledc_channel = {};
        ledc_channel.channel    = pwm_channels[ch];
        ledc_channel.duty       = 0;
        ledc_channel.gpio_num   = pwm_pins[ch];
        ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
        ledc_channel.hpoint     = 0;
        ledc_channel.timer_sel  = LEDC_TIMER_0;
        ledc_channel_config(&ledc_channel);
    }
}

// PWM-Reibungskraft-Nullpunkt abrufen (veraltet, angenommen 50 % Duty Cycle)
void MOTOR_get_pwm_zero()
{
    float pwm_zero[4] = {0, 0, 0, 0};
    MC_AS5600.updata_angle();

    int16_t last_angle[4];
    for (int index = 0; index < 4; index++)
    {
        last_angle[index] = MC_AS5600.raw_angle[index];
    }
    for (int pwm = 300; pwm < 1000; pwm += 10)
    {
        MC_AS5600.updata_angle();
        for (int index = 0; index < 4; index++)
        {

            if (pwm_zero[index] == 0)
            {
                if (abs(MC_AS5600.raw_angle[index] - last_angle[index]) > 50)
                {
                    pwm_zero[index] = pwm;
                    pwm_zero[index] *= 0.90;
                    Motion_control_set_PWM(index, 0);
                }
                else if ((MC_AS5600.online[index] == true))
                {
                    Motion_control_set_PWM(index, -pwm);
                }
            }
            else
            {
                Motion_control_set_PWM(index, 0);
            }
        }
        delay(100);
    }
    for (int index = 0; index < 4; index++)
    {
        Motion_control_set_PWM(index, 0);
        MOTOR_CONTROL[index].set_pwm_zero(pwm_zero[index]);
    }
}
// Winkelwert in Winkeldifferenz umrechnen
int M5600_angle_dis(int16_t angle1, int16_t angle2)
{

    int cir_E = angle1 - angle2;
    if ((angle1 > 3072) && (angle2 <= 1024))
    {
        cir_E = -4096;
    }
    else if ((angle1 <= 1024) && (angle2 > 3072))
    {
        cir_E = 4096;
    }
    return cir_E;
}

// Bewegungsrichtung des Motors testen
void MOTOR_get_dir()
{
    int dir[4] = {0, 0, 0, 0};
    bool done = false;
    bool have_data = Motion_control_read();
    if (!have_data)
    {
        for (int index = 0; index < 4; index++)
        {
            Motion_control_data_save.Motion_control_dir[index] = 0;
        }
    }
    MC_AS5600.updata_angle(); // Anfangswinkelwert des AS5600 auslesen

    int16_t last_angle[4];
    for (int index = 0; index < 4; index++)
    {
        last_angle[index] = MC_AS5600.raw_angle[index];                  // Anfangswinkelwert speichern
        dir[index] = Motion_control_data_save.Motion_control_dir[index]; // Dir-Daten aus dem Flash speichern
    }
    //bool need_test = false; // Muss überprüft werden?
    bool need_save = false; // Muss der Status aktualisiert werden?
    for (int index = 0; index < 4; index++)
    {
        if ((MC_AS5600.online[index] == true)) // AS5600 vorhanden → Kanal ist online
        {
            if (Motion_control_data_save.Motion_control_dir[index] == 0) // Vorheriges Testergebnis war 0, Test erforderlich
            {
                Motion_control_set_PWM(index, 1000); // Motor einschalten
                //need_test = true;                    // Test erforderlich setzen
                need_save = true;                    // Status wurde aktualisiert
            }
        }
        else
        {
            dir[index] = 0;   // Kanal offline → seine Richtungsdaten löschen
            need_save = true; // Status aktualisiert
        }
    }
    int i = 0;
    while (done == false)
    {
        done = true;

        delay(10);                // Alle 10 ms prüfen
        MC_AS5600.updata_angle(); // Winkeldaten aktualisieren

        if (i++ > 200) // Keine Antwort länger als 2 Sekunden
        {
            for (int index = 0; index < 4; index++)
            {
                Motion_control_set_PWM(index, 0);                       // Stopp
                Motion_control_data_save.Motion_control_dir[index] = 0; // Richtung auf 0 setzen
            }
            break; // Schleife verlassen
        }
        for (int index = 0; index < 4; index++) // Durchlaufen / Iterieren
        {
            if ((MC_AS5600.online[index] == true) && (Motion_control_data_save.Motion_control_dir[index] == 0)) // 对于新的通道
            {
                int angle_dis = M5600_angle_dis(MC_AS5600.raw_angle[index], last_angle[index]);
                if (abs(angle_dis) > 163) // 移动超过1mm
                {
                    Motion_control_set_PWM(index, 0); // 停止
                    if (angle_dis > 0)                // 这里AS600正对着磁铁，和背贴方向是反的
                    {
                        dir[index] = 1;
                    }
                    else
                    {
                        dir[index] = -1;
                    }
                }
                else
                {
                    done = false; // 没有移动。继续等待
                }
            }
        }
    }
    for (int index = 0; index < 4; index++) // 遍历四个电机
    {
        Motion_control_data_save.Motion_control_dir[index] = dir[index]; // 数据复制
    }
    if (need_save) // 如果需要保存数据
    {
        Motion_control_save(); // 数据保存
    }
}
// 初始化电机
// 指定方向
int first_boot = 1; // 1 表示第一次启动，用于执行仅启动时.
void set_motor_directions(int dir0, int dir1, int dir2, int dir3)
{
    Motion_control_data_save.Motion_control_dir[0] = dir0;
    Motion_control_data_save.Motion_control_dir[1] = dir1;
    Motion_control_data_save.Motion_control_dir[2] = dir2;
    Motion_control_data_save.Motion_control_dir[3] = dir3;

    Motion_control_save(); // 保存到闪存
}

void MOTOR_init()
{

    MC_PWM_init();
    MC_AS5600.init(AS5600_SCL, AS5600_SDA, 4);
    // MOTOR_get_pwm_zero();
    // 自动方向
    MOTOR_get_dir();

    // Wird verwendet, um die Richtung der Motoren festzulegen
    if (first_boot == 1)
    { // Erstes Starten
        set_motor_directions(1 , 1 , 1 , 1 ); // 1 für Vorwärtsdrehung, -1 für Rückwärtsdrehung
        first_boot = 0;
    }
    for (int index = 0; index < 4; index++)
    {
        Motion_control_set_PWM(index, 0);
        MOTOR_CONTROL[index].set_pwm_zero(500);
        MOTOR_CONTROL[index].dir = Motion_control_data_save.Motion_control_dir[index];
    }
    MC_AS5600.updata_angle();
    for (int i = 0; i < 4; i++)
    {
        as5600_distance_save[i] = MC_AS5600.raw_angle[i];
    }
}

extern void RGB_update();
void Motion_control_init() // 初始化所有运动和传感器
{
    MC_PULL_ONLINE_init();
    MC_PULL_ONLINE_read();
    MOTOR_init();
    
    /*
    //这是一段阻塞的DEBUG代码
    while (1)
    {
        delay(10);
        MC_PULL_ONLINE_read();

        for (int i = 0; i < 4; i++)
        {
            MOTOR_CONTROL[i].set_motion(filament_motion_pressure_ctrl_on_use, 100);
            if (!get_filament_online(i)) // 通道不在线则电机不允许工作
                MOTOR_CONTROL[i].set_motion(filament_motion_stop, 100);
            MOTOR_CONTROL[i].run(0); // 根据状态信息来驱动电机
        }
        char s[100];
        int n = sprintf(s, "%d", (int)(MC_PULL_stu_raw[3] * 1000));
        DEBUG_num(s, n);
    }*/

    for (int i = 0; i < 4; i++)
    {
        // if(MC_AS5600.online[i])//用AS5600是否有信号来判断通道是否插入
        // {
        //     filament_channel_inserted[i]=true;
        // }
        // else
        // {
        //     filament_channel_inserted[i]=false;
        // }
        filament_now_position[i] = filament_idle;//将通道初始状态设置为空闲
    }
}
