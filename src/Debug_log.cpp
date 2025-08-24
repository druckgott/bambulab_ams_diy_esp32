#include "Debug_log.h"
#include <Arduino.h>

#ifdef Debug_log_on

// Wähle UART-Port und Pins
#define DEBUG_UART_PORT 1         // 0 = Serial, 1 = Serial1, 2 = Serial2
#define DEBUG_TX_PIN    17        // TX GPIO
#define DEBUG_RX_PIN    16        // RX GPIO (optional)
#define DEBUG_BAUDRATE  115200

uint32_t stack[1000];  // optional, wie im Original

void Debug_log_init()
{
    // Serial1 initialisieren (8 Datenbits + gerade Parität + 1 Stopbit)
    Serial1.begin(DEBUG_BAUDRATE, SERIAL_8E1, DEBUG_RX_PIN, DEBUG_TX_PIN);

    // Debug-Nachricht
    Serial1.println("Debug UART ready");
}

uint64_t Debug_log_count64()
{
    // Optional: einfach 0, wie im Original
    return 0;
}

void Debug_log_time()
{
    // Optional: leer lassen oder millis() nutzen
}

void Debug_log_write(const void *data)
{
    const char *str = (const char *)data;
    Serial1.write(str);
}

void Debug_log_write_num(const void *data, int num)
{
    const char *str = (const char *)data;
    Serial1.write((const uint8_t *)str, num);
}

#endif