#include "FlashStorage.h"
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

Preferences preferences;  // globales Preferences-Objekt

void Flash_init() {
    // Namespace "storage" wird automatisch angelegt, read/write = false/true
    if (!preferences.begin("storage", false)) {
        printf("Fehler: Preferences-Storage konnte nicht geöffnet werden!");
    } else {
        printf("Preferences-Storage geöffnet.");
    }
}

bool Flash_saves(void *buf, uint32_t length, uint32_t address) {
    // address als Key (analog zu altem Ansatz)
    char key[16];
    snprintf(key, sizeof(key), "%u", address);

    if (!preferences.putBytes(key, buf, length)) {
        printf("Flash_saves: Fehler beim Schreiben des Preferences-Keys %s", key);
        return false;
    }

    // Optional: direktes Lesen zur Verifikation
    uint8_t verify[length];
    size_t read_bytes = preferences.getBytes(key, verify, length);
    if (read_bytes != length || memcmp(buf, verify, length) != 0) {
        printf("Flash_saves: Verify Fehler für Key %s", key);
        return false;
    }

    return true;
}

bool Flash_read(void* buf, uint32_t length, uint32_t address) {
    char key[16];
    snprintf(key, sizeof(key), "%u", address);

    size_t read_bytes = preferences.getBytes(key, buf, length);
    if (read_bytes != length) {
        printf("Flash_read: Fehler beim Lesen des Preferences-Keys %s (gelesen %u statt %u)",
               key, read_bytes, length);
        return false;
    }

    return true;
}
