#ifndef __MEASURE_MODULE_H
#define __MEASURE_MODULE_H
#ifdef __cplusplus
 extern "C" {
#endif
	 
/* Includes ------------------------------------------------------------------*/
#include "stm32l1xx_hal.h"
#include "main.h"
	 
#include "sensor.h"
#include "ad7705.h"	 
#include "adc.h"

extern struct tm datetime;	 
	 
/*温度测量*/
/*标准电阻R0*/
#define R0_RESISTANCE             (100)	 
/*温度测量的AD7705通道*/
#define RT_AD7705_CH              AD7705_CH_AIN1P_AIN1M
//AD7705 Gain
/*电阻测量的增益选择*/	 
#define R0_AD7705_GAIN            AD7705_GAIN_1
#define RT_AD7705_GAIN            AD7705_GAIN_16
/*电阻测量AD7705片内缓冲器控制位选择*/	 
//AD7705 Buffer
#define R0_AD7705_BUFFER          AD7705_BUF_ENABLE  /*缓冲器控制位为高电平*/
#define RT_AD7705_BUFFER          AD7705_BUF_ENABLE	 



/*AD7705的通道2*/
#define AD7705_CH2                (AD7705_CH_AIN2P_AIN2M)
//AD7705 Gain
#define AD7705_CH2_GAIN           (AD7705_GAIN_1)
//AD7705 Buffer
#define AD7705_CH2_BUFFER         AD7705_BUF_DISABLE
//VDD In Limitation 电压限制
#define VI_LOWER_LIMIT            (55)  /*最低5.5V*/
#define VI_UPPER_LIMIT            (145) /*最高14.5V*/
/*电源电压分压电阻倍值*/
#define R1_R2												(6)


/* ADC Parameter */
#define REF_EXT_VOLTAGE           (1.25)    /* extern reference voltage 外部参考电压1.25V*/
#define MAX_AD_VALUE              (65535)   /* maximum adc value */
/*恒流电流*/
#define CURRENT_FLOW              (0.0002)


//记忆存储数据地址宏定义
#define TIME_ADDR							  (FLASH_EEPROM_BASE + 4)					/*未成功发送的分钟数据的计数值*/
#define STATION_ADDR					  (TIME_ADDR + 8)									/*台站号*/

extern uint8_t SD_STATUS_OK;








/*函数声明*/
int32_t init_measure_module(void);
int32_t start_measure(void);

#ifdef __cplusplus
}
#endif
#endif /*__MEASURE_MODULE_H */
