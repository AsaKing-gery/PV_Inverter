/**
 ****************************************************************************************************
 * @file        touch.h
 * @author      ����ԭ���Ŷ�(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-25
 * @brief       ������ ��������
 *   @note      ֧�ֵ���/����ʽ������
 *              ������������֧��ADS7843/7846/UH7843/7846/XPT2046/TSC2046/GT9147/GT9271/FT5206�ȣ�����
 *
 * @license     Copyright (c) 2020-2032, �������������ӿƼ����޹�˾
 ****************************************************************************************************
 * @attention
 *
 * ʵ��ƽ̨:����ԭ�� STM32F103������
 * ������Ƶ:www.yuanzige.com
 * ������̳:www.oT_PENedv.com
 * ��˾��ַ:www.alientek.com
 * �����ַ:oT_PENedv.taobao.com
 *
 * �޸�˵��
 * V1.0 20200425
 * ��һ�η���
 *
 ****************************************************************************************************
 */

#ifndef __TOUCH_H__
#define __TOUCH_H__

#include "../sys.h"
// #include "gt9xxx.h"   /* 未使用此触摸IC */
// #include "ft5206.h"   /* 未使用此触摸IC */


/******************************************************************************************/
/* ���败��������IC T_PEN/T_CS/T_MISO/T_MOSI/T_SCK ���� ���� */

#define T_PEN_GPIO_PORT                 GPIOF
#define T_PEN_GPIO_PIN                  GPIO_PIN_10
#define T_PEN_GPIO_CLK_ENABLE()         do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)   /* ����IO��ʱ��ʹ�� */

#define T_CS_GPIO_PORT                  GPIOF
#define T_CS_GPIO_PIN                   GPIO_PIN_11
#define T_CS_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)   /* ����IO��ʱ��ʹ�� */

#define T_MISO_GPIO_PORT                GPIOB
#define T_MISO_GPIO_PIN                 GPIO_PIN_2
#define T_MISO_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* ����IO��ʱ��ʹ�� */

#define T_MOSI_GPIO_PORT                GPIOF
#define T_MOSI_GPIO_PIN                 GPIO_PIN_9
#define T_MOSI_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)   /* ����IO��ʱ��ʹ�� */

#define T_CLK_GPIO_PORT                 GPIOB
#define T_CLK_GPIO_PIN                  GPIO_PIN_1
#define T_CLK_GPIO_CLK_ENABLE()         do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* ����IO��ʱ��ʹ�� */

/******************************************************************************************/

/* ���败������������ */
#define T_PEN           HAL_GPIO_ReadPin(T_PEN_GPIO_PORT, T_PEN_GPIO_PIN)           /* T_PEN */
#define T_MISO          HAL_GPIO_ReadPin(T_MISO_GPIO_PORT, T_MISO_GPIO_PIN)         /* T_MISO */

#define T_MOSI(x)     do{ x ? \
                          HAL_GPIO_WritePin(T_MOSI_GPIO_PORT, T_MOSI_GPIO_PIN, GPIO_PIN_SET) : \
                          HAL_GPIO_WritePin(T_MOSI_GPIO_PORT, T_MOSI_GPIO_PIN, GPIO_PIN_RESET); \
                      }while(0)     /* T_MOSI */

#define T_CLK(x)      do{ x ? \
                          HAL_GPIO_WritePin(T_CLK_GPIO_PORT, T_CLK_GPIO_PIN, GPIO_PIN_SET) : \
                          HAL_GPIO_WritePin(T_CLK_GPIO_PORT, T_CLK_GPIO_PIN, GPIO_PIN_RESET); \
                      }while(0)     /* T_CLK */

#define T_CS(x)       do{ x ? \
                          HAL_GPIO_WritePin(T_CS_GPIO_PORT, T_CS_GPIO_PIN, GPIO_PIN_SET) : \
                          HAL_GPIO_WritePin(T_CS_GPIO_PORT, T_CS_GPIO_PIN, GPIO_PIN_RESET); \
                      }while(0)     /* T_CS */


/* ���������� */
/* MCU��XTP2046�������XTP2046�������� */
uint16_t tp_write_and_read_ad(uint8_t cmd_data);

void tp_init(void);              /* ��ʼ�� */


#endif

















