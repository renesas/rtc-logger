#ifndef R_UART_API_H
#define R_UART_API_H

#ifndef R_FAKE_UART_API_H
#define R_FAKE_UART_API_H
#endif 

#include <stdint.h>
#include "r_byteq.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum e_uart_event
{
    UART_EVENT_RX_CHAR       = 1,
    UART_EVENT_TX_DATA_EMPTY = 2,
    UART_EVENT_ERR_OVERFLOW  = 3,
} uart_event_t;

typedef struct st_uart_callback_args
{
    uart_event_t event;
    uint32_t     data;
} uart_callback_args_t;

typedef void (* uart_callback_t)(uart_callback_args_t * p_args);

typedef struct st_uart_cfg
{
    uart_callback_t p_callback;
} uart_cfg_t;

typedef struct st_uart_api
{
    fsp_err_t (* open)(void * p_ctrl,
                       uart_cfg_t const * p_cfg);

    fsp_err_t (* write)(void * p_ctrl,
                        uint8_t const * p_src,
                        uint32_t bytes);

    fsp_err_t (* callbackSet)(void                * p_ctrl,
                              uart_callback_t       p_callback,
                              void                * p_context,
                              uart_callback_args_t * p_callback_memory);
} uart_api_t;

typedef struct st_uart_instance
{
    void             * p_ctrl;
    uart_cfg_t const * p_cfg;
    uart_api_t const * p_api;
} uart_instance_t;

#ifdef __cplusplus
}
#endif

#endif