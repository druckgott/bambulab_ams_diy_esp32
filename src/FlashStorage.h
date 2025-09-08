#pragma once

#include "main.h"

extern void Flash_init();
extern bool Flash_saves(void* buf, uint32_t length, uint32_t address);
extern bool Flash_read(void* buf, uint32_t length, uint32_t address);