#include "led.h"
#include "stm32f4xx_hal.h"

void LED_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = LED_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
}

void LED_On(void)
{
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
}

void LED_Off(void)
{
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
}

void LED_Toggle1(void)
{
  HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}
