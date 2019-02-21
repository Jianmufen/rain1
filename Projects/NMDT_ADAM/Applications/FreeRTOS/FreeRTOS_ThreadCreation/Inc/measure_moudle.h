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
	 
/*�¶Ȳ���*/
/*��׼����R0*/
#define R0_RESISTANCE             (100)	 
/*�¶Ȳ�����AD7705ͨ��*/
#define RT_AD7705_CH              AD7705_CH_AIN1P_AIN1M
//AD7705 Gain
/*�������������ѡ��*/	 
#define R0_AD7705_GAIN            AD7705_GAIN_1
#define RT_AD7705_GAIN            AD7705_GAIN_16
/*�������AD7705Ƭ�ڻ���������λѡ��*/	 
//AD7705 Buffer
#define R0_AD7705_BUFFER          AD7705_BUF_ENABLE  /*����������λΪ�ߵ�ƽ*/
#define RT_AD7705_BUFFER          AD7705_BUF_ENABLE	 



/*AD7705��ͨ��2*/
#define AD7705_CH2                (AD7705_CH_AIN2P_AIN2M)
//AD7705 Gain
#define AD7705_CH2_GAIN           (AD7705_GAIN_1)
//AD7705 Buffer
#define AD7705_CH2_BUFFER         AD7705_BUF_DISABLE
//VDD In Limitation ��ѹ����
#define VI_LOWER_LIMIT            (55)  /*���5.5V*/
#define VI_UPPER_LIMIT            (145) /*���14.5V*/
/*��Դ��ѹ��ѹ���豶ֵ*/
#define R1_R2												(6)


/* ADC Parameter */
#define REF_EXT_VOLTAGE           (1.25)    /* extern reference voltage �ⲿ�ο���ѹ1.25V*/
#define MAX_AD_VALUE              (65535)   /* maximum adc value */
/*��������*/
#define CURRENT_FLOW              (0.0002)


//����洢���ݵ�ַ�궨��
#define TIME_ADDR							  (FLASH_EEPROM_BASE + 4)					/*δ�ɹ����͵ķ������ݵļ���ֵ*/
#define STATION_ADDR					  (TIME_ADDR + 8)									/*̨վ��*/

extern uint8_t SD_STATUS_OK;








/*��������*/
int32_t init_measure_module(void);
int32_t start_measure(void);

#ifdef __cplusplus
}
#endif
#endif /*__MEASURE_MODULE_H */
