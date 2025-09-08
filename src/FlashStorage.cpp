#include "FlashStorage.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_partition.h"
#include <string.h>
#include <vector>

const char* FLASH_PARTITION_NAME = "storage";
const esp_partition_t* FLASH_STORAGE_PARTITION = nullptr;

void Flash_init() {
    FLASH_STORAGE_PARTITION = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        FLASH_PARTITION_NAME
    );

    if (!FLASH_STORAGE_PARTITION) {
        printf("Fehler: Partition '%s' nicht gefunden!\n", FLASH_PARTITION_NAME);
    } else {
        printf("Storage Partition gefunden: addr=0x%X, size=%u\n",
               FLASH_STORAGE_PARTITION->address, FLASH_STORAGE_PARTITION->size);
    }
}

bool Flash_saves(void *buf, uint32_t length, uint32_t address)
{
    if(FLASH_STORAGE_PARTITION) {
        printf("if partition: Storage Partition gefunden: addr=0x%X, size=%u\n", FLASH_STORAGE_PARTITION->address, FLASH_STORAGE_PARTITION->size);
    } else {
        printf("if partition: Partition '%s' nicht gefunden!\n", FLASH_PARTITION_NAME);
    }

    if (FLASH_STORAGE_PARTITION == NULL) {
        printf("Partition '%s' nicht gefunden!\n", FLASH_PARTITION_NAME);
        return false;
    }

    if (address + length > FLASH_STORAGE_PARTITION->size) {
        printf("Flash_saves: Bereich zu groß (addr=0x%X len=%u, size=%u)\n",
               address, length, FLASH_STORAGE_PARTITION->size);
        return false;
    }

    uint32_t erase_size = (length + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1);

    esp_err_t err = esp_partition_erase_range(FLASH_STORAGE_PARTITION, address, erase_size);
    if (err != ESP_OK) {
        printf("Flash_saves: Erase Fehler %d\n", err);
        return false;
    }

    err = esp_partition_write(FLASH_STORAGE_PARTITION, address, buf, length);
    if (err != ESP_OK) {
        printf("Flash_saves: Write Fehler %d\n", err);
        return false;
    }

    std::vector<uint8_t> verify(length);
    err = esp_partition_read(FLASH_STORAGE_PARTITION, address, verify.data(), length);
    if (err != ESP_OK) {
        printf("Flash_saves: Read Fehler %d\n", err);
        return false;
    }

    if (memcmp(buf, verify.data(), length) != 0) {
        printf("Flash_saves: Verify Fehler!\n");
        return false;
    }

    return true;
}

bool Flash_read(void* buf, uint32_t length, uint32_t address) {
    if (!FLASH_STORAGE_PARTITION) {
        printf("Flash_read: Partition nicht initialisiert!\n");
        return false;
    }

    if (address + length > FLASH_STORAGE_PARTITION->size) {
        printf("Flash_read: Bereich zu groß!\n");
        return false;
    }

    esp_err_t err = esp_partition_read(FLASH_STORAGE_PARTITION, address, buf, length);
    if (err != ESP_OK) {
        printf("Flash_read: Fehler %d\n", err);
        return false;
    }

    return true;
}