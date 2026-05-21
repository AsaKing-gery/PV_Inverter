/**
 * @file    delay.h
 * @brief   ALIENTEK delay 适配 shim（供 LCD/触摸驱动使用）
 *          基于 STM32F4 DWT 周期计数器实现微秒级延时，
 *          毫秒级延时使用 HAL_Delay()。
 */
#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f4xx_hal.h"

/** 初始化 DWT 周期计数器（delay_us 依赖此初始化） */
static inline void delay_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/** 微秒级延时 */
static inline void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
}

/** 毫秒级延时 */
static inline void delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

#endif /* __DELAY_H */
