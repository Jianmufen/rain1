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

/*��̬ȫ�ֱ���*/
static char first_power = 1;
static int second_data = 0;						/*��ȡ��δ�ɹ��������ݵ�ʱ����*/
static int i = 0;											/*��ǰ���Ӷ�Ӧ��������*/
static volatile int j = 0;						/*δ�ɹ����͵ķ�����������Ӧ��������*/

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
			device.fail_down_flag = 0;		/*����δ���ͳɹ������ݵı�־*/
			//��ʼ��RTC
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
				rain.RAIN_1M++ ;/*��������*/
				rain.RAIN_1H++;/*Сʱ����*/
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
			
			 while(osSemaphoreWait(semaphore, 1)	==	osOK);					//�������ĵ�ʱ��������ź�������Ϊι�����ڸ�����������еģ�������ĵ����ź��������µ�һ��ι�����ܼ�ʱ������CPU��λ
			
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
										
												/*����Ҫ�ͽ���־��0*/
												if(device.down_number == 0)
												{
														device.fail_down_flag = 0;
												}
												if((device.down_flag == 1) && (device.link_flag == 1))							/*������һ�β�Ҫ����*/
												{
															DEBUG("��ʼ��һ�β�Ҫ\r\n");
															if(start_measure() == 0)				/*��ʼ��Ҫ����*/
															{
																		DEBUG("��ʼ��һ�β�Ҫ�ɹ���\r\n");
																		device.down_flag = 0;			/*������ɾ������־λ*/
																		device.down_t_flag = 1;		/*�����ɹ��ͽ���Ҫ��־��һ�����������ڲ�Ҫ������*/
															}
															else
															{
																		DEBUG("��ʼ��һ�β�Ҫʧ����\r\n");
																		device.down_t_flag = 0;		/*����ʧ�ܼ����ڻ�û�п�ʼ��Ҫ���ݵ�״̬*/
															}
												}
												if((sys_time.Minutes==25) && (sys_time.Seconds==15))	/*ͬ��ʱ��*/
												{
															sync_time();
												}
												if(sys_time.Seconds == 0)
												{
															rain.RAIN_M[sys_time.Minutes] =  rain.RAIN_1M;
													
															/*����������*/
															memset(real_data, 0, sizeof(real_data));
															OutputHourDataFill(real_data, &sys_date, &sys_time);
													
															rain.RAIN_1M = 0;                                            /*������һ���ӵ�����*/
															first_power = 0;
															
															//������������֮��ÿСʱ����ʼ����������飨�洢60������������ֵ��
															if(sys_time.Minutes == 0)			
															{
																	for(i = 0; i < 60; i++)
																	{
																			rain.RAIN_M[i] = 0;
																	}
															}
															
															/*��д�벻ȷ���Ƿ��ͳɹ��ķ������ݣ���real_data[0] ='1'*/
															i = CalculateSector(&sys_date, &sys_time);
															DEBUG("1Sectors=%d\r\n", i);
															//û�в���SD��������SD������д���ݿ�������������������������дSD��֮ǰ���ж�SD���Ƿ����
															if(SD_STATUS_OK == 0)
															{
																	if(MSD_OK == BSP_SD_WriteBlocks((uint32_t *)real_data, i * 512, 512, 1))			//һ��ֻдһ������
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
												
												if((device.data_t == 1) && (sys_time.Seconds == 15) && (first_power == 0) && (device.link_flag == 0))			/*��һ�η��ͷ�������*/
												{
															printf("%s", real_data + 2);
												}
												
												if((device.data_t == 1) && (sys_time.Seconds == 30) && (first_power == 0) && (device.link_flag == 0))			/*�ڶ��η��ͷ�������*/
												{
															printf("%s", real_data + 2);
												}
												
												if((device.data_t == 1) && (sys_time.Seconds == 45) && (first_power == 0) && (device.link_flag == 0))			/*�����η��ͷ�������*/
												{
															printf("%s", real_data + 2);
												}
												
												if((device.data_t == 1) && (sys_time.Seconds == 59) && (first_power == 0) && (device.link_flag == 0))			/*�����η��ͷ���������Ȼû�н��յ���Ӧ*/	
												{
															device.link_flag	 = 0;				/*δ������վ������ȷ������*/
															device.minute_numbers++;			/*��������δ���ͳɾͼ�1*/
															if(device.minute_numbers > 21600)			/*21600��Ӧ���15��ķ������ݣ�ֻ��֤���15��ķ�������*/
															{
																		device.minute_numbers = 21600;
															}
												}
												
												if((first_power == 0) && (device.success_flag == 1) && (strncmp(real_data + 13, success_time, 16) == 0) && (device.link_flag == 0) && (device.data_t == 1))/*ʵʱ��������*/
												{
															device.success_flag = 0;		/*���յ���ȷ��Ӧ��־*/
															device.link_flag = 1;		/*������ȷ���Ӿͽ���־��һ*/
															/*���ͷ������ݺ󣬽��յ�����վ����ȷ���յ���Ӧ֮���ٽ���������д��SD������real_data[0] = '0'*/
															real_data[0] = '0';
															//printf("real_data=%s\r\n", real_data);
															if(SD_STATUS_OK == 0)
															{
																	if(MSD_OK == BSP_SD_WriteBlocks((uint32_t *)real_data, i * 512, 512, 1))			//һ��ֻдһ������
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
												if((device.down_t_flag == 2) && (sys_time.Seconds == device.down_t_time) && (device.down_t_time >= 0) && (device.fail_down_flag == 1) && (device.link_flag == 1))	/*�ڶ��η��Ͳ�Ҫ����δ�ɹ�*/
												{
															device.down_t_flag = 3;
															printf("%s", down_data + 2);						/*15��֮��ڶ��η���*/
															device.down_t_time += 15;								/*�õ�������Ҫ���Ͳ�Ҫ���ݵ�ʱ��*/
															if(device.down_t_time > 59)
															{
																		device.down_t_time -= 59;
															}
												}
												if((device.down_t_flag == 3) && (sys_time.Seconds == device.down_t_time) && (device.down_t_time >= 0) && (device.fail_down_flag == 1) && (device.link_flag == 1))	/*�����η��Ͳ�Ҫ����δ�ɹ�*/
												{
															device.down_t_flag = 4;
															printf("%s", down_data + 2);						/*�õ������η��ͺ�ĵ�15���ʱ��*/
															device.down_t_time += 15;
															if(device.down_t_time > 59)
															{
																		device.down_t_time -= 59;
															}
												}
												if((device.down_t_flag == 4) && (sys_time.Seconds == device.down_t_time) && (device.down_t_time >= 0) && (device.fail_down_flag == 1))	/*�����η���֮���15�뻹û�н��յ���Ӧ*/
												{
															device.down_t_flag = 0;					/*��Ҫ����ʧ�ܾ������벹Ҫ��ص����б���*/
															device.down_t_time = -2;				/*��Ҫ���ݷ���ʧ��*/
															device.down_t_flag = 2;					/*��Ҫ���ݷ���ʧ�ܱ�־*/
															device.down_number = 0;
															device.data_flag = 0;
															device.sectors_down = 0;
												}
												if((device.link_flag == 1) && (device.minute_numbers != 0) && (second_data == 0) && (device.fail_down_flag == 0))/*�����ǰ��һ����������վ���������ӣ��ʹ���δ�ɹ����͵ķ�������,real_data��i�Ϳ���ʹ����*/
												{
															if((i -21600) >= 0)
															{
																		for(j = i -1; j > i -21600; j--)	/*i-1����ǰ���ӵ�ǰһ�����������洢�������ţ�i-21600������뵱ǰʱ��Ϊ15�����һ�����������洢��������*/
																		{
																					if(SD_STATUS_OK == 0)
																					{
																								memset(real_data, 0, sizeof(real_data));
																								if(MSD_OK == BSP_SD_ReadBlocks((uint32_t *)real_data, j * 512, 512, 1))
																								{
																											if(real_data[0] == '1')		/*�ҵ�δ�ɹ����͵ķ�������*/
																											{
																														second_data = sys_time.Seconds;		/*�õ���ǰ��ʱ��*/
																														second_data += 15;								/*��һ�η��ͼ�15��*/
																														printf("%s", real_data + 2);		/*���ͷ�������*/
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
																											if(real_data[0] == '1')		/*�ҵ�δ�ɹ����͵ķ�������*/
																											{
																														second_data = sys_time.Seconds;		/*�õ���ǰ��ʱ��*/
																														second_data += 15;								/*��һ�η��ͼ�15��*/
																														printf("%s", real_data + 2);		/*���ͷ�������*/
																														break;
																											}
																								}
																					}
																		}
																		
																		for(j = 527040; j > 505440 + i; j--)	/*527040��12��31��23��59�ֵķ����������洢��������*/
																		{
																					if(SD_STATUS_OK == 0)
																					{
																								if(MSD_OK == BSP_SD_ReadBlocks((uint32_t *)real_data, j * 512, 512, 1))
																								{
																											if(real_data[0] == '1')		/*�ҵ�δ�ɹ����͵ķ�������*/
																											{
																														second_data = sys_time.Seconds;		/*�õ���ǰ��ʱ��*/
																														second_data += 15;								/*��һ�η��ͼ�15��*/
																														printf("%s", real_data + 2);		/*���ͷ�������*/
																														break;
																											}
																								}
																					}
																		}
															}
															
												}
												
												if((first_power == 0) && (device.link_flag == 1) && (device.success_flag == 1) && (strncmp(real_data + 13, success_time, 16) == 0) && (device.data_t == 1) && (device.fail_down_flag == 0))/*δ���ͳɹ������ݷ��ͳɹ�*/
												{
															second_data = 0;
															device.success_flag = 0;
															device.minute_numbers -= 1;			/*�ɹ��ͽ�����ֵ��һ*/
															if(device.minute_numbers <= 0)
															{
																		device.minute_numbers = 0;
															}
															
															real_data[0] = '0';
															if(SD_STATUS_OK == 0)
															{
																	if(MSD_OK == BSP_SD_WriteBlocks((uint32_t *)real_data, j * 512, 512, 1))			//һ��ֻдһ������
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
												if((sys_time.Seconds == second_data) && (second_data <= 59) && (first_power == 0) && (device.link_flag == 1) && (device.success_flag == 0) && (device.fail_down_flag == 0))		/*�ڶ��η���δ�ɹ����͵ķ�������*/
												{
																second_data += 15;
																printf("%s", real_data + 2);		/*���ͷ�������*/
												}
//												if((sys_time.Seconds == second_data) && (second_data <= 59) && (first_power == 0) && (device.link_flag == 1) && (device.success_flag == 0))		/*�����η���δ�ɹ����͵ķ�������*/
//												{
//																second_data += 15;
//																printf("%s", real_data + 2);		/*���ͷ�������*/
//												}
												if((second_data != 0) && (device.link_flag == 0) && (first_power == 0))
												{
															second_data = 0;
												}
												if(sys_time.Seconds == 59)				/*ÿ���ӵ�59�����㽨�����ӳɹ���־*/
												{
															device.link_flag = 0;				/*���������ӳɹ���־����*/
															device.data_t = 0;					/*����GPRS״̬��־*/
												}
												if((device.down_t_flag == 1) && (device.down_t_time == -1) && (device.fail_down_flag == 1))								/*��һ�η��Ͳ�Ҫ����δ�ɹ�*/
												{
															device.down_t_flag = 2;
															device.down_t_time = sys_time.Seconds;		/*�õ���һ�η��Ͳ�Ҫ���ݵ�ʱ��*/
															device.down_t_time += 15;									/*�õ��ڶ���Ҫ���Ͳ�Ҫ���ݵ�ʱ��*/
															if(device.down_t_time > 59)
															{
																		device.down_t_time -= 59;
															}
												}
												
												
												if(sys_time.Seconds == 30)				/*������·���ѹ*/
												{
															/* OnBoard Measurement ��·���ѹ����*/
															OnBoardMeasure();
													
												}
												
												if(sys_time.Seconds == 10)				/*��GPRS���������ͷ������ݱ�־*/
												{
															device.data_t = 0;
															printf("GPRS?\r\n");
												}
												if((sys_time.Seconds == 15) && (device.data_t == 0))	/*5��Ȳ���GPRS OK�Ͳ����ͷ�������ֱ�Ӽ���ֵ��1*/
												{
															device.minute_numbers++;			/*��������δ���ͳɾͼ�1*/
												}
												
												if((sys_time.Seconds == 1) && (sys_time.Minutes == 0))	/*����Сʱ����*/
												{
															rain.RAIN_1H = 0;
												}
												if(sys_time.Seconds == 3)
												{
															data_eeprom_write(TIME_ADDR, (uint8_t *)&device.minute_numbers, 4);			/*ÿ������CPU�ڲ�д��һ�μ���ֵ*/
												}
												/* Release mutex */
												osMutexRelease(mutex);
									}
									else
									{
												//DEBUG("û�еȵ������ź���\r\n");
									}
							}
						
							if(hiwdg.Instance)
							{
										HAL_IWDG_Refresh(&hiwdg);  /* refresh the IWDG */
										//DEBUG("ι����\r\n");
							}
				}
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
  
			/* Release the semaphore every 1 second */
		 if(semaphore!=NULL)
			{
						//DEBUG("����A�жϲ�����\r\n");
						osSemaphoreRelease(semaphore);
			}
}


/*��·���ѹ����*/
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
			
			HAL_ADC_Start(&hadc);  /* Start ADC Conversion ��ʼ������·���ѹ*/
			
			/* Wait for regular group conversion to be completed. �ȵ�������ת�����*/
			if(HAL_ADC_PollForConversion(&hadc,1000)==HAL_OK)
			{
				/*��ȡ��·��AD����ֵ*/
				device.board_AD = HAL_ADC_GetValue(&hadc);
				DEBUG("board_AD=%d\r\n", device.board_AD);
			}
			
			
			/*��·���ѹ*/
			device.board_voltage    = (((float)device.board_AD) / ((float)MAX_ADC1_VALUE)) * REF_ADC1_VOLTAGE;
			DEBUG("board_voltage1=%f\r\n", device.board_voltage);
			
			device.board_voltage    = (device.board_voltage) * R1_R2;
			DEBUG("board_voltage2=%f\r\n", device.board_voltage);
			
			device.board_voltage_10 = (uint32_t) (device.board_voltage * 10 + 0.5);
			DEBUG("board_voltage_10=%d\r\n", device.board_voltage_10);
			
			
//			/*���Ʋ�����ѹ���߻��߹���*/
//			if(device.board_voltage_10 > VI_UPPER_LIMIT)
//			{
//				device.board_voltage_10 = 145;
//			}
//			if(device.board_voltage_10 < VI_LOWER_LIMIT)
//			{
//				device.board_voltage_10 = 55;
//			}
}

