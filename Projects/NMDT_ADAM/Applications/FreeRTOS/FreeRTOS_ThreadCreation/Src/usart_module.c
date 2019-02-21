/* Includes ------------------------------------------------------------------*/
#include "usart_module.h"
#include "cmsis_os.h"

#include "sys_time_module.h"
#include "measure_moudle.h"
#include "string.h"
#include "stdlib.h"
#include "stm32_adafruit_sd.h"
#include "eeprom.h"

/* Private define ------------------------------------------------------------*/
#define UART_RX_BUF_SIZE  (512) 	
#define storagePRIORITY     osPriorityNormal
#define storageSTACK_SIZE   (384)

/*全局变量*/
int year1 = 0, month1 = 0, day1 = 0, hour1 = 0, minute1 = 0;
static char b[8];

/* RTC Time通过串口设置时间*/
static RTC_TimeTypeDef Usart_Time;
static RTC_DateTypeDef Usart_Date;
		
static char rx1_buffer[UART_RX_BUF_SIZE]={0};  /* USART1 receiving buffer */
static uint32_t rx1_count=0;     /* receiving counter */
static uint8_t cr1_begin=false;        /* '\r'  received */ 
static uint8_t rx1_cplt=false;   /* received a frame of data ending with '\r'and'\n' */

/* os relative */
static osThreadId Usart1ThreadHandle;
static osSemaphoreId semaphore_usart1;
static void Usart1_Thread(void const *argument);

/**
  * @brief  Init Storage Module. 
  * @retval 0:success;-1:failed
  */
int32_t init_usart_module(void)
{
			USART1_UART_Init(9600);
			
			printf("Hello World!\r\n");
	
			ADC_Init();
			OnBoardMeasure();			/*刚开机就测量一次电路板电压*/
	
			if(data_eeprom_read(STATION_ADDR, (uint8_t *)&b, 8) == HAL_OK)
			{
						DEBUG("station=%s\r\n", device.station);
						if(b[0] < ' ')
						{
									device.station[0] = 'A';
									device.station[1] = '0';
									device.station[2] = '0';
									device.station[3] = '0';
									device.station[4] = '1';
									device.station[5] = '\0';
									b[0] = 'A';
									b[1] = '0';
									b[2] = '0';
									b[3] = '0';
									b[4] = '1';
									b[5] = '0';
									b[6] = '0';
									b[7] = '\0';
									data_eeprom_write(STATION_ADDR, (uint8_t *)&b, 8);
									DEBUG("station=%s\r\n", device.station);
						}
						device.station[0] = b[0];
						device.station[1] = b[1];
						device.station[2] = b[2];
						device.station[3] = b[3];
						device.station[4] = b[4];
						device.station[5] = '\0';
			}
			else
			{
						device.station[0] = 'A';
						device.station[1] = '0';
						device.station[2] = '0';
						device.station[3] = '0';
						device.station[4] = '1';
						device.station[5] = '\0';
						b[0] = 'A';
						b[1] = '0';
						b[2] = '0';
						b[3] = '0';
						b[4] = '1';
						b[5] = '0';
						b[6] = '0';
						b[7] = '\0';
						data_eeprom_write(STATION_ADDR, (uint8_t *)&b, 8);
						DEBUG("station=%s\r\n", device.station);
			}
	
			
			/* Define used semaphore 创建串口1的信号量*/
			osSemaphoreDef(SEM_USART1);
			/* Create the semaphore used by the two threads */
			semaphore_usart1=osSemaphoreCreate(osSemaphore(SEM_USART1), 1);
			if(semaphore_usart1 == NULL)
			{
				DEBUG("Create Semaphore_USART1 failed!");
				return -1;
			}
		 
			
			/* Create a thread to read historical data创建串口1处理存储数据任务 */
			osThreadDef(Usart1, Usart1_Thread, storagePRIORITY, 0, storageSTACK_SIZE);
			Usart1ThreadHandle=osThreadCreate(osThread(Usart1), NULL);
			if(Usart1ThreadHandle == NULL)
			{
				DEBUG("Create Usart1 Thread failed!");
				return -1;
			}
			
			return 0;
}


/*串口1处理数据任务*/
static void Usart1_Thread(void const *argument)
{
	unsigned long tatal_seconds2 = 0, tatal_seconds1 = 0;
	unsigned int year2 = 0, month2 = 0, day2 = 0, hour2 = 0, minute2 = 0;

	volatile uint32_t i = 0;
	volatile uint8_t down_number = 0;
	
	if(init_sys_time_module()<0)
  {
				DEBUG("init sys_time module failed!\r\n");
  }
  else
  {
				DEBUG("init sys_time module ok!\r\n");
  }
	
	if(init_measure_module()<0)
  {
				DEBUG("init measure module failed!\r\n");
  }
  else
  {
				DEBUG("init measure module ok!\r\n");
  }
	
	while(osSemaphoreWait(semaphore_usart1, 1)	==	osOK);			//CPU刚启动的时候，创建信号量的时候，就有可用的信号量，在这里先消耗掉该信号量。等待该信号量的时间尽可能短，这里写1代表一个时钟周期
	
	while(1)
	{
		
		if(osSemaphoreWait(semaphore_usart1,osWaitForever)==osOK)
		{
					
					//得到串口接收到命令时的时间
				get_sys_time(&Usart_Date, &Usart_Time);
			
				if((strcmp("OK", rx1_buffer) == 0) && (rx1_count == 2))
				{
							if((Usart_Time.Seconds < 15) && (Usart_Time.Seconds >= 10))
							{
										device.data_t = 1;
							}
//							else
//							{
//										device.data_t = 0;
//							}
				}
				else if((strcmp("ER", rx1_buffer) == 0)&&(rx1_count == 2))
				{
							device.data_t = 0;
				}
				else if((rx1_buffer[0] == 'X') && (rx1_count == 13))
				{
							device.success_flag = 1;							/*发送数据接收到正确的响应*/
							success_time[0] = rx1_buffer[1];			/*年*/
							success_time[1] = rx1_buffer[2];
							success_time[2] = rx1_buffer[3];
							success_time[3] = rx1_buffer[4];
							success_time[4] = '-';
							success_time[5] = rx1_buffer[5];			/*月*/
							success_time[6] = rx1_buffer[6];
							success_time[7] = '-';	
							success_time[8] = rx1_buffer[7];			/*日*/
							success_time[9] = rx1_buffer[8];
							success_time[10] = ' ';
							success_time[11] = rx1_buffer[9];			/*时*/
							success_time[12] = rx1_buffer[10];
							success_time[13] = ':';
							success_time[14] = rx1_buffer[11];		/*分*/
							success_time[15] = rx1_buffer[12];
							success_time[16] = '\0';
							DEBUG("success_time=%s\r\n", success_time);
							DEBUG("%d\r\n", strncmp(down_data + 13, success_time, 16));
							/*补要数据的发送*/
							if((device.down_t_flag != 1) && (device.data_flag != 0) && (strncmp(success_time, time_buf, 16) == 0) && (device.down_number > 0))
							{
										device.down_t_time = -1;					/*开启下一次补要数据*/
										device.down_t_flag = 1;						/*补要数据发送成功为0*/
								
										if(device.data_flag == 2)
										{
													AddaHour(&year1, &month1, &day1, &hour1, &minute1, 0);			/*加1小时*/
													device.sectors_down += 60;					/*扇区号自加60，整点扇区号相差60*/
										}
										if(device.data_flag == 1)
										{
													AddaMinute(&year1, &month1, &day1, &hour1, &minute1, 0);				/*加1分钟*/
													device.sectors_down += 1;					/*扇区号自加1*/
										}
										
										
										device.down_number -= 1;
										if(device.down_number <= 0)
										{
													device.data_flag = 0;
													device.down_number = 0;
										}
										
										memset(time_buf, 0, sizeof(time_buf));
										sprintf(time_buf, "20%02d-%02d-%02d %02d:%02d", year1, month1, day1, hour1, minute1);
								
										start_measure();			/*得到中心站的补要数据响应就释放补要任务函数的信号量*/
							}
				}
				else if((rx1_buffer[0] == 'W') &&(rx1_count == 13))
				{
							device.success_flag = 1;							/*发送数据接收到正确的响应*/
							success_time[0] = rx1_buffer[1];			/*年*/
							success_time[1] = rx1_buffer[2];
							success_time[2] = rx1_buffer[3];
							success_time[3] = rx1_buffer[4];
							success_time[4] = '-';
							success_time[5] = rx1_buffer[5];			/*月*/
							success_time[6] = rx1_buffer[6];
							success_time[7] = '-';	
							success_time[8] = rx1_buffer[7];			/*日*/
							success_time[9] = rx1_buffer[8];
							success_time[10] = ' ';
							success_time[11] = rx1_buffer[9];			/*时*/
							success_time[12] = rx1_buffer[10];
							success_time[13] = ':';
							success_time[14] = rx1_buffer[11];		/*分*/
							success_time[15] = rx1_buffer[12];
							success_time[16] = '\0';
							DEBUG("success_time=%s\r\n", success_time);
							DEBUG("%d\r\n", strncmp(down_data + 13, success_time, 16));
							/*补要数据的发送*/
							if((device.down_t_flag != 1) && (device.data_flag != 0) && (strncmp(success_time, time_buf, 16) == 0) && (device.down_number > 0))
							{
										device.down_t_time = -1;					/*开启下一次补要数据*/
										device.down_t_flag = 1;						/*补要数据发送成功为0*/
								
										if(device.data_flag == 2)
										{
													AddaHour(&year1, &month1, &day1, &hour1, &minute1, 0);			/*加1小时*/
													device.sectors_down += 60;					/*扇区号自加60，整点扇区号相差60*/
										}
										if(device.data_flag == 1)
										{
													AddaMinute(&year1, &month1, &day1, &hour1, &minute1, 0);				/*加1分钟*/
													device.sectors_down += 1;					/*扇区号自加1*/
										}
										
										
										device.down_number -= 1;
										if(device.down_number <= 0)
										{
													device.data_flag = 0;
													device.down_number = 0;
										}
										
										memset(time_buf, 0, sizeof(time_buf));
										sprintf(time_buf, "20%02d-%02d-%02d %02d:%02d", year1, month1, day1, hour1, minute1);
								
										start_measure();			/*得到中心站的补要数据响应就释放补要任务函数的信号量*/
							}
				}
				else if((rx1_buffer[0] == 'B') &&(rx1_count == 27))		/*对时*/
				{
							device.success_flag = 1;							/*发送数据接收到正确的响应*/
							success_time[0] = rx1_buffer[1];			/*年*/
							success_time[1] = rx1_buffer[2];
							success_time[2] = rx1_buffer[3];
							success_time[3] = rx1_buffer[4];
							success_time[4] = '-';
							success_time[5] = rx1_buffer[5];			/*月*/
							success_time[6] = rx1_buffer[6];
							success_time[7] = '-';	
							success_time[8] = rx1_buffer[7];			/*日*/
							success_time[9] = rx1_buffer[8];
							success_time[10] = ' ';
							success_time[11] = rx1_buffer[9];			/*时*/
							success_time[12] = rx1_buffer[10];
							success_time[13] = ' ';
							success_time[14] = rx1_buffer[11];		/*分*/
							success_time[15] = rx1_buffer[12];
							success_time[16] = '\0';
							DEBUG("success_time=%s\r\n", success_time);
					
							Usart_Time.Seconds     =   (rx1_buffer[25]-48)*10+rx1_buffer[26]-48;
							Usart_Time.Minutes     =   (rx1_buffer[23]-48)*10+rx1_buffer[24]-48;
							Usart_Time.Hours       =   (rx1_buffer[21]-48)*10+rx1_buffer[22]-48;
							Usart_Date.Date        =   (rx1_buffer[19]-48)*10+rx1_buffer[20]-48;
							Usart_Date.Month       =   (rx1_buffer[17]-48)*10+rx1_buffer[18]-48;
							Usart_Date.Year        =   (rx1_buffer[15]-48)*10+rx1_buffer[16]-48;
							if(PCF8563_Set_Time(Usart_Date.Year, Usart_Date.Month, Usart_Date.Date, Usart_Time.Hours, Usart_Time.Minutes, Usart_Time.Seconds) != HAL_OK)
							{
										DEBUG("Set Time Fail");
							}
							else
							{
										sync_time();
										DEBUG("Set Time Success");
							}	
				}
				else if((rx1_buffer[0] == 'C') &&(rx1_count == 37))	/*补要分钟数据*/
				{
							device.success_flag = 1;							/*发送数据接收到正确的响应*/
							device.fail_down_flag = 1;						/*补要就将这个标志置一*/
							success_time[0] = rx1_buffer[1];			/*年*/
							success_time[1] = rx1_buffer[2];
							success_time[2] = rx1_buffer[3];
							success_time[3] = rx1_buffer[4];
							success_time[4] = '-';
							success_time[5] = rx1_buffer[5];			/*月*/
							success_time[6] = rx1_buffer[6];
							success_time[7] = '-';	
							success_time[8] = rx1_buffer[7];			/*日*/
							success_time[9] = rx1_buffer[8];
							success_time[10] = ' ';
							success_time[11] = rx1_buffer[9];			/*时*/
							success_time[12] = rx1_buffer[10];
							success_time[13] = ':';
							success_time[14] = rx1_buffer[11];		/*分*/
							success_time[15] = rx1_buffer[12];
							success_time[16] = '\0';
							if(device.down_number == 0)
							{
										DEBUG("原来在这\r\n");
										device.down_flag = 1;
										device.data_flag = 1;
										/*补要起始时间*/
										year1      = (rx1_buffer[15]-48)*10+rx1_buffer[16]-48;
										month1     = (rx1_buffer[17]-48)*10+rx1_buffer[18]-48;
										day1    	 = (rx1_buffer[19]-48)*10+rx1_buffer[20]-48;
										hour1 	   = (rx1_buffer[21]-48)*10+rx1_buffer[22]-48;
										minute1    = (rx1_buffer[23]-48)*10+rx1_buffer[24]-48;
								
										/*补要终止时间*/
										year2      = (rx1_buffer[27]-48)*10+rx1_buffer[28]-48;
										month2     = (rx1_buffer[29]-48)*10+rx1_buffer[30]-48;
										day2    	 = (rx1_buffer[31]-48)*10+rx1_buffer[32]-48;
										hour2 	   = (rx1_buffer[33]-48)*10+rx1_buffer[34]-48;
										minute2    = (rx1_buffer[35]-48)*10+rx1_buffer[36]-48;
										
										tatal_seconds2 = l_mktime(year2, month2, day2, hour2, minute2, 0);		/*终止时间*/
										tatal_seconds1 = l_mktime(year1, month1, day1, hour1, minute1, 0);		/*起始时间*/
										
										/*补要分钟数据的条数*/
										if(tatal_seconds2 >= tatal_seconds1)
										{
													sprintf(time_buf, "20%02d-%02d-%02d %02d:%02d", year1, month1, day1, hour1, minute1);
													device.down_number = (tatal_seconds2 - tatal_seconds1) / 60 + 1;
										}
										else
										{
													device.down_number = 0;
										}
								
										Usart_Date.Year			= year1;
										Usart_Date.Month    = month1;
										Usart_Date.Date     = day1;
										Usart_Time.Hours    = hour1;
										Usart_Time.Minutes  = minute1;
										Usart_Time.Seconds	= 0;
										device.sectors_down = CalculateSector(&Usart_Date, &Usart_Time);
										
										DEBUG("down_number=%d sectors_down=%ld\r\n", device.down_number, device.sectors_down);
										DEBUG("start=20%02d-%02d-%02d %02d:%02d\r\n", year1, month1, day1, hour1, minute1);
										DEBUG("end  =20%02d-%02d-%02d %02d:%02d\r\n", year2, month2, day2, hour2, minute2);
							}
							else
							{
										DEBUG("在这呢\r\n");
										device.down_number = 0;
										device.down_flag = 1;			/*开始补要数据*/
										device.down_t_flag = 0;
										device.down_t_time = -2;
										device.data_flag = 1;			/*补要分钟数据*/
										/*补要起始时间*/
										year1      = (rx1_buffer[15]-48)*10+rx1_buffer[16]-48;
										month1     = (rx1_buffer[17]-48)*10+rx1_buffer[18]-48;
										day1    	 = (rx1_buffer[19]-48)*10+rx1_buffer[20]-48;
										hour1 	   = (rx1_buffer[21]-48)*10+rx1_buffer[22]-48;
										minute1    = (rx1_buffer[23]-48)*10+rx1_buffer[24]-48;
								
										/*补要终止时间*/
										year2      = (rx1_buffer[27]-48)*10+rx1_buffer[28]-48;
										month2     = (rx1_buffer[29]-48)*10+rx1_buffer[30]-48;
										day2    	 = (rx1_buffer[31]-48)*10+rx1_buffer[32]-48;
										hour2 	   = (rx1_buffer[33]-48)*10+rx1_buffer[34]-48;
										minute2    = (rx1_buffer[35]-48)*10+rx1_buffer[36]-48;
										
										tatal_seconds2 = l_mktime(year2, month2, day2, hour2, minute2, 0);		/*终止时间*/
										tatal_seconds1 = l_mktime(year1, month1, day1, hour1, minute1, 0);		/*起始时间*/
										
										/*补要分钟数据的条数*/
										if(tatal_seconds2 >= tatal_seconds1)
										{
													sprintf(time_buf, "20%02d-%02d-%02d %02d:%02d", year1, month1, day1, hour1, minute1);
													device.down_number = (tatal_seconds2 - tatal_seconds1) / 60 + 1;
										}
										else
										{
													device.down_number = 0;
										}
								
										Usart_Date.Year			= year1;
										Usart_Date.Month    = month1;
										Usart_Date.Date     = day1;
										Usart_Time.Hours    = hour1;
										Usart_Time.Minutes  = minute1;
										Usart_Time.Seconds	= 0;
										device.sectors_down = CalculateSector(&Usart_Date, &Usart_Time);
										
										DEBUG("down_number=%d sectors_down=%ld\r\n", device.down_number, device.sectors_down);
										DEBUG("start=20%02d-%02d-%02d %02d:%02d\r\n", year1, month1, day1, hour1, minute1);
										DEBUG("end  =20%02d-%02d-%02d %02d:%02d\r\n", year2, month2, day2, hour2, minute2);
							}
				}
				else if((rx1_buffer[0] == 'D') &&(rx1_count == 37))	/*补要小时数据*/
				{
							device.success_flag = 1;							/*发送数据接收到正确的响应*/
							device.fail_down_flag = 1;						/*补要就将这个标志置一*/
							success_time[0] = rx1_buffer[1];			/*年*/
							success_time[1] = rx1_buffer[2];
							success_time[2] = rx1_buffer[3];
							success_time[3] = rx1_buffer[4];
							success_time[4] = '-';
							success_time[5] = rx1_buffer[5];			/*月*/
							success_time[6] = rx1_buffer[6];
							success_time[7] = '-';	
							success_time[8] = rx1_buffer[7];			/*日*/
							success_time[9] = rx1_buffer[8];
							success_time[10] = ' ';
							success_time[11] = rx1_buffer[9];			/*时*/
							success_time[12] = rx1_buffer[10];
							success_time[13] = ':';
							success_time[14] = rx1_buffer[11];		/*分*/
							success_time[15] = rx1_buffer[12];
							success_time[16] = '\0';
							if(device.down_number == 0)
							{
										device.down_flag = 1;			/*开始补要数据*/
										device.data_flag = 2;			/*补要小时数据*/
										/*补要起始时间*/
										year1      = (rx1_buffer[15]-48)*10+rx1_buffer[16]-48;
										month1     = (rx1_buffer[17]-48)*10+rx1_buffer[18]-48;
										day1    	 = (rx1_buffer[19]-48)*10+rx1_buffer[20]-48;
										hour1 	   = (rx1_buffer[21]-48)*10+rx1_buffer[22]-48;
										minute1    = (rx1_buffer[23]-48)*10+rx1_buffer[24]-48;
								
										/*补要终止时间*/
										year2      = (rx1_buffer[27]-48)*10+rx1_buffer[28]-48;
										month2     = (rx1_buffer[29]-48)*10+rx1_buffer[30]-48;
										day2    	 = (rx1_buffer[31]-48)*10+rx1_buffer[32]-48;
										hour2 	   = (rx1_buffer[33]-48)*10+rx1_buffer[34]-48;
										minute2    = (rx1_buffer[35]-48)*10+rx1_buffer[36]-48;
										
										tatal_seconds2 = l_mktime(year2, month2, day2, hour2, minute2, 0);		/*终止时间*/
										tatal_seconds1 = l_mktime(year1, month1, day1, hour1, minute1, 0);		/*起始时间*/
										
										if(minute1 == 0)
										{
												
										}
										else
										{
												minute1 = 0;
												AddaHour(&year1, &month1, &day1, &hour1, 0, 0);
										}
										
										/*补要小时数据的条数*/
										if(tatal_seconds2 >= tatal_seconds1)
										{
													sprintf(time_buf, "20%02d-%02d-%02d %02d:%02d", year1, month1, day1, hour1, minute1);
													device.down_number = (tatal_seconds2 - tatal_seconds1) / 3600 + 1;
										}
										else
										{
													device.down_number = 0;
										}

										Usart_Date.Year			= year1;
										Usart_Date.Month    = month1;
										Usart_Date.Date     = day1;
										Usart_Time.Hours    = hour1;
										Usart_Time.Minutes  = 0;
										Usart_Time.Seconds	= 0;
										device.sectors_down = CalculateSector(&Usart_Date, &Usart_Time);
										
										DEBUG("Minute Data Numbers=%d sectors_down=%ld\r\n", device.down_number, device.sectors_down);
										DEBUG("start=20%02d-%02d-%02d %02d:%02d\r\n", year1, month1, day1, hour1, minute1);
										DEBUG("end  =20%02d-%02d-%02d %02d:%02d\r\n", year2, month2, day2, hour2, minute2);
							}
							else
							{
										device.down_flag = 1;			/*开始补要数据*/
										device.data_flag = 2;			/*补要小时数据*/
										device.down_number = 0;
										device.down_t_flag = 0;
										device.down_t_time = -2;
										/*补要起始时间*/
										year1      = (rx1_buffer[15]-48)*10+rx1_buffer[16]-48;
										month1     = (rx1_buffer[17]-48)*10+rx1_buffer[18]-48;
										day1    	 = (rx1_buffer[19]-48)*10+rx1_buffer[20]-48;
										hour1 	   = (rx1_buffer[21]-48)*10+rx1_buffer[22]-48;
										minute1    = (rx1_buffer[23]-48)*10+rx1_buffer[24]-48;
								
										/*补要终止时间*/
										year2      = (rx1_buffer[27]-48)*10+rx1_buffer[28]-48;
										month2     = (rx1_buffer[29]-48)*10+rx1_buffer[30]-48;
										day2    	 = (rx1_buffer[31]-48)*10+rx1_buffer[32]-48;
										hour2 	   = (rx1_buffer[33]-48)*10+rx1_buffer[34]-48;
										minute2    = (rx1_buffer[35]-48)*10+rx1_buffer[36]-48;
										
										tatal_seconds2 = l_mktime(year2, month2, day2, hour2, minute2, 0);		/*终止时间*/
										tatal_seconds1 = l_mktime(year1, month1, day1, hour1, minute1, 0);		/*起始时间*/
										if(minute1 == 0)
										{
												
										}
										else
										{
												minute1 = 0;
												AddaHour(&year1, &month1, &day1, &hour1, 0, 0);
										}
										/*补要小时数据的条数*/
										if(tatal_seconds2 >= tatal_seconds1)
										{
													sprintf(time_buf, "20%02d-%02d-%02d %02d:%02d", year1, month1, day1, hour1, minute1);
													device.down_number = (tatal_seconds2 - tatal_seconds1) / 3600 + 1;
										}
										else
										{
													device.down_number = 0;
										}
								
										Usart_Date.Year			= year1;
										Usart_Date.Month    = month1;
										Usart_Date.Date     = day1;
										Usart_Time.Hours    = hour1;
										Usart_Time.Minutes  = 0;
										Usart_Time.Seconds	= 0;
										
										device.sectors_down = CalculateSector(&Usart_Date, &Usart_Time);
										
										DEBUG("Minute Data Numbers=%d sectors_down=%ld\r\n", device.down_number, device.sectors_down);
										DEBUG("start=20%02d-%02d-%02d %02d:%02d\r\n", year1, month1, day1, hour1, minute1);
										DEBUG("end  =20%02d-%02d-%02d %02d:%02d\r\n", year2, month2, day2, hour2, minute2);
							}
				}
				else if((rx1_buffer[0] == 'E') &&(rx1_count == 51))	/*对时，且补要分钟数据*/
				{
							device.success_flag = 1;							/*发送数据接收到正确的响应*/
							device.fail_down_flag = 1;						/*补要就将这个标志置一*/
							success_time[0] = rx1_buffer[1];			/*年*/
							success_time[1] = rx1_buffer[2];
							success_time[2] = rx1_buffer[3];
							success_time[3] = rx1_buffer[4];
							success_time[4] = '-';
							success_time[5] = rx1_buffer[5];			/*月*/
							success_time[6] = rx1_buffer[6];
							success_time[7] = '-';	
							success_time[8] = rx1_buffer[7];			/*日*/
							success_time[9] = rx1_buffer[8];
							success_time[10] = ' ';
							success_time[11] = rx1_buffer[9];			/*时*/
							success_time[12] = rx1_buffer[10];
							success_time[13] = ':';
							success_time[14] = rx1_buffer[11];		/*分*/
							success_time[15] = rx1_buffer[12];
							success_time[16] = '\0';
					
							Usart_Time.Seconds     =   (rx1_buffer[25]-48)*10+rx1_buffer[26]-48;
							Usart_Time.Minutes     =   (rx1_buffer[23]-48)*10+rx1_buffer[24]-48;
							Usart_Time.Hours       =   (rx1_buffer[21]-48)*10+rx1_buffer[22]-48;
							Usart_Date.Date        =   (rx1_buffer[19]-48)*10+rx1_buffer[20]-48;
							Usart_Date.Month       =   (rx1_buffer[17]-48)*10+rx1_buffer[18]-48;
							Usart_Date.Year        =   (rx1_buffer[15]-48)*10+rx1_buffer[16]-48;
							if(PCF8563_Set_Time(Usart_Date.Year, Usart_Date.Month, Usart_Date.Date, Usart_Time.Hours, Usart_Time.Minutes, Usart_Time.Seconds) != HAL_OK)
							{
										DEBUG("Set Time Fail");
							}
							else
							{
										sync_time();
										DEBUG("Set Time Success");
							}	
							
							if(device.down_number == 0)
							{
										device.down_flag = 1;
										device.data_flag = 1;
										/*补要起始时间*/
										year1      = (rx1_buffer[29]-48)*10+rx1_buffer[30]-48;
										month1     = (rx1_buffer[31]-48)*10+rx1_buffer[32]-48;
										day1    	 = (rx1_buffer[33]-48)*10+rx1_buffer[34]-48;
										hour1 	   = (rx1_buffer[35]-48)*10+rx1_buffer[36]-48;
										minute1    = (rx1_buffer[37]-48)*10+rx1_buffer[38]-48;
								
										/*补要终止时间*/
										year2      = (rx1_buffer[41]-48)*10+rx1_buffer[42]-48;
										month2     = (rx1_buffer[43]-48)*10+rx1_buffer[44]-48;
										day2    	 = (rx1_buffer[45]-48)*10+rx1_buffer[46]-48;
										hour2 	   = (rx1_buffer[47]-48)*10+rx1_buffer[48]-48;
										minute2    = (rx1_buffer[49]-48)*10+rx1_buffer[50]-48;
										
										tatal_seconds2 = l_mktime(year2, month2, day2, hour2, minute2, 0);		/*终止时间*/
										tatal_seconds1 = l_mktime(year1, month1, day1, hour1, minute1, 0);		/*起始时间*/
										
										/*补要分钟数据的条数*/
										device.down_number = (tatal_seconds2 - tatal_seconds1) / 60 + 1;
								
										Usart_Date.Year			= year1;
										Usart_Date.Month    = month1;
										Usart_Date.Date     = day1;
										Usart_Time.Hours    = hour1;
										Usart_Time.Minutes  = minute1;
										Usart_Time.Seconds	= 0;
										device.sectors_down = CalculateSector(&Usart_Date, &Usart_Time);
										
										DEBUG("Minute Data Numbers=%d sectors_down=%ld\r\n", device.down_number, device.sectors_down);
							}
							else
							{
										device.down_number = 0;
								
										osDelay(3);				/*挂起3秒，令上一次补要停止*/
								
										device.down_flag = 1;
										/*补要起始时间*/
										year1      = (rx1_buffer[29]-48)*10+rx1_buffer[30]-48;
										month1     = (rx1_buffer[31]-48)*10+rx1_buffer[32]-48;
										day1    	 = (rx1_buffer[33]-48)*10+rx1_buffer[34]-48;
										hour1 	   = (rx1_buffer[35]-48)*10+rx1_buffer[36]-48;
										minute1    = (rx1_buffer[37]-48)*10+rx1_buffer[38]-48;
								
										/*补要终止时间*/
										year2      = (rx1_buffer[41]-48)*10+rx1_buffer[42]-48;
										month2     = (rx1_buffer[43]-48)*10+rx1_buffer[44]-48;
										day2    	 = (rx1_buffer[45]-48)*10+rx1_buffer[46]-48;
										hour2 	   = (rx1_buffer[47]-48)*10+rx1_buffer[48]-48;
										minute2    = (rx1_buffer[49]-48)*10+rx1_buffer[50]-48;
										
										tatal_seconds2 = l_mktime(year2, month2, day2, hour2, minute2, 0);		/*终止时间*/
										tatal_seconds1 = l_mktime(year1, month1, day1, hour1, minute1, 0);		/*起始时间*/
										
										/*补要分钟数据的条数*/
										device.down_number = (tatal_seconds2 - tatal_seconds1) / 60 + 1;
								
										Usart_Date.Year			= year1;
										Usart_Date.Month    = month1;
										Usart_Date.Date     = day1;
										Usart_Time.Hours    = hour1;
										Usart_Time.Minutes  = minute1;
										Usart_Time.Seconds	= 0;
										device.sectors_down = CalculateSector(&Usart_Date, &Usart_Time);
										
										DEBUG("Minute Data Numbers=%d sectors_down=%ld\r\n", device.down_number, device.sectors_down);
							}
				}
				else if((rx1_buffer[0] == 'I') && (rx1_count == 18))	/*设置站号I201806150930J0000\r\n*/
				{
							device.success_flag = 1;							/*发送数据接收到正确的响应*/
							success_time[0] = rx1_buffer[1];			/*年*/
							success_time[1] = rx1_buffer[2];
							success_time[2] = rx1_buffer[3];
							success_time[3] = rx1_buffer[4];
							success_time[4] = '-';
							success_time[5] = rx1_buffer[5];			/*月*/
							success_time[6] = rx1_buffer[6];
							success_time[7] = '-';	
							success_time[8] = rx1_buffer[7];			/*日*/
							success_time[9] = rx1_buffer[8];
							success_time[10] = ' ';
							success_time[11] = rx1_buffer[9];			/*时*/
							success_time[12] = rx1_buffer[10];
							success_time[13] = ':';
							success_time[14] = rx1_buffer[11];		/*分*/
							success_time[15] = rx1_buffer[12];
							success_time[16] = '\0';
					
							device.station[0] = rx1_buffer[13];
							device.station[1] = rx1_buffer[14];
							device.station[2] = rx1_buffer[15];
							device.station[3] = rx1_buffer[16];
							device.station[4] = rx1_buffer[17];
							device.station[5] = '\0';
							b[0] = device.station[0];
							b[1] = device.station[1];
							b[2] = device.station[2];
							b[3] = device.station[3];
							b[4] = device.station[4];
							b[5] = '0';
							b[6] = '0';
							b[7] = '\0';
							data_eeprom_write(STATION_ADDR, (uint8_t *)&b, 8);
				}
				else if((rx1_buffer[0] == 'R') && (rx1_count == 13))	/*重启主采集器I201806150930\r\n*/
				{
							device.success_flag = 1;							/*发送数据接收到正确的响应*/
							success_time[0] = rx1_buffer[1];			/*年*/
							success_time[1] = rx1_buffer[2];
							success_time[2] = rx1_buffer[3];
							success_time[3] = rx1_buffer[4];
							success_time[4] = '-';
							success_time[5] = rx1_buffer[5];			/*月*/
							success_time[6] = rx1_buffer[6];
							success_time[7] = '-';	
							success_time[8] = rx1_buffer[7];			/*日*/
							success_time[9] = rx1_buffer[8];
							success_time[10] = ' ';
							success_time[11] = rx1_buffer[9];			/*时*/
							success_time[12] = rx1_buffer[10];
							success_time[13] = ':';
							success_time[14] = rx1_buffer[11];		/*分*/
							success_time[15] = rx1_buffer[12];
							success_time[16] = '\0';
					
							HAL_NVIC_SystemReset();						//重启CPU
				}
				else if((strlen(rx1_buffer) == 7) && (strcasecmp("restart", rx1_buffer) == 0))
				{
						HAL_NVIC_SystemReset();						//重启CPU
				}
				else if((strlen(rx1_buffer) == 7) && (strcasecmp("version", rx1_buffer) == 0))
				{
						printf("version:V1.0");
				}
				else if((strlen(rx1_buffer) == 4) && (strcasecmp("time", rx1_buffer) == 0))
				{
							device.success_flag = 1;							/*发送数据接收到正确的响应*/
							get_sys_time(&Usart_Date, &Usart_Time);
							printf("time:20%02d-%02d-%02d %02d:%02d:%02d\r\n", Usart_Date.Year , Usart_Date.Month , Usart_Date.Date , Usart_Time.Hours , Usart_Time.Minutes , Usart_Time.Seconds );
				}
				else
				{
						DEBUG("F\r\n");
						DEBUG("%s\r\n", rx1_buffer);
				}
				
				rx1_count	=	0;
				rx1_cplt	=	false;                                              /* clear the flag of receive */
				memset(rx1_buffer,0,sizeof(rx1_buffer));                      /* clear the register of receive */
		}
	}
}



/**串口1中断函数*/
void USART1_IRQHandler(void)
{
  UART_HandleTypeDef *huart=&huart1;
  uint32_t tmp_flag = 0, tmp_it_source = 0;

  tmp_flag = __HAL_UART_GET_FLAG(huart, UART_FLAG_PE);
  tmp_it_source = __HAL_UART_GET_IT_SOURCE(huart, UART_IT_PE);  
  /* UART parity error interrupt occurred ------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  { 
    huart->ErrorCode |= HAL_UART_ERROR_PE;
  }
  
  tmp_flag = __HAL_UART_GET_FLAG(huart, UART_FLAG_FE);
  tmp_it_source = __HAL_UART_GET_IT_SOURCE(huart, UART_IT_ERR);
  /* UART frame error interrupt occurred -------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  {
    huart->ErrorCode |= HAL_UART_ERROR_FE;
  }
  
  tmp_flag = __HAL_UART_GET_FLAG(huart, UART_FLAG_NE);
  /* UART noise error interrupt occurred -------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  {
    huart->ErrorCode |= HAL_UART_ERROR_NE;
  }
  
  tmp_flag = __HAL_UART_GET_FLAG(huart, UART_FLAG_ORE);
  /* UART Over-Run interrupt occurred ----------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  {
    huart->ErrorCode |= HAL_UART_ERROR_ORE;
  }
  
  tmp_flag = __HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE);
  tmp_it_source = __HAL_UART_GET_IT_SOURCE(huart, UART_IT_RXNE);
  /* UART in mode Receiver ---------------------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  {
    /*UART_Receive_IT(huart);*/
    uint8_t data=0;
  
    data=huart->Instance->DR;  /* the byte just received  */
    
		

		
		if(!rx1_cplt)
    {
      if(cr1_begin==true)  /* received '\r' */
      {
        cr1_begin=false;
        if(data=='\n')  /* received '\r' and '\n' */
        {
          /* Set transmission flag: trasfer complete*/
          rx1_cplt=true;
        }
        else
        {
          rx1_count=0;
        }
      }
      else
      {
        if(data=='\r')  /* get '\r' */
        {
          cr1_begin=true;
        }
        else  /* continue saving data */
        {
          rx1_buffer[rx1_count]=data;
          rx1_count++;
          if(rx1_count>UART_RX_BUF_SIZE-1)  /* rx buffer full */
          {
            /* Set transmission flag: trasfer complete*/
            rx1_cplt=true;
          } 
        }
       }
      }
		
    
  	 /* received a data frame 数据接收完成就释放互斥量*/
    if(rx1_cplt == true)
    {
      if(semaphore_usart1 != NULL)
      {
         /* Release mutex */
        osSemaphoreRelease(semaphore_usart1);
				DEBUG("USART1 receive semaphore_usart1\r\n");
      }
    }

   
    }
  
  
  tmp_flag = __HAL_UART_GET_FLAG(huart, UART_FLAG_TXE);
  tmp_it_source = __HAL_UART_GET_IT_SOURCE(huart, UART_IT_TXE);
  /* UART in mode Transmitter ------------------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  {
    /*UART_Transmit_IT(huart);*/
  }

  tmp_flag = __HAL_UART_GET_FLAG(huart, UART_FLAG_TC);
  tmp_it_source = __HAL_UART_GET_IT_SOURCE(huart, UART_IT_TC);
  /* UART in mode Transmitter end --------------------------------------------*/
  if((tmp_flag != RESET) && (tmp_it_source != RESET))
  {
    /*UART_EndTransmit_IT(huart);*/
  }  

  if(huart->ErrorCode != HAL_UART_ERROR_NONE)
  {
    /* Clear all the error flag at once */
    __HAL_UART_CLEAR_PEFLAG(huart);
    
    /* Set the UART state ready to be able to start again the process */
    huart->State = HAL_UART_STATE_READY;
    
    HAL_UART_ErrorCallback(huart);
  } 
}




