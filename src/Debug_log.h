#pragma once

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

// Debug-Funktionen
extern void Debug_log_init();
extern uint64_t Debug_log_count64();
extern void Debug_log_time();
extern void Debug_log_write(const void *data);
extern void Debug_log_write_num(const void *data, int num);

// Dummy-Makro für Zeit-Logging
#define DEBUG_time_log() ;

#ifdef Debug_log_on
#define DEBUG_init() Debug_log_init()
#define DEBUG_MY(logs) Debug_log_write(logs)
#define DEBUG_num(logs, num) Debug_log_write_num(logs, num)
#define DEBUG_time() Debug_log_time()
#define DEBUG_get_time() Debug_log_count64()
#else
#define DEBUG_init() ;
#define DEBUG_MY(logs) ;
#define DEBUG_num(logs, num) ;
#define DEBUG_time() ;
#define DEBUG_get_time() 0
#endif

#ifdef __cplusplus
}
#endif