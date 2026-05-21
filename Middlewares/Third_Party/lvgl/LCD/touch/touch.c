/**
 ****************************************************************************************************
 * @file        touch.c
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
 * ������̳:www.openedv.com
 * ��˾��ַ:www.alientek.com
 * �����ַ:openedv.taobao.com
 *
 * �޸�˵��
 * V1.0 20200425
 * ��һ�η���
 * V1.1 20200606
 * У׼����, case 5����, �ж���ֵ���ϸ�����:
 * �ж���ֵ����0���ж�, ��ֵ���ܵ���0
 * 
 ****************************************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include "lcd.h"
#include "touch.h"
#include "delay.h"

/* MCU��XTP2046�������XTP2046�������� */
uint16_t tp_write_and_read_ad(uint8_t cmd_data)
{
    uint16_t rd_data = 0;
    
    /* ��ʼ״̬ */
    T_CLK(0);
    T_MOSI(0);
    T_CS(0);    /* ѡ�д���IC */
    
    /* MCU��XTP2046�������� */
    for(uint8_t i = 0; i < 8; i++ )
    {
        T_CLK(0);   /* MCU��ʼ׼������ */
        
        if (cmd_data & 0x80)    /* ����Ҫȡ���λ���ͣ�MSB */
        {
            T_MOSI(1);
        }
        else
        {
            T_MOSI(0);
        }
        delay_us(1);    /* MCU׼��������� */
        
        T_CLK(1);       /* MCU�������ݣ�XTP2046��ʼ��ȡ */
        delay_us(1);    /* XTP2046��ȡ������� */
        
        cmd_data <<= 1; /* ���θ�λ��Ϊ���λ�������´�ȡ���λ */
    }
    
    /* ����æ�ź� */
    T_CLK(0);
    delay_us(1);
    T_CLK(1);
    delay_us(1);
    
    /* MCU��ȡXTP2046�������� */
    for(uint8_t i = 0; i < 16; i++ )
    {
        T_CLK(0);   /* XTP2046��ʼ׼������ */
        delay_us(1);
        T_CLK(1);   /* MCU��ʼ��ȡ���� */
        
        rd_data <<= 1;  /* �ճ����λ���������ȡ�������� */
        rd_data |= T_MISO;  /* MCU��ȡ���� */
        delay_us(1);
    }

    /* ����״̬ */    
    T_CLK(0);   /* ���������� */
    T_CS(1);    /* ȡ��ѡ�д���IC */
    
    return  (rd_data >>= 4);
}


/**
 * @brief       ��������ʼ��
 */
void tp_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    
    T_PEN_GPIO_CLK_ENABLE();    /* T_PEN��ʱ��ʹ�� */
    T_CS_GPIO_CLK_ENABLE();     /* T_CS��ʱ��ʹ�� */
    T_MISO_GPIO_CLK_ENABLE();   /* T_MISO��ʱ��ʹ�� */
    T_MOSI_GPIO_CLK_ENABLE();   /* T_MOSI��ʱ��ʹ�� */
    T_CLK_GPIO_CLK_ENABLE();    /* T_CLK��ʱ��ʹ�� */

    gpio_init_struct.Pin = T_PEN_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                 /* ���� */
    gpio_init_struct.Pull = GPIO_PULLUP;                     /* ���� */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;           /* ���� */
    HAL_GPIO_Init(T_PEN_GPIO_PORT, &gpio_init_struct);       /* ��ʼ��T_PEN���� */

    gpio_init_struct.Pin = T_MISO_GPIO_PIN;
    HAL_GPIO_Init(T_MISO_GPIO_PORT, &gpio_init_struct);      /* ��ʼ��T_MISO���� */

    gpio_init_struct.Pin = T_MOSI_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;             /* ������� */
    gpio_init_struct.Pull = GPIO_PULLUP;                     /* ���� */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;           /* ���� */
    HAL_GPIO_Init(T_MOSI_GPIO_PORT, &gpio_init_struct);      /* ��ʼ��T_MOSI���� */

    gpio_init_struct.Pin = T_CLK_GPIO_PIN;
    HAL_GPIO_Init(T_CLK_GPIO_PORT, &gpio_init_struct);       /* ��ʼ��T_CLK���� */

    gpio_init_struct.Pin = T_CS_GPIO_PIN;
    HAL_GPIO_Init(T_CS_GPIO_PORT, &gpio_init_struct);        /* ��ʼ��T_CS���� */

}









