#include <stddef.h>
#include <string.h>

#include "rs_stdio_uart.h"

#if defined(BOARD_RA6E2_FPB)

#if !defined(R_SCI_UART_H)
#error "Please enable using the Smart Configurator configuration.xml > Components > Renesas > HAL Drivers > all > r_sci_uart."
#endif

sci_uart_instance_ctrl_t g_uart_jlob_vcom_ref_ctrl;

const baud_setting_t g_uart_jlob_vcom_ref_baud_setting =
{
    /* Baud rate calculated with 0.469% error, for this project's PCLKA
     * (200 MHz PLL / 4 = 50 MHz — see the clock check below). */
    .semr_baudrate_bits_b.abcse = 0,
    .semr_baudrate_bits_b.abcs  = 0,
    .semr_baudrate_bits_b.bgdm  = 1,
    .cks                        = 0,
    .brr                        = 26,
    .mddr                       = (uint8_t) 256,
    .semr_baudrate_bits_b.brme  = false
};

/** UART extended configuration for UARTonSCI HAL driver */
const sci_uart_extended_cfg_t g_uart_jlob_vcom_ref_cfg_extend =
{
    .clock             = SCI_UART_CLOCK_INT,
    .rx_edge_start     = SCI_UART_START_BIT_FALLING_EDGE,
    .noise_cancel      = SCI_UART_NOISE_CANCELLATION_DISABLE,
    .rx_fifo_trigger   = SCI_UART_RX_FIFO_TRIGGER_MAX,
    .p_baud_setting    = (baud_setting_t *)&g_uart_jlob_vcom_ref_baud_setting,
    .flow_control      = SCI_UART_FLOW_CONTROL_RTS,

#if 0xFF != 0xFF
    .flow_control_pin = BSP_IO_PORT_FF_PIN_0xFF,
#else
    .flow_control_pin = (bsp_io_port_pin_t) UINT16_MAX,
#endif

    .rs485_setting =
    {
        .enable   = SCI_UART_RS485_DISABLE,
        .polarity = SCI_UART_RS485_DE_POLARITY_HIGH,

#if 0xFF != 0xFF
        .de_control_pin = BSP_IO_PORT_FF_PIN_0xFF,
#else
        .de_control_pin = (bsp_io_port_pin_t) UINT16_MAX,
#endif
    },

    .irda_setting =
    {
        .ircr_bits_b.ire     = 0,
        .ircr_bits_b.irrxinv = 0,
        .ircr_bits_b.irtxinv = 0,
    },
};

/** UART interface configuration */
const uart_cfg_t g_uart_jlob_vcom_ref_cfg =
{
    .channel     = 0,
    .data_bits   = UART_DATA_BITS_8,
    .parity      = UART_PARITY_OFF,
    .stop_bits   = UART_STOP_BITS_1,
    .p_callback  = NULL,
    .p_context   = NULL,
    .p_extend    = &g_uart_jlob_vcom_ref_cfg_extend,

#define RA_NOT_DEFINED (1)

#if (RA_NOT_DEFINED == RA_NOT_DEFINED)
    .p_transfer_tx = NULL,
#else
    .p_transfer_tx = &RA_NOT_DEFINED,
#endif

#if (RA_NOT_DEFINED == RA_NOT_DEFINED)
    .p_transfer_rx = NULL,
#else
    .p_transfer_rx = &RA_NOT_DEFINED,
#endif

#undef RA_NOT_DEFINED

    .rxi_ipl = (12),
    .txi_ipl = (12),
    .tei_ipl = (12),
    .eri_ipl = (12),

#if defined(VECTOR_NUMBER_SCI0_RXI)
    .rxi_irq = VECTOR_NUMBER_SCI0_RXI,
#else
    .rxi_irq = FSP_INVALID_VECTOR,
#endif

#if defined(VECTOR_NUMBER_SCI0_TXI)
    .txi_irq = VECTOR_NUMBER_SCI0_TXI,
#else
    .txi_irq = FSP_INVALID_VECTOR,
#endif

#if defined(VECTOR_NUMBER_SCI0_TEI)
    .tei_irq = VECTOR_NUMBER_SCI0_TEI,
#else
    .tei_irq = FSP_INVALID_VECTOR,
#endif

#if defined(VECTOR_NUMBER_SCI0_ERI)
    .eri_irq = VECTOR_NUMBER_SCI0_ERI,
#else
    .eri_irq = FSP_INVALID_VECTOR,
#endif
};

/* Instance structure to use this module. */
const uart_instance_t g_uart_jlob_vcom_ref =
{
    .p_ctrl = &g_uart_jlob_vcom_ref_ctrl,
    .p_cfg  = &g_uart_jlob_vcom_ref_cfg,
    .p_api  = &g_uart_on_sci,
};

rs_stdio_uart_cfg_t const rs_stdio_uart_ref =
{
    .rxd = IOPORT_PORT_04_PIN_10,
    .txd = IOPORT_PORT_04_PIN_11,

    .uart_instance = g_uart_jlob_vcom_ref,
    .uart_cfg = g_uart_jlob_vcom_ref_cfg,

    .sci_uart_baud_setting = g_uart_jlob_vcom_ref_baud_setting,
    .sci_uart_extended_cfg = g_uart_jlob_vcom_ref_cfg_extend,
    .sci_instance_ctrl = {0},
};

static fsp_err_t check_config_generics(
    rs_stdio_uart_cfg_t const * const p_cfg)
{

    int32_t diff = 0;
    
    FSP_ERROR_RETURN(NULL != p_cfg->uart_instance.p_api,
                     FSP_ERR_INVALID_POINTER);

    FSP_ERROR_RETURN(NULL != p_cfg->uart_instance.p_cfg,
                     FSP_ERR_INVALID_POINTER);

    FSP_ERROR_RETURN(NULL != p_cfg->uart_instance.p_ctrl,
                     FSP_ERR_INVALID_POINTER);

    FSP_ERROR_RETURN(&p_cfg->sci_instance_ctrl == p_cfg->uart_instance.p_ctrl,
                     FSP_ERR_INVALID_POINTER);

    FSP_ERROR_RETURN(&g_uart_on_sci ==
                         p_cfg->uart_instance.p_api,
                     FSP_ERR_INVALID_ARGUMENT);

    FSP_ERROR_RETURN((ioport_port_pin_t) BSP_IO_PORT_FF_PIN_FF > p_cfg->txd,
                     FSP_ERR_INVALID_ARGUMENT);

    FSP_ERROR_RETURN((ioport_port_pin_t) BSP_IO_PORT_FF_PIN_FF > p_cfg->rxd,
                     FSP_ERR_INVALID_ARGUMENT);

    diff = memcmp(p_cfg->uart_instance.p_cfg, &p_cfg->uart_cfg, sizeof(p_cfg->uart_cfg));
    FSP_ERROR_RETURN( 0 == diff, FSP_ERR_INVALID_DATA);


    diff = memcmp(p_cfg->uart_instance.p_ctrl, &p_cfg->sci_instance_ctrl, sizeof(p_cfg->sci_instance_ctrl));
    FSP_ERROR_RETURN( 0 == diff, FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(BSP_IRQ_DISABLED !=
                         p_cfg->uart_instance.p_cfg->rxi_ipl,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(BSP_IRQ_DISABLED !=
                         p_cfg->uart_instance.p_cfg->txi_ipl,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(BSP_IRQ_DISABLED !=
                         p_cfg->uart_instance.p_cfg->tei_ipl,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(FSP_INVALID_VECTOR !=
                         p_cfg->uart_instance.p_cfg->rxi_irq,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(FSP_INVALID_VECTOR !=
                         p_cfg->uart_instance.p_cfg->txi_irq,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(FSP_INVALID_VECTOR !=
                         p_cfg->uart_instance.p_cfg->tei_irq,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(NULL !=
                         p_cfg->uart_instance.p_cfg->p_extend,
                     FSP_ERR_INVALID_POINTER);

    /*
     * Reference UART settings assume:
     *
     * HOCO  = 20 MHz
     * PCLKA = 50 MHz  (this project's PLL: HOCO x10 / 1 = 200 MHz,
     *                   PCLKA divider /4 -> 50 MHz. The Buffered I/O UART
     *                   driver's own reference example project uses a
     *                   different PCLKA divider, /2, giving 100 MHz — the
     *                   two projects' clock trees differ, so this file's
     *                   reference values are tuned to this project's real
     *                   clock configuration, not the reference example's.)
     *
     * Different CGC configuration requires
     * regenerated baud settings.
     */

    FSP_ERROR_RETURN(
            UINT32_C(20000000) == R_BSP_SourceClockHzGet(FSP_PRIV_CLOCK_HOCO),
            FSP_ERR_INVALID_HW_CONDITION
    );
    FSP_ERROR_RETURN(
            UINT32_C(50000000) == R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_PCLKA),
            FSP_ERR_INVALID_HW_CONDITION
    );

    FSP_ERROR_RETURN(BSP_ICU_VECTOR_NUM_ENTRIES > p_cfg->uart_cfg.txi_irq, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN(BSP_ICU_VECTOR_NUM_ENTRIES > p_cfg->uart_cfg.rxi_irq, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN(BSP_ICU_VECTOR_NUM_ENTRIES > p_cfg->uart_cfg.tei_irq, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN(BSP_ICU_VECTOR_NUM_ENTRIES > p_cfg->uart_cfg.eri_irq, FSP_ERR_INVALID_DATA);

    {
        uint32_t lowest_prio = (1UL << __NVIC_PRIO_BITS) - 1UL;
        FSP_ERROR_RETURN(lowest_prio >= p_cfg->uart_cfg.txi_ipl, FSP_ERR_INVALID_DATA);
        FSP_ERROR_RETURN(lowest_prio >= p_cfg->uart_cfg.rxi_ipl, FSP_ERR_INVALID_DATA);
        FSP_ERROR_RETURN(lowest_prio >= p_cfg->uart_cfg.tei_ipl, FSP_ERR_INVALID_DATA);
        FSP_ERROR_RETURN(lowest_prio >= p_cfg->uart_cfg.eri_ipl, FSP_ERR_INVALID_DATA);
    }

    FSP_ERROR_RETURN(
        NULL !=
        ((sci_uart_extended_cfg_t const *)
             p_cfg->uart_instance.p_cfg->p_extend)->p_baud_setting,
        FSP_ERR_INVALID_POINTER);

    /* DMA/DTC transfer drivers are not supported. */
    FSP_ERROR_RETURN(NULL ==
                         p_cfg->uart_instance.p_cfg->p_transfer_rx,
                     FSP_ERR_INVALID_POINTER);

    /* DMA/DTC transfer drivers are not supported. */
    FSP_ERROR_RETURN(NULL ==
                         p_cfg->uart_instance.p_cfg->p_transfer_tx,
                     FSP_ERR_INVALID_POINTER);

    return FSP_SUCCESS;
}

static fsp_err_t check_config_exact(
    rs_stdio_uart_cfg_t const * const p_cfg)
{
    rs_stdio_uart_cfg_t const * const p_ref =
        &rs_stdio_uart_ref;

    /* RA6E2 FPB reference VCOM routing */
    FSP_ERROR_RETURN(IOPORT_PORT_04_PIN_10 == p_cfg->rxd,
                     FSP_ERR_INVALID_ARGUMENT);

    /* RA6E2 FPB reference VCOM routing */
    FSP_ERROR_RETURN(IOPORT_PORT_04_PIN_11 == p_cfg->txd,
                     FSP_ERR_INVALID_ARGUMENT);

    sci_uart_extended_cfg_t const * const p_extend_ref =
        (sci_uart_extended_cfg_t const *)
            p_ref->uart_instance.p_cfg->p_extend;

    sci_uart_extended_cfg_t const * const p_extend_actual =
        (sci_uart_extended_cfg_t const *)
            p_cfg->uart_instance.p_cfg->p_extend;

    FSP_ERROR_RETURN(p_extend_ref->clock ==
                         p_extend_actual->clock,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(p_extend_ref->rx_edge_start ==
                         p_extend_actual->rx_edge_start,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(p_extend_ref->noise_cancel ==
                         p_extend_actual->noise_cancel,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(p_extend_ref->rx_fifo_trigger ==
                         p_extend_actual->rx_fifo_trigger,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(p_extend_ref->flow_control_pin ==
                         p_extend_actual->flow_control_pin,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(p_extend_ref->flow_control ==
                         p_extend_actual->flow_control,
                     FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(
        0 == memcmp(&p_extend_ref->rs485_setting,
                    &p_extend_actual->rs485_setting,
                    sizeof(p_extend_actual->rs485_setting)),
        FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(
        0 == memcmp(&p_extend_ref->irda_setting,
                    &p_extend_actual->irda_setting,
                    sizeof(p_extend_actual->irda_setting)),
        FSP_ERR_INVALID_DATA);

    baud_setting_t const * const p_baud_ref =
        p_extend_ref->p_baud_setting;

    baud_setting_t const * const p_baud_actual =
        p_extend_actual->p_baud_setting;

    FSP_ERROR_RETURN(
        p_baud_ref->semr_baudrate_bits ==
            p_baud_actual->semr_baudrate_bits,
        FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(
        p_baud_ref->cks ==
            p_baud_actual->cks,
        FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(
        p_baud_ref->brr ==
            p_baud_actual->brr,
        FSP_ERR_INVALID_DATA);

    FSP_ERROR_RETURN(
        p_baud_ref->mddr ==
            p_baud_actual->mddr,
        FSP_ERR_INVALID_DATA);

    return FSP_SUCCESS;
}

fsp_err_t rs_stdio_uart_check_config(
    rs_stdio_uart_cfg_t const * const p_cfg)
{
    FSP_ERROR_RETURN(NULL != p_cfg,
                     FSP_ERR_INVALID_ARGUMENT);

    fsp_err_t err = check_config_generics(p_cfg);

    FSP_ERROR_RETURN(FSP_SUCCESS == err,
                     err);

    err = check_config_exact(p_cfg);

    FSP_ERROR_RETURN(FSP_SUCCESS == err,
                     err);

    return err;
}

fsp_err_t rs_stdio_config_gen(void)
{
    fsp_err_t err = FSP_SUCCESS;
    uint32_t config_size = 0;
    rs_stdio_uart_cfg_t generated_config;

    err = RS_STDIO_UART_GetConfigSize(&config_size);

    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    err = RS_STDIO_UART_GenConfig(&generated_config, sizeof(generated_config));

    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return err;
}

fsp_err_t RS_STDIO_UART_CheckApplicationConfiguration(void)
{
    extern baud_setting_t g_uart_jlob_vcom_baud_setting;
    extern uart_instance_t const g_uart_jlob_vcom;
    extern const uart_cfg_t g_uart_jlob_vcom_cfg;
    extern const sci_uart_extended_cfg_t g_uart_jlob_vcom_cfg_extend;
    extern sci_uart_instance_ctrl_t g_uart_jlob_vcom_ctrl;
    
    rs_stdio_uart_cfg_t check =
    {
        .rxd = IOPORT_PORT_04_PIN_10,
        .txd = IOPORT_PORT_04_PIN_11,

        .uart_instance = g_uart_jlob_vcom,
        .uart_cfg = g_uart_jlob_vcom_cfg,

        .sci_uart_baud_setting =
            g_uart_jlob_vcom_baud_setting,

        .sci_uart_extended_cfg =
            g_uart_jlob_vcom_cfg_extend,

        .sci_instance_ctrl =
            g_uart_jlob_vcom_ctrl,
    };

    check.uart_instance.p_cfg = &check.uart_cfg;
    check.uart_instance.p_ctrl = &check.sci_instance_ctrl;

    return RS_STDIO_UART_CheckConfig(&check);
}

#endif /* defined(BOARD_RA6E2_FPB) */
