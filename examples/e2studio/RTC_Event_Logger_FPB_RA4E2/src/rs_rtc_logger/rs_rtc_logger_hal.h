/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_rtc_logger_hal.h
 * @brief HAL abstraction shim for the RTC Event Logger PES.
 *
 * Internal header — not part of the public API.
 * Declares the thin shim functions that wrap FSP HAL driver calls,
 * allowing the core module (rs_rtc_logger.c) to remain agnostic to
 * the specific FSP HAL implementation details.
 */

#ifndef RS_RTC_LOGGER_HAL_H
#define RS_RTC_LOGGER_HAL_H

#include <rs_rtc_logger/rs_rtc_logger_types.h>

/***********************************************************************************************************************
 * Internal function prototypes — HAL shim layer
 **********************************************************************************************************************/

/**
 * @brief Open the RTC HAL driver.
 *
 * Wraps p_cfg->p_rtc->p_api->open(). Validates RTC oscillator status
 * after successful open by calling calendarTimeGet() as a readiness probe.
 *
 * @param[in] p_ctrl  PES control block (p_cfg->p_rtc bound).
 *
 * @retval FSP_SUCCESS              RTC opened and oscillator confirmed running.
 * @retval FSP_ERR_NOT_INITIALIZED  RTC opened but oscillator not running.
 * @retval (other)                  Propagated from RTC HAL open().
 */
fsp_err_t rs_rtc_logger_hal_rtc_open(rs_rtc_logger_ctrl_t * p_ctrl);

/**
 * @brief Close the RTC HAL driver.
 *
 * Wraps p_cfg->p_rtc->p_api->close().
 *
 * @param[in] p_ctrl  PES control block.
 *
 * @retval FSP_SUCCESS   RTC closed.
 * @retval (other)       Propagated from RTC HAL close().
 */
fsp_err_t rs_rtc_logger_hal_rtc_close(rs_rtc_logger_ctrl_t * p_ctrl);

/**
 * @brief Get the current RTC calendar time.
 *
 * Wraps p_cfg->p_rtc->p_api->calendarTimeGet().
 *
 * @param[in]  p_ctrl  PES control block.
 * @param[out] p_time  Populated with current RTC value.
 *
 * @retval FSP_SUCCESS   Time retrieved.
 * @retval (other)       Propagated from RTC HAL calendarTimeGet().
 */
fsp_err_t rs_rtc_logger_hal_rtc_time_get(rs_rtc_logger_ctrl_t * p_ctrl,
                                          rtc_time_t           * p_time);

/**
 * @brief Set the RTC calendar time.
 *
 * Wraps p_cfg->p_rtc->p_api->calendarTimeSet().
 *
 * @param[in] p_ctrl  PES control block.
 * @param[in] p_time  Time value to set.
 *
 * @retval FSP_SUCCESS   Time set.
 * @retval (other)       Propagated from RTC HAL calendarTimeSet().
 */
fsp_err_t rs_rtc_logger_hal_rtc_time_set(rs_rtc_logger_ctrl_t * p_ctrl,
                                          rtc_time_t           * p_time);

/**
 * @brief Update the LOCO frequency register (RFRL) for drift compensation.
 *
 * Stops the RTC counter, writes a new RFRL value, reads back the retained
 * calendar time, and restarts the counter so no seconds are lost.
 *
 * This is the correct drift-compensation path for LOCO clock source.
 * R_RTC_ErrorAdjustmentSet() (RADJ register) is Sub-clock only and returns
 * FSP_ERR_UNSUPPORTED when the clock source is LOCO.
 *
 * Valid RFRL range (FSP constraint): 8 ≤ rfrl_value ≤ 510.
 *
 * @param[in] p_ctrl      PES control block.
 * @param[in] rfrl_value  New RFRL register value (8–510).
 *
 * @retval FSP_SUCCESS            RFRL updated; counter restarted.
 * @retval FSP_ERR_INVALID_ARGUMENT  rfrl_value outside valid range.
 * @retval (other)                Propagated from calendarTimeGet/Set.
 */
fsp_err_t rs_rtc_logger_hal_loco_freq_set(rs_rtc_logger_ctrl_t * p_ctrl,
                                           uint16_t               rfrl_value);

/**
 * @brief Open the UART HAL driver.
 *
 * Wraps p_cfg->p_uart->p_api->open().
 *
 * @param[in] p_ctrl  PES control block (p_cfg->p_uart bound).
 *
 * @retval FSP_SUCCESS   UART opened.
 * @retval (other)       Propagated from UART HAL open().
 */
fsp_err_t rs_rtc_logger_hal_uart_open(rs_rtc_logger_ctrl_t * p_ctrl);

/**
 * @brief Close the UART HAL driver.
 *
 * Wraps p_cfg->p_uart->p_api->close().
 *
 * @param[in] p_ctrl  PES control block.
 *
 * @retval FSP_SUCCESS   UART closed.
 * @retval (other)       Propagated from UART HAL close().
 */
fsp_err_t rs_rtc_logger_hal_uart_close(rs_rtc_logger_ctrl_t * p_ctrl);

/**
 * @brief Submit a non-blocking UART write.
 *
 * Wraps p_cfg->p_uart->p_api->write(). Clears p_ctrl->tx_complete before
 * submitting so the HAL callback can set it upon TX completion.
 *
 * @param[in] p_ctrl   PES control block.
 * @param[in] p_data   Data buffer to transmit.
 * @param[in] length   Byte count to transmit.
 *
 * @retval FSP_SUCCESS   Write submitted.
 * @retval (other)       Propagated from UART HAL write().
 */
fsp_err_t rs_rtc_logger_hal_uart_write(rs_rtc_logger_ctrl_t * p_ctrl,
                                        uint8_t        const * p_data,
                                        uint32_t               length);

#endif /* RS_RTC_LOGGER_HAL_H */
