#ifndef clox_common_h
#define clox_common_h


#include <stdint.h>  /* For int8_t, int16_t, int32_t, uint8_t, uint16_t, uint32_t, UINT8_MAX, UINT16_MAX */
#include <stddef.h>  /* For unreachable */


#define DEBUG_TRACE_EXECUTION
#define DEBUG_PRINT_CODE


constexpr uint16_t uint8Count = UINT8_MAX + 1;


#endif
