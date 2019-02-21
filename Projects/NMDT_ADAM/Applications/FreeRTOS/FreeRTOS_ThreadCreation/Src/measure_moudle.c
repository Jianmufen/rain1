/* Includes ------------------------------------------------------------------*/
#include "measure_moudle.h"
#include "cmsis_os.h"

#include "usart_module.h"
#include "sys_time_module.h"
#include "stm32_adafruit_sd.h"
#include "eeprom.h"

#define measureSTACK_SIZE   configMINIMAL_STACK_SIZE
#define measurePRIORITY     osPriorityNormal

  /* RTC Time*/
//static RTC_TimeTypeDef Measure_Time;
//static RTC_DateTypeDef Measure_Date; 
struct tm datetime;	

uint8_t SD_STATUS_OK = 0;					//0：SD卡初始化成功。1：SD卡初始化失败

/* os relative */
static osThreadId MeasureThreadHandle;
static osSemaphoreId semaphore;


/* Private function prototypes -----------------------------------------------*/
static void Measure_Thread(void const *argument);


/**
  * @brief  Init Measurement Module. 
  * @retval 0:success;-1:failed
  */
int32_t init_measure_module(void)
{
	uint8_t status = 0;
	//SD卡初始化
	status = BSP_SD_Init();
	if(status == MSD_OK)
	{
			DEBUG("SD Init OK\r\n");
			SD_STATUS_OK = 0;
	}
	else if(status == MSD_ERROR)
	{
			DEBUG("SD Init Error\r\n");
			SD_STATUS_OK = 1;
			if(BSP_SD_Init() == MSD_OK)
			{
					SD_STATUS_OK = 0;
					DEBUG("SD Init Second OK\r\n");
			}
	}
	
	/*记忆存储计数值*/
	if(data_eeprom_read(TIME_ADDR, (uint8_t *)&device.minute_numbers, 4) == HAL_OK)
	{
				DEBUG("data_eeprom_read=%d\r\n", device.minute_numbers);
				if(device.minute_numbers > 21600)
				{
							device.minute_numbers = 21600;
				}
				else if(device.minute_numbers <= 0)
				{
							device.minute_numbers = 0;
				}
	}
	else
	{
				device.minute_numbers = 21600;
				data_eeprom_write(TIME_ADDR, (uint8_t *)&device.minute_numbers, 4);
				DEBUG("data_eeprom_write=%d\r\n", device.minute_numbers);
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
	
	/* Create a thread to update system date and time */
  osThreadDef(Measure, Measure_Thread, measurePRIORITY, 0, measureSTACK_SIZE);
  MeasureThreadHandle=osThreadCreate(osThread(Measure), NULL);
  if(MeasureThreadHandle == NULL)
  {
    DEBUG("Create Measure Thread failed!\r\n");
    return -1;
  }
  
  
  return 0;
}

/**
  * @brief  Start a measurement. 
  * @retval 0:success;-1:failed
  */
int32_t start_measure(void)
{
  /* Release the semaphore */
  if(semaphore==NULL)
  {
    return -1;
  }
  
  if(osSemaphoreRelease(semaphore)!=osOK)
  {
    return -1;
  }
  
  return 0;
}

/*测量任务函数*/
static void Measure_Thread(void const *argument)
{
			volatile uint32_t i = 0;
			int k = 0;
			
			while(osSemaphoreWait(semaphore, 1)	==	osOK);					//消耗起始的信号量
			
			while(1)
			{
						/* Try to obtain the semaphore */
						if(osSemaphoreWait(semaphore,osWaitForever) == osOK)
						{
									if(device.data_flag == 1)				/*补要分钟数据*/
										{
													i = device.sectors_down * 512;
													memset(down_data, 0, sizeof(down_data));
													if(MSD_OK == BSP_SD_ReadBlocks((uint32_t *)down_data, i, 512, 1))
													{
																DEBUG("read data_buf=%s\r\n", down_data);
																DEBUG("     time_buf=%s\r\n", time_buf);
																if((strncmp(down_data + 2, "AHQX ", 5) == 0) && (strncmp(down_data + 13, time_buf, 16) == 0))			/*读到的是正确的数据*/
																{
																			printf("%s", down_data + 2);
																}
																else
																{
																			memset(down_data, 0, sizeof(down_data));
																			time_buf[16] = '\0';
																			k = snprintf(down_data, 31, "1 AHQX A0001 %s ", time_buf);
																			k += snprintf(down_data + k, 4, "W\r\n");
																			//down_data[k + 2] = '\0';
																			//down_data[33] = '\0';
																			printf("%s", down_data + 2);
																}
													}
													else																							/*读到的数据不正确*/
													{
																memset(down_data, 0, sizeof(down_data));
																time_buf[16] = '\0';
																k = snprintf(down_data, 31, "1 AHQX A0001 %s ", time_buf);
																k += snprintf(down_data + k, 4, "W\r\n");
																//down_data[k + 2] = '\0';
																//down_data[33] = '\0';
																printf("%s", down_data + 2);
													}
													device.down_t_flag = 1;			/*发送补要数据标志为1,0代表不发送补要数据*/
													device.down_t_time = -1;			/*第一次发送补要数据*/
										}
										else if(device.data_flag == 2)				/*补要小时数据*/
										{
																i = device.sectors_down * 512;
																memset(down_data, 0, sizeof(down_data));
																if(MSD_OK == BSP_SD_ReadBlocks((uint32_t *)down_data, i, 512, 1))
																{
																			DEBUG("read data_buf=%s\r\n", down_data);
																			if((strncmp(down_data+ 2, "AHQX ", 5) == 0) && (strncmp(down_data + 13, time_buf, 16) == 0))			/*读到的是正确的数据*/
																			{
																						printf("%s", down_data + 2);
																			}
																			else
																			{
																						time_buf[16] = '\0';
																						k = snprintf(down_data, 31, "1 AHQX A0001 %s ", time_buf);
																						k += snprintf(down_data + k, 4, "W\r\n");
																						//down_data[k + 1] = '\0';
																						//down_data[33] = '\0';
																						printf("%s", down_data + 2);
																			}
																}
																else
																{
																			time_buf[16] = '\0';
																			k = snprintf(down_data, 31, "1 AHQX A0001 %s ", time_buf);
																			k += snprintf(down_data + k, 4, "W\r\n");
																			//down_data[k + 1] = '\0';
																			//down_data[33] = '\0';
																			printf("%s", down_data + 2);
																}
																device.down_t_flag = 1;			/*发送补要数据标志为1,0代表不发送补要数据*/
																device.down_t_time = -1;			/*第一次发送补要数据*/
										}
										else
										{
													device.down_number = 0;
													device.data_flag = 0;
													device.down_t_flag = 0;			/*发送补要数据标志为1,0代表不发送补要数据*/
													device.down_t_time = -2;			/*第一次发送补要数据*/
										}
						}
			}
}









