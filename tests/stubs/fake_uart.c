#include "fake_uart.h"

#include <stddef.h>
#include <string.h>

static uint8_t g_fake_uart_tx_capture[FAKE_UART_TX_CAPTURE_SIZE];
static uint32_t g_fake_uart_tx_capture_len;

static uint32_t g_fake_uart_open_call_count;
static uint32_t g_fake_uart_write_call_count;
static uint32_t g_fake_uart_callback_set_call_count;

static fsp_err_t g_fake_uart_open_result;
static fsp_err_t g_fake_uart_write_result;
static fsp_err_t g_fake_uart_callback_set_result;

static uart_callback_t g_fake_registered_callback;

static int g_fake_uart_ctrl_object;

static fsp_err_t fake_uart_open(void * p_ctrl,
                                uart_cfg_t const * p_cfg);

static fsp_err_t fake_uart_write(void * p_ctrl,
                                 uint8_t const * p_src,
                                 uint32_t bytes);

static fsp_err_t fake_uart_callback_set(void                * p_ctrl,
                                        uart_callback_t       p_callback,
                                        void                * p_context,
                                        uart_callback_args_t * p_callback_memory);

static uart_api_t g_fake_uart_api =
{
    .open        = fake_uart_open,
    .write       = fake_uart_write,
    .callbackSet = fake_uart_callback_set,
};

static uart_cfg_t g_fake_uart_cfg =
{
    .p_callback = NULL,
};

uart_instance_t g_uart_jlob_vcom_fake =
{
    .p_ctrl = &g_fake_uart_ctrl_object,
    .p_cfg  = &g_fake_uart_cfg,
    .p_api  = &g_fake_uart_api,
};

static fsp_err_t fake_uart_open(void * p_ctrl,
                                uart_cfg_t const * p_cfg)
{
    if ((NULL == p_ctrl) || (NULL == p_cfg))
    {
        return FSP_ERR_ASSERTION;
    }

    g_fake_uart_open_call_count++;

    return g_fake_uart_open_result;
}

static fsp_err_t fake_uart_write(void * p_ctrl,
                                 uint8_t const * p_src,
                                 uint32_t bytes)
{
    if ((NULL == p_ctrl) || (NULL == p_src))
    {
        return FSP_ERR_ASSERTION;
    }

    g_fake_uart_write_call_count++;

    if (FSP_SUCCESS != g_fake_uart_write_result)
    {
        return g_fake_uart_write_result;
    }

    if (bytes > FAKE_UART_TX_CAPTURE_SIZE)
    {
        return FSP_ERR_INVALID_SIZE;
    }

    memcpy(g_fake_uart_tx_capture, p_src, bytes);
    g_fake_uart_tx_capture_len = bytes;

    return FSP_SUCCESS;
}

static fsp_err_t fake_uart_callback_set(void                * p_ctrl,
                                        uart_callback_t       p_callback,
                                        void                * p_context,
                                        uart_callback_args_t * p_callback_memory)
{
    (void) p_context;

    if ((NULL == p_ctrl) || (NULL == p_callback) || (NULL == p_callback_memory))
    {
        return FSP_ERR_ASSERTION;
    }

    g_fake_uart_callback_set_call_count++;
    g_fake_registered_callback = p_callback;

    return g_fake_uart_callback_set_result;
}

void fake_uart_reset(void)
{
    memset(g_fake_uart_tx_capture, 0, sizeof(g_fake_uart_tx_capture));

    g_fake_uart_tx_capture_len = 0U;

    g_fake_uart_open_call_count = 0U;
    g_fake_uart_write_call_count = 0U;
    g_fake_uart_callback_set_call_count = 0U;

    g_fake_uart_open_result = FSP_SUCCESS;
    g_fake_uart_write_result = FSP_SUCCESS;
    g_fake_uart_callback_set_result = FSP_SUCCESS;

    g_fake_registered_callback = NULL;

    g_fake_uart_cfg.p_callback = NULL;

    g_uart_jlob_vcom_fake.p_ctrl = &g_fake_uart_ctrl_object;
    g_uart_jlob_vcom_fake.p_cfg  = &g_fake_uart_cfg;
    g_uart_jlob_vcom_fake.p_api  = &g_fake_uart_api;
}

void fake_uart_set_open_result(fsp_err_t err)
{
    g_fake_uart_open_result = err;
}

void fake_uart_set_write_result(fsp_err_t err)
{
    g_fake_uart_write_result = err;
}

void fake_uart_set_callback_set_result(fsp_err_t err)
{
    g_fake_uart_callback_set_result = err;
}

void fake_uart_set_configured_callback(uart_callback_t p_callback)
{
    g_fake_uart_cfg.p_callback = p_callback;
}

uint32_t fake_uart_open_call_count_get(void)
{
    return g_fake_uart_open_call_count;
}

uint32_t fake_uart_write_call_count_get(void)
{
    return g_fake_uart_write_call_count;
}

uint32_t fake_uart_callback_set_call_count_get(void)
{
    return g_fake_uart_callback_set_call_count;
}

uart_callback_t fake_uart_registered_callback_get(void)
{
    return g_fake_registered_callback;
}

uint32_t fake_uart_tx_capture_len_get(void)
{
    return g_fake_uart_tx_capture_len;
}

uint8_t const * fake_uart_tx_capture_get(void)
{
    return g_fake_uart_tx_capture;
}

void fake_uart_invalidate_api(void)
{
    g_uart_jlob_vcom_fake.p_api = NULL;
}

void fake_uart_invalidate_ctrl(void)
{
    g_uart_jlob_vcom_fake.p_ctrl = NULL;
}

void fake_uart_invalidate_cfg(void)
{
    g_uart_jlob_vcom_fake.p_cfg = NULL;
}

void fake_uart_raise_rx_char(char c)
{
    if (NULL != g_fake_registered_callback)
    {
        uart_callback_args_t args =
        {
            .event = UART_EVENT_RX_CHAR,
            .data  = (uint32_t) ((uint8_t) c),
        };

        g_fake_registered_callback(&args);
    }
}

void fake_uart_raise_tx_data_empty(void)
{
    if (NULL != g_fake_registered_callback)
    {
        uart_callback_args_t args =
        {
            .event = UART_EVENT_TX_DATA_EMPTY,
            .data  = 0U,
        };

        g_fake_registered_callback(&args);
    }
}

void fake_uart_raise_overflow_event(void)
{
    if (NULL != g_fake_registered_callback)
    {
        uart_callback_args_t args =
        {
            .event = UART_EVENT_ERR_OVERFLOW,
            .data  = 0U,
        };

        g_fake_registered_callback(&args);
    }
}