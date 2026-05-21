/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    display.c
  * @brief   LCD显示驱动实现，连接LVGL和ILI9341
  *          使用FSMC接口驱动LCD，SPI2驱动XPT2046触摸
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "display.h"
#include "fsmc.h"
#include "spi.h"
#include <string.h>

/* Private define ------------------------------------------------------------*/
/* LCD寄存器地址定义 */
#define LCD_BASE_ADDR       ((uint32_t)(0x60000000 | 0x000007FE))
#define LCD_REG_ADDR        (*((volatile uint16_t *)LCD_BASE_ADDR))
#define LCD_DATA_ADDR       (*((volatile uint16_t *)(LCD_BASE_ADDR + 2)))

/* 触摸校准参数 */
#define TOUCH_CALIBRATE_X_MIN   100
#define TOUCH_CALIBRATE_X_MAX   3900
#define TOUCH_CALIBRATE_Y_MIN   100
#define TOUCH_CALIBRATE_Y_MAX   3900

/* Private variables ---------------------------------------------------------*/
/* LVGL显示缓冲区 */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LVGL_BUFFER_SIZE];
static lv_color_t buf2[LVGL_BUFFER_SIZE];

/* 触摸校准值 */
static int32_t touch_x_offset = 0;
static int32_t touch_x_scale = 1;
static int32_t touch_y_offset = 0;
static int32_t touch_y_scale = 1;

/* Private function prototypes -----------------------------------------------*/
static void LCD_WriteReg(uint16_t reg, uint16_t data);
static uint16_t LCD_ReadReg(uint16_t reg);
static void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
static void LCD_Flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void Touch_Read(lv_indev_drv_t *drv, lv_indev_data_t *data);
static uint16_t Touch_ReadAD(uint8_t cmd);

/* Private user code ---------------------------------------------------------*/

/**
  * @brief  写LCD寄存器
  */
static void LCD_WriteReg(uint16_t reg, uint16_t data)
{
    LCD_REG_ADDR = reg;
    LCD_DATA_ADDR = data;
}

/**
  * @brief  读LCD寄存器
  */
static uint16_t LCD_ReadReg(uint16_t reg)
{
    LCD_REG_ADDR = reg;
    return LCD_DATA_ADDR;
}

/**
  * @brief  设置LCD窗口
  */
static void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    /* 设置列地址 */
    LCD_WriteReg(0x2A, x1 >> 8);
    LCD_WriteReg(0x2A + 1, x1 & 0xFF);
    LCD_WriteReg(0x2A + 2, x2 >> 8);
    LCD_WriteReg(0x2A + 3, x2 & 0xFF);
    
    /* 设置行地址 */
    LCD_WriteReg(0x2B, y1 >> 8);
    LCD_WriteReg(0x2B + 1, y1 & 0xFF);
    LCD_WriteReg(0x2B + 2, y2 >> 8);
    LCD_WriteReg(0x2B + 3, y2 & 0xFF);
    
    /* 准备写GRAM */
    LCD_REG_ADDR = 0x2C;
}

/**
  * @brief  LVGL刷新回调
  */
static void LCD_Flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    
    /* 设置窗口 */
    LCD_SetWindow(area->x1, area->y1, area->x2, area->y2);
    
    /* 批量写入像素数据 */
    for (uint32_t i = 0; i < width * height; i++)
    {
        LCD_DATA_ADDR = color_p[i].full;
    }
    
    /* 通知LVGL刷新完成 */
    lv_disp_flush_ready(disp_drv);
}

/**
  * @brief  读取XPT2046 ADC值
  */
static uint16_t Touch_ReadAD(uint8_t cmd)
{
    uint8_t txData[3] = {cmd, 0xFF, 0xFF};
    uint8_t rxData[3] = {0};
    uint16_t result = 0;
    
    /* CS拉低 */
    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_RESET);
    
    /* 发送命令并读取 */
    HAL_SPI_TransmitReceive(&TOUCH_SPI, txData, rxData, 3, 100);
    
    /* CS拉高 */
    HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET);
    
    /* 提取12位结果 */
    result = ((rxData[1] << 8) | rxData[2]) >> 3;
    
    return result;
}

/**
  * @brief  读取触摸坐标
  */
static void Touch_Read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static int32_t last_x = 0;
    static int32_t last_y = 0;
    
    /* 检查触摸中断 */
    if (HAL_GPIO_ReadPin(TOUCH_IRQ_PORT, TOUCH_IRQ_PIN) == GPIO_PIN_SET)
    {
        /* 无触摸 */
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = last_x;
        data->point.y = last_y;
        return;
    }
    
    /* 读取X坐标 */
    uint16_t x_raw = Touch_ReadAD(0xD0);  /* X通道 */
    /* 读取Y坐标 */
    uint16_t y_raw = Touch_ReadAD(0x90);  /* Y通道 */
    
    /* 转换为屏幕坐标 */
    int32_t x = (x_raw - touch_x_offset) * touch_x_scale / 1000;
    int32_t y = (y_raw - touch_y_offset) * touch_y_scale / 1000;
    
    /* 限幅 */
    if (x < 0) x = 0;
    if (x >= LCD_WIDTH) x = LCD_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= LCD_HEIGHT) y = LCD_HEIGHT - 1;
    
    /* 坐标转换 (根据屏幕方向调整) */
    data->point.x = LCD_WIDTH - 1 - x;  /* X轴镜像 */
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
    
    last_x = data->point.x;
    last_y = data->point.y;
}

/* Public functions ----------------------------------------------------------*/

/**
  * @brief  显示系统初始化
  * @note   依赖main.c中已完成的初始化:
  *         - MX_GPIO_Init(): PB0(背光), PB1(触摸中断), PE6(按键复位)
  *         - MX_SPI2_Init(): SPI2用于触摸通信
  *         - MX_FSMC_Init(): FSMC用于LCD通信
  * @note   PE6是按键复位(按下按键复位屏幕)，不是自动复位
  */
void Display_Init(void)
{
    /* 注意: PE6是按键复位，如需复位屏幕请按下按键 */
    /* 不在此处自动复位，避免与按键功能冲突 */
    
    /* 打开背光 (PB0已在gpio.c中配置) */
    Display_SetBacklight(100);
    
    /* 初始化LVGL */
    lv_init();
    
    /* 初始化显示缓冲区 */
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LVGL_BUFFER_SIZE);
    
    /* 注册显示驱动 */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = LCD_Flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    /* 注册触摸驱动 */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = Touch_Read;
    lv_indev_drv_register(&indev_drv);
    
    /* 设置默认触摸校准值 */
    touch_x_offset = TOUCH_CALIBRATE_X_MIN;
    touch_x_scale = (LCD_WIDTH * 1000) / (TOUCH_CALIBRATE_X_MAX - TOUCH_CALIBRATE_X_MIN);
    touch_y_offset = TOUCH_CALIBRATE_Y_MIN;
    touch_y_scale = (LCD_HEIGHT * 1000) / (TOUCH_CALIBRATE_Y_MAX - TOUCH_CALIBRATE_Y_MIN);
}

/**
  * @brief  背光控制
  */
void Display_SetBacklight(uint8_t brightness)
{
    if (brightness > 100) brightness = 100;
    
    /* 简单开关控制，如需PWM调光需要配置TIM */
    if (brightness > 0)
    {
        HAL_GPIO_WritePin(LCD_BL_GPIO_PORT, LCD_BL_GPIO_PIN, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(LCD_BL_GPIO_PORT, LCD_BL_GPIO_PIN, GPIO_PIN_RESET);
    }
}

/**
  * @brief  屏幕硬件复位
  * @note   使用PE6引脚，已在gpio.c中配置
  */
void Display_Reset(void)
{
    HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_SET);
    HAL_Delay(20);
}

/**
  * @brief  LVGL任务处理
  */
void Display_LvglHandler(void)
{
    lv_timer_handler();
}

/**
  * @brief  获取触摸坐标
  */
uint8_t Display_GetTouch(uint16_t* x, uint16_t* y)
{
    if (HAL_GPIO_ReadPin(TOUCH_IRQ_PORT, TOUCH_IRQ_PIN) == GPIO_PIN_SET)
    {
        return 0;  /* 无触摸 */
    }
    
    uint16_t x_raw = Touch_ReadAD(0xD0);
    uint16_t y_raw = Touch_ReadAD(0x90);
    
    *x = (x_raw - touch_x_offset) * touch_x_scale / 1000;
    *y = (y_raw - touch_y_offset) * touch_y_scale / 1000;
    
    /* 限幅 */
    if (*x >= LCD_WIDTH) *x = LCD_WIDTH - 1;
    if (*y >= LCD_HEIGHT) *y = LCD_HEIGHT - 1;
    
    return 1;  /* 有触摸 */
}

/**
  * @brief  触摸校准
  */
void Display_TouchCalibrate(void)
{
    /* 简化的校准：使用默认值 */
    touch_x_offset = TOUCH_CALIBRATE_X_MIN;
    touch_x_scale = (LCD_WIDTH * 1000) / (TOUCH_CALIBRATE_X_MAX - TOUCH_CALIBRATE_X_MIN);
    touch_y_offset = TOUCH_CALIBRATE_Y_MIN;
    touch_y_scale = (LCD_HEIGHT * 1000) / (TOUCH_CALIBRATE_Y_MAX - TOUCH_CALIBRATE_Y_MIN);
}
