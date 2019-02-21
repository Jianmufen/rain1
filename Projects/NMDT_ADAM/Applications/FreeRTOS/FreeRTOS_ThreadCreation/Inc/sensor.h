#ifndef __SENSOR_H
#define __SENSOR_H
#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32l1xx_hal.h"
#include "main.h"	 
#include "time.h"		 
	 

	 
//#define QC_R 0  //数据正确
//#define QC_L 1  //数据缺测	 
//#define TEMP_CORRECTION_VALUE    (0)  /*PT100的温度订正值*/
	 
//#define st(x)      do { x } while (__LINE__ == -1)
   
/* length of array */
#define LENGTH_OF(x)   (sizeof(x)/sizeof(*(x)))
	

extern char  time_buf[17];
extern char  success_time[17];
extern char  down_data[512];			/*补要数据专用缓存*/
extern char  real_data[512];			/*实时分钟数据专用缓存*/

typedef struct  //雨量数据
{
	uint32_t RAIN_1M;               /*分钟雨量累计值*/
	int      RAIN_M[60];              /*一小时内60个分钟雨量*/
	uint32_t RAIN_1H;               /*小时雨量累计值*/
}RAIN;
extern RAIN rain;


typedef struct /*设备状态*/
{
			uint32_t  board_AD;             /*电路板测量AD*/
			float     board_voltage;        /*板电压*/
			uint32_t  board_voltage_10;     /*测量的电路板电压的10倍*/
			uint8_t 			usart_sampling;				/*串口远程查看采样值*/
			char station[6];									/*台站号*/
			char fail_down_flag;							/*未发送成功的数据和补要数据只能发送一个，补要的时候不能发送发送失败的数据，*/
			char data_t;											/*采集器发送数据标志，0采集器不发送数据，1采集器发送数据*/
			char down_flag;										/*补要数据标志，*/
			char data_flag;										/*1代表分钟数据补要，2代表小时数据补要，0代表其他*/
			char success_flag;								/*0代表发送失败，1代表发送成功*/
			char link_flag;										/*连接标志，1代表成功，0代表失败，成功建立连接即发送分钟数据接收到中心站正确的响应*/
			char down_t_flag;									/*0代表结束补要即补要数据未开始或者已经结束了,1代表补要数据第一次发送结束，2代表补要数据第二次发送结束，3代表补要数据第三次发送结束，4代表补要数据发送失败*/
			int  down_t_time;									/*补要数据发送时间，-1代表刚结束第一次发送补要数据，-2代表补要任务结束*/
			unsigned long sectors_down;				/*补要数据起始扇区号*/
			unsigned int down_number;					/*补要数据条数*/
			unsigned int minute_numbers;			/*未成功发送的分钟数据个数*/
}Device_State;
extern Device_State device;


/*函数声明*/
int OutputHourDataFill(char *Data, RTC_DateTypeDef *date, RTC_TimeTypeDef *time);
uint32_t CalculateSector(RTC_DateTypeDef *date, RTC_TimeTypeDef *time);

#ifdef __cplusplus
}
#endif
#endif
