#pragma once

#include "main.h"

#define FLASH_PAGE_SIZE 4096

extern const char* FLASH_PARTITION_NAME;

extern const esp_partition_t* FLASH_STORAGE_PARTITION;

extern bool Flash_saves(void *buf, uint32_t length, uint32_t address);
extern bool Flash_read(void* buf, uint32_t length, uint32_t address);
extern void Flash_init();