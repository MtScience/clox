#ifndef clox_common_h
#define clox_common_h


#include <stdint.h>  /* For int8_t, int16_t, int32_t, uint8_t, uint16_t, uint32_t, UINT8_MAX, UINT16_MAX */
#include <stddef.h>  /* For unreachable */


/**
 * \defgroup ExitCodes
 * \brief Custom exit codes.
 *
 * Defined as macros in order to stay in line with the standard library, where `EXIT_SUCCESS` and
 * `EXIT_FAILURE` are defined as macros.
 *
 * @{
 */
#define EXIT_WRONG_ARGS    64
#define EXIT_COMPILE_ERROR 65
#define EXIT_RUNTIME_ERROR 70
#define EXIT_FILE_ERROR    74
/**
 * @}
 */


/**
 * \brief Number of values representable with `uint8_t` type.
 */
constexpr uint16_t uint8Count = UINT8_MAX + 1;


#endif
