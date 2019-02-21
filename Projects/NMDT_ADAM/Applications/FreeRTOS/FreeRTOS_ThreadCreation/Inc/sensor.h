#ifndef __SENSOR_H
#define __SENSOR_H
#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32l1xx_hal.h"
#include "main.h"	 
#include "time.h"		 
	 

	 
//#define QC_R 0  //������ȷ
//#define QC_L 1  //����ȱ��	 
//#define TEMP_CORRECTION_VALUE    (0)  /*PT100���¶ȶ���ֵ*/
	 
//#define st(x)      do { x } while (__LINE__ == -1)
   
/* length of array */
#define LENGTH_OF(x)   (sizeof(x)/sizeof(*(x)))
	

extern char  time_buf[17];
extern char  success_time[17];
extern char  down_data[512];			/*��Ҫ����ר�û���*/
extern char  real_data[512];			/*ʵʱ��������ר�û���*/

typedef struct  //��������
{
	uint32_t RAIN_1M;               /*���������ۼ�ֵ*/
	int      RAIN_M[60];              /*һСʱ��60����������*/
	uint32_t RAIN_1H;               /*Сʱ�����ۼ�ֵ*/
}RAIN;
extern RAIN rain;


typedef struct /*�豸״̬*/
{
			uint32_t  board_AD;             /*��·�����AD*/
			float     board_voltage;        /*���ѹ*/
			uint32_t  board_voltage_10;     /*�����ĵ�·���ѹ��10��*/
			uint8_t 			usart_sampling;				/*����Զ�̲鿴����ֵ*/
			char station[6];									/*̨վ��*/
			char fail_down_flag;							/*δ���ͳɹ������ݺͲ�Ҫ����ֻ�ܷ���һ������Ҫ��ʱ���ܷ��ͷ���ʧ�ܵ����ݣ�*/
			char data_t;											/*�ɼ����������ݱ�־��0�ɼ������������ݣ�1�ɼ�����������*/
			char down_flag;										/*��Ҫ���ݱ�־��*/
			char data_flag;										/*1����������ݲ�Ҫ��2����Сʱ���ݲ�Ҫ��0��������*/
			char success_flag;								/*0������ʧ�ܣ�1�����ͳɹ�*/
			char link_flag;										/*���ӱ�־��1����ɹ���0����ʧ�ܣ��ɹ��������Ӽ����ͷ������ݽ��յ�����վ��ȷ����Ӧ*/
			char down_t_flag;									/*0���������Ҫ����Ҫ����δ��ʼ�����Ѿ�������,1����Ҫ���ݵ�һ�η��ͽ�����2����Ҫ���ݵڶ��η��ͽ�����3����Ҫ���ݵ����η��ͽ�����4����Ҫ���ݷ���ʧ��*/
			int  down_t_time;									/*��Ҫ���ݷ���ʱ�䣬-1����ս�����һ�η��Ͳ�Ҫ���ݣ�-2����Ҫ�������*/
			unsigned long sectors_down;				/*��Ҫ������ʼ������*/
			unsigned int down_number;					/*��Ҫ��������*/
			unsigned int minute_numbers;			/*δ�ɹ����͵ķ������ݸ���*/
}Device_State;
extern Device_State device;


/*��������*/
int OutputHourDataFill(char *Data, RTC_DateTypeDef *date, RTC_TimeTypeDef *time);
uint32_t CalculateSector(RTC_DateTypeDef *date, RTC_TimeTypeDef *time);

#ifdef __cplusplus
}
#endif
#endif
