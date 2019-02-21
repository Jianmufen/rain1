/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_MODULE_H
#define __USART_MODULE_H
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l1xx_hal.h"
#include "main.h"
#include "time.h"
#include "usart.h"

extern int year1, month1, day1, hour1, minute1;
	 
//#define DEBUG_T_R
	 
#ifdef DEBUG_T_R
#define DEBUG(fmt, args...) 	printf(fmt, ##args)
#else
#define DEBUG(fmt, args...) 	do {} while (0)
#endif

int32_t init_usart_module(void);
	 
#ifdef __cplusplus
}
#endif
#endif /*__STORAGE_MODULE_H */
