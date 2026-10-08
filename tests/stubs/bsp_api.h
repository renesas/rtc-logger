#ifndef BSP_API_H
#define BSP_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This macro defines a variable for saving previous mask value */
#ifndef FSP_CRITICAL_SECTION_DEFINE
 #define FSP_CRITICAL_SECTION_DEFINE        (void)0;
#endif

#ifndef FSP_CRITICAL_SECTION_ENTER
#define FSP_CRITICAL_SECTION_ENTER          (void)0;
#endif

#ifndef FSP_CRITICAL_SECTION_EXIT
#define FSP_CRITICAL_SECTION_EXIT           (void)0;
#endif

typedef enum e_fsp_err
{
    FSP_SUCCESS              = 0,

    FSP_ERR_ASSERTION        = 1,
    FSP_ERR_NOT_OPEN         = 7,
    FSP_ERR_ALREADY_OPEN     = 14,
    FSP_ERR_INVALID_ARGUMENT = 3,
    FSP_ERR_BUFFER_EMPTY     = 36,
    FSP_ERR_OUT_OF_MEMORY    = 9,
    FSP_ERR_INVALID_SIZE          = 23,
} fsp_err_t;

#ifdef __cplusplus
}
#endif

#endif /* BSP_API_H */