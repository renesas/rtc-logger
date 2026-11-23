#ifndef RS_STDIO_UART_H
#define RS_STDIO_UART_H

#include  <string.h>
#include "r_uart_api.h"
#include "r_ioport.h"

#if __has_include ("r_sci_uart.h")
#include "r_sci_uart.h"
#elif __has_include ("r_sci_b_uart.h")
#include "r_sci_b_uart.h"
#elif __has_include ("r_uarta.h")
#include "r_uarta.h"
#else
#include "fake_uart.h"
#endif

typedef struct {
    ioport_port_pin_t txd;
    ioport_port_pin_t rxd;
    uart_instance_t uart_instance;
    uart_cfg_t  uart_cfg;
#if defined (R_SCI_B_UART_H)
    sci_b_baud_setting_t sci_b_baud_setting;
    sci_b_uart_extended_cfg_t sci_b_uart_extended_cfg;
    sci_b_uart_instance_ctrl_t sci_b_uart_instance_ctrl;
#elif defined(R_SCI_UART_H)
    baud_setting_t sci_uart_baud_setting;
    sci_uart_extended_cfg_t sci_uart_extended_cfg;
    sci_uart_instance_ctrl_t sci_instance_ctrl;
#elif defined (R_UARTA_H)
    uarta_baud_setting_t uarta_baud_setting;
    uarta_extended_cfg_t uarta_extended_cfg;
    uarta_instance_ctrl_t uarta_instance_ctrl;
#endif
}rs_stdio_uart_cfg_t;

fsp_err_t RS_STDIO_UART_GetConfigSize(uint32_t * p_size);
fsp_err_t RS_STDIO_UART_CheckConfig(rs_stdio_uart_cfg_t const * const p_cfg);
fsp_err_t RS_STDIO_UART_GenConfig(rs_stdio_uart_cfg_t * const p_cfg, uint32_t const config_size);
fsp_err_t RS_STDIO_UART_CheckApplicationConfiguration(void);

#endif