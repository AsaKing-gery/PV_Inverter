/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    display.h
  * @brief   LCD显示驱动接口，连接LVGL和ILI9341驱动
  *          适配正点原子ILI9341 240x320屏幕
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __DISPLAY_H
#define __DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lvgl.h"

/* LCD配置 -------------------------------------------------------------------*/
#define LCD_WIDTH           240
#define LCD_HEIGHT          320
#define LCD_PIXEL_SIZE      2       /* RGB565 = 2字节 */

/* 引脚定义 (使用main.h中已定义的宏) */
/* LCD复位 - PE6用于LCD硬件复位 */
#define LCD_RST_GPIO_PORT   KEY_RST_GPIO_Port  /* GPIOE */
#define LCD_RST_GPIO_PIN    KEY_RST_Pin        /* GPIO_PIN_6 */

/* LCD背光 - 已配置在gpio.c中 */
#define LCD_BL_GPIO_PORT    LCD_BL_GPIO_Port
#define LCD_BL_GPIO_PIN     LCD_BL_Pin

/* 触摸中断 - 已配置在gpio.c中 */
#define LCD_INT_GPIO_PORT   T_PEN_GPIO_Port
#define LCD_INT_GPIO_PIN    T_PEN_Pin

/* 触摸配置 */
#define TOUCH_SPI           hspi2
#define TOUCH_CS_PORT       GPIOB
#define TOUCH_CS_PIN        GPIO_PIN_12
#define TOUCH_IRQ_PORT      GPIOB
#define TOUCH_IRQ_PIN       GPIO_PIN_1

/* LVGL缓冲区大小 */
#define LVGL_BUFFER_SIZE    (LCD_WIDTH * LCD_HEIGHT / 10)  /* 1/10屏幕大小 */

/* 外部变量 ------------------------------------------------------------------*/
extern SPI_HandleTypeDef hspi2;
extern SRAM_HandleTypeDef hsram1;

/* 函数声明 ------------------------------------------------------------------*/

/**
  * @brief  显示系统初始化
  * @param  None
  * @retval None
  */
void Display_Init(void);

/**
  * @brief  背光控制
  * @param  brightness: 亮度值 0-100
  * @retval None
  */
void Display_SetBacklight(uint8_t brightness);

/**
  * @brief  屏幕复位
  * @param  None
  * @retval None
  */
void Display_Reset(void);

/**
  * @brief  LVGL任务处理，需在任务中循环调用
  * @param  None
  * @retval None
  */
void Display_LvglHandler(void);

/**
  * @brief  获取触摸坐标
  * @param  x: X坐标指针
  * @param  y: Y坐标指针
  * @retval 0: 无触摸, 1: 有触摸
  */
uint8_t Display_GetTouch(uint16_t* x, uint16_t* y);

/**
  * @brief  触摸校准
  * @param  None
  * @retval None
  */
void Display_TouchCalibrate(void);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_H */
