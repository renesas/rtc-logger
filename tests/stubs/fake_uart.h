#ifndef FAKE_UART_H
#define FAKE_UART_H

#include <stdint.h>
#include "r_uart_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FAKE_UART_TX_CAPTURE_SIZE    (512U)

extern uart_instance_t g_uart_jlob_vcom_fake;

void fake_uart_reset(void);

void fake_uart_set_open_result(fsp_err_t err);
void fake_uart_set_write_result(fsp_err_t err);
void fake_uart_set_callback_set_result(fsp_err_t err);

void fake_uart_set_configured_callback(uart_callback_t p_callback);

uint32_t fake_uart_open_call_count_get(void);
uint32_t fake_uart_write_call_count_get(void);
uint32_t fake_uart_callback_set_call_count_get(void);

uart_callback_t fake_uart_registered_callback_get(void);

uint32_t fake_uart_tx_capture_len_get(void);
uint8_t const * fake_uart_tx_capture_get(void);

void fake_uart_invalidate_api(void);
void fake_uart_invalidate_ctrl(void);
void fake_uart_invalidate_cfg(void);

void fake_uart_raise_rx_char(char c);
void fake_uart_raise_tx_data_empty(void);
void fake_uart_raise_overflow_event(void);

#ifdef __cplusplus
}
#endif

#endif