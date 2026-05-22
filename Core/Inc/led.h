#ifndef __LED_H
#define __LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define LED_PORT       GPIOB
#define LED_PIN        GPIO_PIN_2
#define LED_BLINK_MS   500

void LED_Init(void);
void LED_On(void);
void LED_Off(void);
void LED_Toggle1(void);

#ifdef __cplusplus
}
#endif

#endif
