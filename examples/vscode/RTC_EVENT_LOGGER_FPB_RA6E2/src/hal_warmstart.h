/***********************************************************************************************************************
 * File Name    : hal_warmstart.h
 * Description  : Warm Reset Detection Helper — Public Interface
 **********************************************************************************************************************/

#ifndef HAL_WARMSTART_H
#define HAL_WARMSTART_H

#include <stdint.h>
#include <stdbool.h>

bool     hal_warmstart_is_warm_reset        (void);
void     hal_warmstart_clear_reset_flags    (void);
void     hal_warmstart_seq_count_save       (uint32_t seq_count);
uint32_t hal_warmstart_seq_count_load       (bool * p_valid);
void     hal_warmstart_seq_count_invalidate (void);

#endif /* HAL_WARMSTART_H */
