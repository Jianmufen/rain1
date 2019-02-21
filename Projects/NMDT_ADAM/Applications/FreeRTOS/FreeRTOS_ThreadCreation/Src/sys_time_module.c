/* Includes ------------------------------------------------------------------*/
#include "sys_time_module.h"
#include "cmsis_os.h"

#include "usart_module.h"
#include "measure_moudle.h"
#include "sensor.h"
#include "stm32_adafruit_sd.h"
#include "eeprom.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define systimeSTACK_SIZE   (384)
#define systimePRIORITY     osPriorityHigh

/*静态全局变量*/
static char first_power = 1;
static int second_data = 0;						/*读取到未成功发送数据的时间秒*/
static int i = 0;											/*当前分钟对应的扇区号*/
static volatile int j = 0;						/*未成功发送的分钟数据所对应的扇区号*/

/* RTC Time*/
static RTC_TimeTypeDef sys_time;
static RTC_DateTypeDef sys_date;

/* os relative */
static osThreadId SysTimeThreadHandle;
static osSemaphoreId semaphore;
static osMutexId mutex;
/* Private function prototypes -----------------------------------------------*/
static void SysTime_Thread(void const *argument);

/**
  * @brief  Init System Time Module. 
  * @retval 0:success;-1:failed
  */
int32_t init_sys_time_module(void)
{
			first_power = 1;
			device.fail_down_flag = 0;		/*发送未发送成功的数据的标志*/
			//初始化RTC
			MX_RTC_Init();
			
			 /* Init extern RTC PCF8563 */
			if(IIC_Init() == HAL_ERROR)
			{
				DEBUG("init pcf8563 failed!\r\n");
			}
			else
			{
				/* synchronize internal RTC with pcf8563 */
				sync_time();
			}

			
			/* Define used semaphore */
			osSemaphoreDef(SEM);
			/* Create the semaphore used by the two threads */
			semaphore=osSemaphoreCreate(osSemaphore(SEM), 1);
			if(semaphore == NULL)
			{
				DEBUG("Create Semaphore failed!\r\n");
				return -1;
			}
			
			/* Create the mutex */
			osMutexDef(Mutex);
			mutex=osMutexCreate(osMutex(Mutex));
			if(mutex == NULL)
			{
				DEBUG("Create Mutex failed!\r\n");
				return -1;
			}
			
			/* Create a thread to update system date and time */
			osThreadDef(SysTime, SysTime_Thread, systimePRIORITY, 0, systimeSTACK_SIZE);
			SysTimeThreadHandle=osThreadCreate(osThread(SysTime), NULL);
			if(SysTimeThreadHandle == NULL)
			{
				DEBUG("Create System Time Thread failed!\r\n");
				return -1;
			}
			return 0;
}

/**
  * @brief  get System Date and Time. 
  * @retval 0:success;-1:failed
  */
int32_t get_sys_time(RTC_DateTypeDef *sDate,RTC_TimeTypeDef *sTime)
{
  /* Wait until a Mutex becomes available */
  if(osMutexWait(mutex,500)==osOK)
  {
    if(sDate)
    {
      *sDate=sys_date;
    }
    if(sTime)
    {
      *sTime=sys_time;
    }
    
    /* Release mutex */
    osMutexRelease(mutex);
    
    return 0;
  }
  else
  {
    /* Time */
    if(sTime)
    {
      sTime->Seconds=0;
      sTime->Minutes=0;
      sTime->Hours=0;
    }
    /* Date */
    if(sDate)
    {
      sDate->Date=1;
      sDate->WeekDay=RTC_WEEKDAY_SUNDAY;
      sDate->Month=(uint8_t)RTC_Bcd2ToByte(RTC_MONTH_JANUARY);
      sDate->Year=0;
    }
    
    return -1;
  }
}

int32_t get_sys_time_tm(struct tm *DateTime)
{
  /* Wait until a Mutex becomes available */
  if(osMutexWait(mutex,500)==osOK)
  {
    if(DateTime)
    {
      DateTime->tm_year	=	sys_date.Year+2000;
      DateTime->tm_mon	=	sys_date.Month;
      DateTime->tm_mday	=	sys_date.Date;
      DateTime->tm_hour	=	sys_time.Hours;
      DateTime->tm_min	=	sys_time.Minutes;
      DateTime->tm_sec	=	sys_time.Seconds;
    }
    
    /* Release mutex */
    osMutexRelease(mutex);
    
    return 0;
  }
  else
  {
    if(DateTime)
    {
      DateTime->tm_year=2000;
      DateTime->tm_mon=0;
      DateTime->tm_mday=0;
      DateTime->tm_hour=0;
      DateTime->tm_min=0;
      DateTime->tm_sec=0;
    }
    
    return -1;
  }
}

int32_t set_sys_time(RTC_DateTypeDef *sDate,RTC_TimeTypeDef *sTime)
{
  int32_t res=0;
  
  /* Wait until a Mutex becomes available */
  if(osMutexWait(mutex,500)==osOK)
  {
    if(sDate)
    {
      sys_date=*sDate;
    }
    if(sTime)
    {
      sys_time=*sTime;
    }
    
    /* check param */
    if(IS_RTC_YEAR(sys_date.Year) && IS_RTC_MONTH(sys_date.Month) && IS_RTC_DATE(sys_date.Date) &&
       IS_RTC_HOUR24(sys_time.Hours) && IS_RTC_MINUTES(sys_time.Minutes) && IS_RTC_SECONDS(sys_time.Seconds))
    {
    
      if((HAL_RTC_SetDate(&hrtc,&sys_date,FORMAT_BIN)==HAL_OK)&&  /* internal RTC */
         (HAL_RTC_SetTime(&hrtc,&sys_time,FORMAT_BIN)==HAL_OK)&&
         (PCF8563_Set_Time(sys_date.Year,sys_date.Month,sys_date.Date,sys_time.Hours,sys_time.Minutes,sys_time.Seconds)==HAL_OK) )     /* PCF8563 */
      {
        res=0;
      }
      else
      {
        res=-1;
      }
    }
    else
    {
      res=-1;
    }
    
    /* Release mutex */
    osMutexRelease(mutex);
    
    return res;
  }
  else
  {
    return -1;
  }
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	 /* Disable Interrupt */
	HAL_NVIC_DisableIRQ((IRQn_Type)(EXTI9_5_IRQn));
	
	if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_6) == 0)
		{
				rain.RAIN_1M++ ;/*分钟雨量*/
				rain.RAIN_1H++;/*小时雨量*/
				DEBUG("rain numbers++\r\n");
		}
	else if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_6) == 1)
		{
				DEBUG("no rain\r\n");
		}
}
/**
  * @brief  System sys_time update
  * @param  thread not used
  * @retval None
  */
static void SysTime_Thread(void const *argument)
{
			//int i = 0;
			/* Init IWDG  */
			IWDG_Init();
			
			 while(osSemaphoreWait(semaphore, 1)	==	osOK);					//不能消耗掉时间任务的信号量，因为喂狗是在该任务里面进行的，如果消耗掉该信号量将导致第一次喂狗不能及时，导致CPU复位
			
			while(1)
			{
						/* Try to obtain the semaphore */
						if(osSemaphoreWait(semaphore,osWaitForever)==osOK)
						{
									/* Wait until a Mutex becomes available */
									if(osMutexWait(mutex,500)==osOK)
									{
												HAL_RTC_GetTime(&hrtc,&sys_time,FORMAT_BIN);
												HAL_RTC_GetDate(&hrtc,&sys_date,FORMAT_BIN);
												
												DEBUG("RTC:20%02d-%02d-%02d %02d:%02d:%02d\r\n",
															 sys_date.Year,sys_date.Month,sys_date.Date,
															 sys_time.Hours,sys_time.Minutes,sys_time.Seconds);
										
												DEBUG("data_t        =%d\r\n", device.data_t);
												DEBUG("first_power   =%d\r\n", first_power);
												DEBUG("link_flag     =%d\r\n", device.link_flag);
												DEBUG("success_flag  =%d\r\n", device.success_flag);
												DEBUG("second_data   =%d\r\n", second_data);
												DEBUG("down_t_flag   =%d\r\n", device.down_t_flag);
												DEBUG("down_t_time   =%d\r\n", device.down_t_time);
												DEBUG("down_number   =%d\r\n", device.down_number);
												DEBUG("sectors_down  =%ld\r\n", device.sectors_down);
												DEBUG("minute_numbers=%d\r\n", device.minute_numbers);
										
												/*不补要就将标志置0*/
												if(device.down_number == 0)
												{
														device.fail_down_flag = 0;
												}
												if((device.down_flag == 1) && (device.link_flag == 1))							/*启动第一次补要数据*/
												{
															DEBUG("开始第一次补要\r\n");
															if(start_measure() == 0)				/*开始补要数据*/
															{
																		DEBUG("开始第一次补要成功了\r\n");
																		device.down_flag = 0;			/*启动完成就清零标志位*/
																		device.down_t_flag = 1;		/*启动成功就将补要标志置一，即处于正在补要数据中*/
															}
															else
															{
																		DEBUG("开始第一次补要失败了\r\n");
																		device.down_t_flag = 0;		/*启动失败即处于还没有开始补要数据的状态*/
															}
												}
												if((sys_time.Minutes==25) && (sys_time.Seconds==15))	/*同步时间*/
												{
															sync_time();
												}
												if(sys_time.Seconds == 0)
												{
															rain.RAIN_M[sys_time.Minutes] =  rain.RAIN_1M;
													
															/*填充分钟数据*/
															memset(real_data, 0, sizeof(real_data));
															OutputHourDataFill(real_data, &sys_date, &sys_time);
													
															rain.RAIN_1M = 0;                                            /*清零上一分钟的雨量*/
															first_power = 0;
															
															//填充完分钟数据之后，每小时的起始清空雨量数组（存储60个分钟雨量的值）
															if(sys_time.Minutes == 0)			
															{
																	for(i = 0; i < 60; i++)
																	{
																			rain.RAIN_M[i] = 0;
																	}
															}
															
															/*先写入不确定是否发送成功的分钟数据，即real_data[0] ='1'*/
															i = CalculateSector(&sys_date, &sys_time);
															DEBUG("1Sectors=%d\r\n", i);
															//没有插入SD卡导致向SD卡里面写数据卡死（测量任务函数崩溃），故写SD卡之前先判断SD卡是否插入
															if(SD_STATUS_OK == 0)
															{
																	if(MSD_OK == BSP_SD_WriteBlocks((uint32_t *)real_data, i * 512, 512, 1))			//一次只写一个扇区
																	{
																			DEBUG("write data to SD Card OK\r\n");
																	}
																	else
																	{
																			DEBUG("write data to SD Card error\r\n");
																			BSP_SD_Init();
																	}
															}
												}
												
												if((device.data_t == 1) && (sys_time.Seconds == 15) && (first_power == 0) && (device.link_flag == 0))			/*第一次发送分钟数据*/
												{
															printf("%s", real_data + 2);
												}
												
												if((device.data_t == 1) && (sys_time.Seconds == 30) && (first_power == 0) && (device.link_flag == 0))			/*第二次发送分钟数据*/
												{
															printf("%s", real_data + 2);
												}
												
												if((device.data_t == 1) && (sys_time.Seconds == 45) && (first_power == 0) && (device.link_flag == 0))			/*第三次发送分钟数据*/
												{
															printf("%s", real_data + 2);
												}
												
												if((device.data_t == 1) && (sys_time.Seconds == 59) && (first_power == 0) && (device.link_flag == 0))			/*第三次发送分钟数据任然没有接收到响应*/	
												{
															device.link_flag	 = 0;				/*未与中心站建立正确的链接*/
															device.minute_numbers++;			/*分钟数据未发送成就加1*/
															if(device.minute_numbers > 21600)			/*21600对应最近15天的分钟数据，只保证最近15天的分钟数据*/
															{
																		device.minute_numbers = 21600;
															}
												}
												
												if((first_power == 0) && (device.success_flag == 1) && (strncmp(real_data + 13, success_time, 16) == 0) && (device.link_flag == 0) && (device.data_t == 1))/*实时分钟数据*/
												{
															device.success_flag = 0;		/*接收到正确响应标志*/
															device.link_flag = 1;		/*建立正确链接就将标志置一*/
															/*发送分钟数据后，接收到中心站的正确接收的响应之后，再将分钟数据写入SD卡，即real_data[0] = '0'*/
															real_data[0] = '0';
															//printf("real_data=%s\r\n", real_data);
															if(SD_STATUS_OK == 0)
															{
																	if(MSD_OK == BSP_SD_WriteBlocks((uint32_t *)real_data, i * 512, 512, 1))			//一次只写一个扇区
																	{
																			DEBUG("write data to SD Card OK\r\n");
																			//HAL_UART_Transmit(&huart1, (uint8_t *)data_buf, sizeof(data_buf), 0xFF);
																	}
																	else
																	{
																			DEBUG("write data to SD Card error\r\n");
																			BSP_SD_Init();
																	}
															}
												}
												if((device.down_t_flag == 2) && (sys_time.Seconds == device.down_t_time) && (device.down_t_time >= 0) && (device.fail_down_flag == 1) && (device.link_flag == 1))	/*第二次发送补要数据未成功*/
												{
															device.down_t_flag = 3;
															printf("%s", down_data + 2);						/*15秒之后第二次发送*/
															device.down_t_time += 15;								/*得到第三次要发送补要数据的时间*/
															if(device.down_t_time > 59)
															{
																		device.down_t_time -= 59;
															}
												}
												if((device.down_t_flag == 3) && (sys_time.Seconds == device.down_t_time) && (device.down_t_time >= 0) && (device.fail_down_flag == 1) && (device.link_flag == 1))	/*第三次发送补要数据未成功*/
												{
															device.down_t_flag = 4;
															printf("%s", down_data + 2);						/*得到第三次发送后的第15秒的时间*/
															device.down_t_time += 15;
															if(device.down_t_time > 59)
															{
																		device.down_t_time -= 59;
															}
												}
												if((device.down_t_flag == 4) && (sys_time.Seconds == device.down_t_time) && (device.down_t_time >= 0) && (device.fail_down_flag == 1))	/*第三次发送之后的15秒还没有接收到响应*/
												{
															device.down_t_flag = 0;					/*补要数据失败就清零与补要相关的所有变量*/
															device.down_t_time = -2;				/*补要数据发送失败*/
															device.down_t_flag = 2;					/*补要数据发送失败标志*/
															device.down_number = 0;
															device.data_flag = 0;
															device.sectors_down = 0;
												}
												if((device.link_flag == 1) && (device.minute_numbers != 0) && (second_data == 0) && (device.fail_down_flag == 0))/*如果当前这一分钟与中心站建立了连接，就处理未成功发送的分钟数据,real_data和i就可以使用了*/
												{
															if((i -21600) >= 0)
															{
																		for(j = i -1; j > i -21600; j--)	/*i-1代表当前分钟的前一分钟数据所存储的扇区号，i-21600代表距离当前时间为15天的那一分钟数据所存储的扇区号*/
																		{
																					if(SD_STATUS_OK == 0)
																					{
																								memset(real_data, 0, sizeof(real_data));
																								if(MSD_OK == BSP_SD_ReadBlocks((uint32_t *)real_data, j * 512, 512, 1))
																								{
																											if(real_data[0] == '1')		/*找到未成功发送的分钟数据*/
																											{
																														second_data = sys_time.Seconds;		/*得到当前的时间*/
																														second_data += 15;								/*第一次发送加15秒*/
																														printf("%s", real_data + 2);		/*发送分钟数据*/
																														break;
																											}
																								}
																					}
																		}
															}
															else
															{
																		for(j = i -1; j > 0; j--)
																		{
																					if(SD_STATUS_OK == 0)
																					{
																								memset(real_data, 0, sizeof(real_data));
																								if(MSD_OK == BSP_SD_ReadBlocks((uint32_t *)real_data, j * 512, 512, 1))
																								{
																											if(real_data[0] == '1')		/*找到未成功发送的分钟数据*/
																											{
																														second_data = sys_time.Seconds;		/*得到当前的时间*/
																														second_data += 15;								/*第一次发送加15秒*/
																														printf("%s", real_data + 2);		/*发送分钟数据*/
																														break;
																											}
																								}
																					}
																		}
																		
																		for(j = 527040; j > 505440 + i; j--)	/*527040是12月31号23点59分的分钟数据所存储的扇区号*/
																		{
																					if(SD_STATUS_OK == 0)
																					{
																								if(MSD_OK == BSP_SD_ReadBlocks((uint32_t *)real_data, j * 512, 512, 1))
																								{
																											if(real_data[0] == '1')		/*找到未成功发送的分钟数据*/
																											{
																														second_data = sys_time.Seconds;		/*得到当前的时间*/
																														second_data += 15;								/*第一次发送加15秒*/
																														printf("%s", real_data + 2);		/*发送分钟数据*/
																														break;
																											}
																								}
																					}
																		}
															}
															
												}
												
												if((first_power == 0) && (device.link_flag == 1) && (device.success_flag == 1) && (strncmp(real_data + 13, success_time, 16) == 0) && (device.data_t == 1) && (device.fail_down_flag == 0))/*未发送成功的数据发送成功*/
												{
															second_data = 0;
															device.success_flag = 0;
															device.minute_numbers -= 1;			/*成功就将计数值减一*/
															if(device.minute_numbers <= 0)
															{
																		device.minute_numbers = 0;
															}
															
															real_data[0] = '0';
															if(SD_STATUS_OK == 0)
															{
																	if(MSD_OK == BSP_SD_WriteBlocks((uint32_t *)real_data, j * 512, 512, 1))			//一次只写一个扇区
																	{
																			DEBUG("write data1 to SD Card OK\r\n");
																	}
																	else
																	{
																			DEBUG("write data1 to SD Card error\r\n");
																			BSP_SD_Init();
																	}
															}
												}
												if((sys_time.Seconds == second_data) && (second_data <= 59) && (first_power == 0) && (device.link_flag == 1) && (device.success_flag == 0) && (device.fail_down_flag == 0))		/*第二次发送未成功发送的分钟数据*/
												{
																second_data += 15;
																printf("%s", real_data + 2);		/*发送分钟数据*/
												}
//												if((sys_time.Seconds == second_data) && (second_data <= 59) && (first_power == 0) && (device.link_flag == 1) && (device.success_flag == 0))		/*第三次发送未成功发送的分钟数据*/
//												{
//																second_data += 15;
//																printf("%s", real_data + 2);		/*发送分钟数据*/
//												}
												if((second_data != 0) && (device.link_flag == 0) && (first_power == 0))
												{
															second_data = 0;
												}
												if(sys_time.Seconds == 59)				/*每分钟的59秒清零建立连接成功标志*/
												{
															device.link_flag = 0;				/*将建立连接成功标志清零*/
															device.data_t = 0;					/*清零GPRS状态标志*/
												}
												if((device.down_t_flag == 1) && (device.down_t_time == -1) && (device.fail_down_flag == 1))								/*第一次发送补要数据未成功*/
												{
															device.down_t_flag = 2;
															device.down_t_time = sys_time.Seconds;		/*得到第一次发送补要数据的时间*/
															device.down_t_time += 15;									/*得到第二次要发送补要数据的时间*/
															if(device.down_t_time > 59)
															{
																		device.down_t_time -= 59;
															}
												}
												
												
												if(sys_time.Seconds == 30)				/*测量电路板电压*/
												{
															/* OnBoard Measurement 电路板电压测量*/
															OnBoardMeasure();
													
												}
												
												if(sys_time.Seconds == 10)				/*向GPRS发送请求发送分钟数据标志*/
												{
															device.data_t = 0;
															printf("GPRS?\r\n");
												}
												if((sys_time.Seconds == 15) && (device.data_t == 0))	/*5秒等不到GPRS OK就不发送分钟数据直接计数值加1*/
												{
															device.minute_numbers++;			/*分钟数据未发送成就加1*/
												}
												
												if((sys_time.Seconds == 1) && (sys_time.Minutes == 0))	/*清零小时雨量*/
												{
															rain.RAIN_1H = 0;
												}
												if(sys_time.Seconds == 3)
												{
															data_eeprom_write(TIME_ADDR, (uint8_t *)&device.minute_numbers, 4);			/*每分钟向CPU内部写入一次计数值*/
												}
												/* Release mutex */
												osMutexRelease(mutex);
									}
									else
									{
												//DEBUG("没有等到互斥信号量\r\n");
									}
							}
						
							if(hiwdg.Instance)
							{
										HAL_IWDG_Refresh(&hiwdg);  /* refresh the IWDG */
										//DEBUG("喂狗了\r\n");
							}
				}
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
  
			/* Release the semaphore every 1 second */
		 if(semaphore!=NULL)
			{
						//DEBUG("闹钟A中断产生了\r\n");
						osSemaphoreRelease(semaphore);
			}
}


/*电路板电压测量*/
void OnBoardMeasure(void)
{
			 /* use STM32 ADC Channel 1 to measure VDD */
			/**Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
				*/
			ADC_ChannelConfTypeDef sConfig={0};
			
			/* channel : ADC_CHANNEL_1 */
			sConfig.Channel=ADC_CHANNEL_9;
			sConfig.Rank=ADC_REGULAR_RANK_1;
			sConfig.SamplingTime=ADC_SAMPLETIME_96CYCLES;
			HAL_ADC_ConfigChannel(&hadc,&sConfig);
			
			HAL_ADC_Start(&hadc);  /* Start ADC Conversion 开始测量电路板电压*/
			
			/* Wait for regular group conversion to be completed. 等到规则组转换完成*/
			if(HAL_ADC_PollForConversion(&hadc,1000)==HAL_OK)
			{
				/*读取电路板AD测量值*/
				device.board_AD = HAL_ADC_GetValue(&hadc);
				DEBUG("board_AD=%d\r\n", device.board_AD);
			}
			
			
			/*电路板电压*/
			device.board_voltage    = (((float)device.board_AD) / ((float)MAX_ADC1_VALUE)) * REF_ADC1_VOLTAGE;
			DEBUG("board_voltage1=%f\r\n", device.board_voltage);
			
			device.board_voltage    = (device.board_voltage) * R1_R2;
			DEBUG("board_voltage2=%f\r\n", device.board_voltage);
			
			device.board_voltage_10 = (uint32_t) (device.board_voltage * 10 + 0.5);
			DEBUG("board_voltage_10=%d\r\n", device.board_voltage_10);
			
			
//			/*限制测量电压过高或者过低*/
//			if(device.board_voltage_10 > VI_UPPER_LIMIT)
//			{
//				device.board_voltage_10 = 145;
//			}
//			if(device.board_voltage_10 < VI_LOWER_LIMIT)
//			{
//				device.board_voltage_10 = 55;
//			}
}

