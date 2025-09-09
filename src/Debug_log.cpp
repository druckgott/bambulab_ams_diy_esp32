#include "Debug_log.h"
#include <Arduino.h>

#ifdef Debug_log_on
#define DEBUG_BAUDRATE  Debug_log_baudrate

uint32_t stack[1000];  // optional, wie im Original

void Debug_log_init()
{
    // Serial1 initialisieren (8 Datenbits + gerade Parität + 1 Stopbit)
    //Serial.begin(DEBUG_BAUDRATE, SERIAL_8E1, DEBUG_RX_PIN, DEBUG_TX_PIN);
    Serial.begin(DEBUG_BAUDRATE, SERIAL_8E1);
    // Debug-Nachricht
    Serial.println("Debug UART ready");
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
    Serial.write(str);
    webSerial.println(str);
}

void Debug_log_write_num(const void *data, int num)
{
    const char *str = (const char *)data;
    Serial.write((const uint8_t *)str, num);

    // WebSerial
    char buffer[num + 1];        // temporärer String, +1 für null terminator
    memcpy(buffer, str, num);
    buffer[num] = '\0';          // null-terminieren
    webSerial.println(buffer);

}

void Debug_log_write_bin(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        char buf[4];
        sprintf(buf, "%02X ", data[i]); // Hex-Darstellung
        webSerial.print(buf);           // WebSerial ausgeben
        Serial.print(buf);              // optional für Hardware-Serial
    }
    webSerial.println(); // Zeilenumbruch nach einem Paket
    Serial.println();
}

#endif