/***********************************************************************************************************************
 * @file FreeRTOS.h
 * @brief FreeRTOS stub for host-based unit tests in Example Project mode.
 *
 * Provides the minimum type definitions and no-op function stubs needed to
 * compile rs_rtc_logger.c with RS_RTC_LOGGER_CFG_EXAMPLE_MODE_ENABLE=1 on
 * a host (x86/x64) without a real FreeRTOS kernel.
 *
 * The consumer task created by xTaskCreateStatic() never actually runs on
 * the host — EP-mode host tests exercise Open()/EventTrigger()/EventDequeue()
 * directly without a scheduler.  Task-notification functions are no-ops.
 **********************************************************************************************************************/

#ifndef FREERTOS_H
#define FREERTOS_H

#include <stdint.h>
#include <stdbool.h>

/* ── Basic FreeRTOS types ───────────────────────────────────────────────── */

typedef uint32_t  TickType_t;
typedef long      BaseType_t;
typedef uint32_t  UBaseType_t;
typedef uint32_t  StackType_t;
typedef void *    TaskHandle_t;

typedef struct { uint8_t _reserved[256]; } StaticTask_t;

#define pdFALSE   ((BaseType_t)0)
#define pdTRUE    ((BaseType_t)1)
#define pdPASS    pdTRUE
#define pdFAIL    pdFALSE

#define portMAX_DELAY  ((TickType_t)0xFFFFFFFFU)

#define tskIDLE_PRIORITY  ((UBaseType_t)0U)

#define pdMS_TO_TICKS(ms)  ((TickType_t)(ms))

/* ── Task management stubs ──────────────────────────────────────────────── */

static inline TaskHandle_t xTaskCreateStatic(
    void (*pxTaskCode)(void *), const char * pcName,
    uint32_t ulStackDepth, void * pvParameters,
    UBaseType_t uxPriority, StackType_t * puxStackBuffer,
    StaticTask_t * pxTaskBuffer)
{
    (void)pxTaskCode; (void)pcName; (void)ulStackDepth;
    (void)pvParameters; (void)uxPriority;
    (void)puxStackBuffer; (void)pxTaskBuffer;
    /* Return non-NULL so the PES stores a valid-looking task handle */
    return (TaskHandle_t)(uintptr_t)1U;
}

/* ── Task notification stubs ────────────────────────────────────────────── */

static inline uint32_t ulTaskNotifyTake(BaseType_t xClearCountOnExit,
                                         TickType_t xTicksToWait)
{
    (void)xClearCountOnExit; (void)xTicksToWait;
    return 1U;  /* always signal immediately — no blocking on host */
}

static inline void xTaskNotifyGive(TaskHandle_t xTaskToNotify)
{
    (void)xTaskToNotify;
}

static inline void vTaskNotifyGiveFromISR(TaskHandle_t xTaskToNotify,
                                           BaseType_t * pxHigherPriorityTaskWoken)
{
    (void)xTaskToNotify;
    if (pxHigherPriorityTaskWoken)
    {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
}

#define portYIELD_FROM_ISR(x)  ((void)(x))

/* ── Tick count stubs ───────────────────────────────────────────────────── */

static inline TickType_t xTaskGetTickCount(void)         { return 0U; }
static inline TickType_t xTaskGetTickCountFromISR(void)  { return 0U; }

#endif /* FREERTOS_H */
