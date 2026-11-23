#include "rs_stdio_uart.h"

fsp_err_t RS_STDIO_UART_GetConfigSize(uint32_t * p_size)
{
    FSP_ASSERT(NULL != p_size);
    *p_size = sizeof(rs_stdio_uart_cfg_t);
    return FSP_SUCCESS;
}

fsp_err_t RS_STDIO_UART_GenConfig(rs_stdio_uart_cfg_t * const p_cfg, uint32_t const config_size)
{
    FSP_ASSERT(NULL != p_cfg);

    fsp_err_t err = FSP_SUCCESS;
    uint32_t needed_size = 0;

    err = RS_STDIO_UART_GetConfigSize(&needed_size);

    FSP_ERROR_RETURN(config_size == needed_size, FSP_ERR_INVALID_ARGUMENT);

    extern rs_stdio_uart_cfg_t const rs_stdio_uart_ref;

    memcpy(p_cfg, &rs_stdio_uart_ref, sizeof(rs_stdio_uart_ref));

    p_cfg->uart_instance.p_cfg = &p_cfg->uart_cfg;

#if defined (R_SCI_B_UART_H)
    p_cfg->uart_instance.p_ctrl = &p_cfg->sci_b_uart_instance_ctrl;
#elif defined(R_SCI_UART_H)
    p_cfg->uart_instance.p_ctrl = &p_cfg->sci_instance_ctrl;
#elif defined (R_UARTA_H)
    p_cfg->uart_instance.p_ctrl = &p_cfg->uarta_instance_ctrl;
#endif

    err = RS_STDIO_UART_CheckConfig(p_cfg);

    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return FSP_SUCCESS;
}

static fsp_err_t check_implementation_agnostic_config(
    rs_stdio_uart_cfg_t const * const p_cfg)
{
    int32_t diff = 0;

    FSP_ERROR_RETURN(NULL != p_cfg->uart_instance.p_api,
                     FSP_ERR_INVALID_POINTER);

    FSP_ERROR_RETURN(NULL != p_cfg->uart_instance.p_cfg,
                     FSP_ERR_INVALID_POINTER);

    FSP_ERROR_RETURN(NULL != p_cfg->uart_instance.p_ctrl,
                     FSP_ERR_INVALID_POINTER);

    FSP_ERROR_RETURN((p_cfg->uart_instance.p_cfg == &p_cfg->uart_cfg), FSP_ERR_INVALID_POINTER);

    diff = memcmp(p_cfg->uart_instance.p_cfg, &p_cfg->uart_cfg, sizeof(p_cfg->uart_cfg));
    FSP_ERROR_RETURN( 0 == diff, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN( 0 <= p_cfg->uart_cfg.rxi_ipl, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN( 0 <= p_cfg->uart_cfg.rxi_irq, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN( 0 <= p_cfg->uart_cfg.txi_ipl, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN( 0 <= p_cfg->uart_cfg.txi_irq, FSP_ERR_INVALID_DATA);
#if !defined (R_UARTA_H)
    FSP_ERROR_RETURN( 0 <=  p_cfg->uart_cfg.tei_ipl, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN( 0 <= p_cfg->uart_cfg.tei_irq, FSP_ERR_INVALID_DATA);
#endif

    FSP_ERROR_RETURN( 0 == diff, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN( 0 == diff, FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN((p_cfg->rxd != p_cfg->txd), FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN((NULL == p_cfg->uart_instance.p_cfg->p_transfer_rx), FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN((NULL == p_cfg->uart_instance.p_cfg->p_transfer_tx), FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN((NULL != p_cfg->uart_instance.p_cfg->p_extend), FSP_ERR_INVALID_POINTER);


    return FSP_SUCCESS;
}

fsp_err_t RS_STDIO_UART_CheckConfig(rs_stdio_uart_cfg_t const * const p_cfg)
{
    FSP_ERROR_RETURN(NULL != p_cfg, FSP_ERR_INVALID_ARGUMENT);
    fsp_err_t fsp_err = FSP_SUCCESS;
    fsp_err = check_implementation_agnostic_config(p_cfg);
    FSP_ERROR_RETURN(FSP_SUCCESS == fsp_err, fsp_err);

#if defined (R_SCI_B_UART_H) || defined (R_SCI_UART_H) || defined (R_UARTA_H)
    extern fsp_err_t rs_stdio_uart_check_config(rs_stdio_uart_cfg_t const * const p_cfg);
    fsp_err = rs_stdio_uart_check_config(p_cfg);
#else
    extern fsp_err_t rs_stdio_fake_uart_check_config(rs_stdio_uart_cfg_t const * const p_cfg);
    fsp_err = rs_stdio_fake_uart_check_config(p_cfg);
#endif
    return fsp_err;
}
