/**
  ******************************************************************************
  * File Name          : sys_time_module.c
  * Description        : This file provides a module to store/read system data. 
  *                                         
  ******************************************************************************
  *
  * COPYRIGHT(c) 2015 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "storage_module.h"
#include "cmsis_os.h"

#include "sys_time_module.h"
#include "usart_module.h"
#include "measure_moudle.h"



/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define BUF_SIZE    (512)
#define NAME_SIZE   (64)

#define storageSTACK_SIZE   configMINIMAL_STACK_SIZE
#define storagePRIORITY     osPriorityNormal

/*将具有记忆功能的参数归整到一个uint32_t的数组里面*/
uint32_t para_data[3];

/* RTC Time通过串口设置时间*/
static RTC_TimeTypeDef Storage_Time;
static RTC_DateTypeDef Storage_Date;
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* SD Card */
static SD_CardInfo cardinfo;

/* FATFS file system */
static FATFS SDFatFs;  /* File system object for SD card logical drive */
static char SDPath[4]; /* SD card logical drive path */
/* File */
static FIL file;          /* File object structure (FIL) */


/* os relative */
static osThreadId StorageThreadHandle;
static osSemaphoreId semaphore;
//static osMutexId mutex_sd;

/* Private function prototypes -----------------------------------------------*/
static FRESULT init_directorys(void);
/*static FRESULT WriteFile(const uint8_t *Filename,uint32_t Offset,const uint8_t *pData,uint32_t Size,uint32_t *bw);
static FRESULT ReadFile(const uint8_t *Filename,uint32_t Offset,uint8_t *pData,uint32_t Size,uint32_t *br);*/
static void Storage_Thread(void const *argument);

/**
  * @brief  Init Storage Module. 
  * @retval 0:success;-1:failed
  */
int32_t init_storage_module(void)
{
  FRESULT  res    = FR_OK;
	uint32_t byte_i = 0;
  FATFS *fs;
  DWORD fre_clust, fre_sect, tot_sect;
	
  /* Init SD Card */
  if(BSP_SD_Init()!=MSD_OK)
  {
    printf("SD Card Init failed!\r\n");
    /*return -1;*/
  }
  else
  {
    printf("SD Card Init ok!\r\n");
    if(BSP_SD_GetCardInfo(&cardinfo)==MSD_OK)  /* get sd card info */
    {
      printf("SD Card Size: %u MB,Block Size:%u\r\n",
             cardinfo.CardCapacity>>20,
             cardinfo.CardBlockSize);
    }
    
    
    /* Important: must add SD_GET_STATUS_WORKAROUND to the compile's definition in case of BSP_SD_GetStatus() failed.
     * This is a temporary workaround for this issue: on some STM32 Nucleo boards 
     * reading the SD card status will return an error 
     */
    
    /* Init FatFs */
    /*##-1- Link the micro SD disk I/O driver ##################################*/
    if(FATFS_LinkDriver(&SD_Driver,SDPath)!=0)
    {
      printf("FATFS LinkDriver failed!\r\n");
      return -1;
    }
    
    /*##-2- Register the file system object to the FatFs module ##############*/
    if((res=f_mount(&SDFatFs,(const TCHAR *)SDPath,0))!=FR_OK)
    {
      printf("FatFs mount failed:%d\r\n",res);
      return -1;
    }
    //printf("FatFs mount success!\r\n");
    
    /* SD Card Info */
    /* Get volume information and free clusters of sd drive */
    res = f_getfree(SDPath, &fre_clust, &fs);
    if (res == FR_OK)
    {
      fs=&SDFatFs;
      /* Get total sectors and free sectors */
      tot_sect = (fs->n_fatent - 2) * fs->csize;
      fre_sect = fre_clust * fs->csize;
      
      /* Print the free space (assuming 512 bytes/sector) */
      //printf("SD Card:%lu MiB total drive space.%lu MiB available.\r\n",
      //        tot_sect >> 11, fre_sect >> 11);
    }
    
    /* init directory used */
    if(init_directorys()!=FR_OK)
    {
      printf("creat file failde \r\n");
    }
		else
		{
			printf("create file success\r\n");
		}
  
  }
	
	/*开机就读一次parameter文件，得到站号、轮询间隔、温度订正值*/
	res = f_open(&file,(const char *)"parameter", FA_OPEN_ALWAYS|FA_WRITE);
	if(res == FR_OK)
	{
		res=f_lseek(&file,0);
		if(res == FR_OK)
		{
			res=f_read(&file,&para_data,sizeof(para_data),&byte_i);
			if(res == FR_OK)
			{
				printf("para_data:%d\r\n",(unsigned int)para_data);
				device.station       = para_data[0];
				device.data_time     = para_data[1];
				temp.temp_correction_value = (float)(para_data[2])/10;
			}
		}
	}
	
  /* Define used semaphore */
  osSemaphoreDef(SEM);
  /* Create the semaphore used by the two threads */
  semaphore=osSemaphoreCreate(osSemaphore(SEM), 1);
  if(semaphore == NULL)
  {
    printf("Create Semaphore failed!\r\n");
    return -1;
  }
  
  
  /* Create a thread to read historical data */
  osThreadDef(Storage, Storage_Thread, storagePRIORITY, 0, storageSTACK_SIZE);
  StorageThreadHandle=osThreadCreate(osThread(Storage), NULL);
  if(StorageThreadHandle == NULL)
  {
    printf("Create Storage Thread failed!\r\n");
    return -1;
  }
	
  
  return 0;
}



  


/**
  * @brief  File/Directory Management Processing Command.
  * @param  str: Pointer to data buffer
  * @param  len: Length of the command
  * @retval 0:Command Matched;-1:Command not Matched
  */
int32_t Processing_FileManagement_Command(const uint8_t *str,uint32_t len)
{
  /* files & directorys */
  FRESULT res=FR_OK;
  //static FIL file;          /* File object structure (FIL) */
  /*static*/ char filename[NAME_SIZE]="stm32.txt";
  static uint8_t  rtext[BUF_SIZE];
  static uint32_t bytesread;
  
  static DIR dir;           /* Directory object structure (DIR) */
  /*static*/ char pathname[NAME_SIZE];
  /*static*/ FILINFO fileinfo;  /* File status structure (FILINFO) */
  
  /*static*/ RTC_TimeTypeDef time={0};
  
  int32_t ret=0;
  
  
  (void)get_sys_time(NULL,&time);
  if((time.Seconds<5) || (time.Seconds>55))
  {
    return -1;
  }
  
//  /* Wait until a Mutex becomes available */
//  if(osMutexWait(mutex_sd,_FS_TIMEOUT)!=osOK)
//  {
//    return -1;
//  }
  
  
  /* File/Directory Management */
  if(strncasecmp((char *)str,"cat ",4)==0)  /* view the contents of a file */
  {
    if(len<5)
    {
      printf("please specify a file name.\r\n");
    }
    else
    {
      snprintf(filename,sizeof(filename),"%.*s",len-4,str+4);
      printf("file \"%s\":\r\n",filename);
      
      /* read file */
      /*##-- Open the text file object with read access ###############*/
      res=f_open(&file,filename,FA_READ);
      
      if(res!=FR_OK)
      {
        printf("open file \"%s\" failed:%d\r\n",filename,res);
      }
      else
      {
        /*##-- Read data from the text file ###########################*/
        while(1)
        {
          res=f_read(&file,rtext,sizeof(rtext),&bytesread);
          
          if(res!=FR_OK)   /* Error */
          {
            printf("read file %s failed:%d\r\n",filename,res);
            break;
          }
          else if(bytesread==0)  /* End Of File */
          {
            printf("\r\n/********************EOF********************/\r\n");
            break;
          }
          else
          {
            //printf("read %u bytes from file %s:\r\n%s\r\n",bytesread,filename,rtext);
            /* print file contents */
            /*for(uint16_t i=0;i<sizeof(rtext);i++)
            {
              //printf("%c",rtext[i]);
              fputc(rtext[i],stdout);
            }*/
            HAL_UART_Transmit(&huart1,rtext,bytesread,5000);
          }
        }
        
        /*##-- Close the open text file #############################*/
        res=f_close(&file);
        if(res!=FR_OK)
        {
          printf("close file %s failed:%d\r\n",filename,res);
        }
      }
    }
  }
      
  /* Directory */
  else if(strncasecmp((char *)str,"ls",2)==0)  /* 忽略大小写比较字符串view files of directory */
  {
    if((len>3) && (*(str+2)==' '))  /* specify a path */
    {
      snprintf(pathname,sizeof(pathname),"%.*s",len-3,str+3);
    }
    else
    {
      /* get the current directory */
      res=f_getcwd(pathname,sizeof(pathname));
      
      if(res!=FR_OK)
      {
        printf("getcwd failed:%d\r\n",res);
        snprintf(pathname,sizeof(pathname),"/");  /* root directory */
      }
    }
    
    /* opens the directory */
    res=f_opendir(&dir,pathname);
    
    if(res!=FR_OK)
    {
      printf("open directory \"%s\" failed:%d\r\n",pathname,res);
    }
    else
    {
      /* print format */
      printf("PATH:%s\r\n",pathname);
      /**      Timestamp | Attr | Length | Name              **/
      printf("%-20s | %-5s | %-10s | %-15s\r\n","Timestamp","Mode","Length","Name");
      printf("%-20s | %-5s | %-10s | %-15s\r\n","---------","----","------","----");
      
      /* reads directory entries */
      while(1)
      {
        res = f_readdir(&dir, &fileinfo);                   /* Read a directory item */
        if (res != FR_OK || fileinfo.fname[0] == 0) break;  /* Break on error or end of dir */
        /*if (fileinfo.fname[0] == '.') continue; */            /* Ignore dot entry */            
        
        
        /* print directory info */
        /* time stamp */
        printf("%4u/%02u/%02u %4u:%02u   | ",
              (fileinfo.fdate>>9)+1980,     /* Year */
              (fileinfo.fdate>>5)&0x0F,     /* Month */
              (fileinfo.fdate)&0x1F,	/* Day */
              (fileinfo.ftime>>11),	        /* Hour */
              (fileinfo.ftime>>5)&0x3F);    /* Minute */ 
        /* File/Directory Attribute */
        printf("%c%c%c%c%c | ",
              (fileinfo.fattrib & AM_DIR) ? 'D' : '-',	  /* Directory */
              (fileinfo.fattrib & AM_RDO) ? 'R' : '-',	  /* Read-only File */
              (fileinfo.fattrib & AM_HID) ? 'H' : '-',	  /* Hidden File */
              (fileinfo.fattrib & AM_SYS) ? 'S' : '-',	  /* System File */
              (fileinfo.fattrib & AM_ARC) ? 'A' : '-');       /* Archive File */
        printf("%-10u | %-15s\r\n",
               (unsigned int)fileinfo.fsize,				  /* File Size */
               fileinfo.fname);				  /* File/Directory Name */
      
      }
      
      /* closes the directory */
      res=f_closedir(&dir);
      
      if(res!=FR_OK)
      {
        printf("close directory \"%s\" failed:%d\r\n",pathname,res);
      }
    }
  }
  else if(strncasecmp((char *)str,"cd ",3)==0)   /* change current directory */
  {
    if(len<4)
    {
      /* get the current directory */
      res=f_getcwd(pathname,sizeof(pathname));
      
      if(res!=FR_OK)
      {
        printf("getcwd failed:%d\r\n",res);
        snprintf(pathname,sizeof(pathname),"/");  /* root directory */
      }
      
      printf("%s\r\n",pathname);
    }
    else  /* specified directory */
    {
      snprintf(pathname,sizeof(pathname),"%.*s",len-3,str+3);
      
      /* change the current directory */
      res=f_chdir(pathname);
      
      if(res!=FR_OK)
      {
        printf("chdir failed:%d\r\n",res);
      }
      else
      {
        printf("%s\r\n",pathname);
      }
    }
  }
  else if(strcasecmp((char *)str,"pwd")==0)   /* show the path of current directory */
  {
    /* get the current directory */
    res=f_getcwd(pathname,sizeof(pathname));
    
    if(res!=FR_OK)
    {
      printf("getcwd failed:%d\r\n",res);
      snprintf(pathname,sizeof(pathname),"/");  /* root directory */
    }
    
    printf("%s\r\n",pathname);
  }
  else if(strncasecmp((char *)str,"rm ",3)==0)  /* removes a file or sub-directory */
  {
    if(len<4)
    {
      printf("please specify a file/directory.\r\n");
    }
    else
    {
      snprintf(pathname,sizeof(pathname),"%.*s",len-3,str+3);
      
      res=f_unlink(pathname);
      if(res!=FR_OK)
      {
        printf("remove \"%s\" failed:%d\r\n",pathname,res);
      }
      else
      {
        printf("\"%s\" is removed!\r\n",pathname);
      }
    }
  }
  else if(strncasecmp((char *)str,"mkdir ",6)==0)  /* Create a sub-directory */
  {
    if(len<7)
    {
      printf("please specify a directory name.\r\n");
    }
    else
    {
      snprintf(pathname,sizeof(pathname),"%.*s",len-6,str+6);
      
      res=f_mkdir(pathname);
      if(res!=FR_OK)
      {
        printf("make directory \"%s\" failed:%d\r\n",pathname,res);
      }
      else
      {
        printf("make directory \"%s\" ok!\r\n",pathname);
      }
    }
  }
  
  else
  {
    ret=-1;
  }
  
  /* Release mutex */
  //osMutexRelease(mutex_sd);
  
  return ret;
}







/**
  * @}
  */

/**
  * @}
  */

int32_t start_storage(void)
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

/**
  * @brief  Download historical data
  * @param  thread not used
  * @retval None
  */
static void Storage_Thread(void const *argument)
{
	static uint32_t byte_s=0;
  static FRESULT res_s = FR_OK;
  static uint8_t pathfile[24]={0};
	static uint32_t offset=0;
  while(1)
  {
    /* storage data等待开始存储数据的信号量 */
    if(osSemaphoreWait(semaphore,osWaitForever)==osOK)
    {
			 get_sys_time(&Storage_Date,&Storage_Time);
			/*存储分钟数据到SD卡*/
							offset=252*Storage_Time.Minutes;
							snprintf((char *)pathfile,sizeof(pathfile),"/DATA/%02u/%02u_%02u.txt",Storage_Date.Month,Storage_Date.Date,Storage_Time.Hours);
							res_s = f_open(&file,(const char *)pathfile, FA_OPEN_ALWAYS|FA_WRITE);
							if(res_s == FR_OK)
							{
								res_s=f_lseek(&file,offset);
								if(res_s == FR_OK)
								{
									res_s=f_write(&file,&data_1hour,252,&byte_s);
									if(res_s == FR_OK)
									{
										res_s = f_close(&file);	
									}
									else
									{
										if(BSP_SD_Init()!=MSD_OK)
											printf("sd init failed!\r\n");
										else
											printf("sd init ok!\r\n");
									}
								}
								else
								{
									if(BSP_SD_Init()!=MSD_OK)
											printf("sd init failed!\r\n");
									else
											printf("sd init ok!\r\n");
								}
							}
							else 
								{
									if(BSP_SD_Init()!=MSD_OK)
											printf("sd init failed!\r\n");
									else
											printf("sd init ok!\r\n");
								}
								
								/*存储小时数据*/
								if(Storage_Time.Minutes == 0)
								{
									offset=252*Storage_Time.Hours ;
									snprintf((char *)pathfile,sizeof(pathfile),"/DATAH/%02u/%02u.txt",Storage_Date.Month,Storage_Date.Date);
									res_s = f_open(&file,(const char *)pathfile, FA_OPEN_ALWAYS|FA_WRITE);
									if(res_s == FR_OK)
										{
											res_s=f_lseek(&file,offset);
											if(res_s == FR_OK)
												{
													res_s=f_write(&file,&data_1hour,252,&byte_s);
													if(res_s == FR_OK)
														{
															res_s = f_close(&file);	
														}
													else
														{
															if(BSP_SD_Init()!=MSD_OK)
																printf("sd init failed!\r\n");
															else
																printf("sd init ok!\r\n");
														}
												}
											else
												{
													if(BSP_SD_Init()!=MSD_OK)
														printf("sd init failed!\r\n");
													else
														printf("sd init ok!\r\n");
												}
										}
									else 
										{
											if(BSP_SD_Init()!=MSD_OK)
												printf("sd init failed!\r\n");											
											else
												printf("sd init ok!\r\n");
										}
								}
    }
  }
}



/**
  * @brief  Init directorys used when save/read sensor data.
  * @retval FRESULT
  */
static FRESULT init_directorys(void)
{
  FRESULT res=FR_OK;
  uint32_t month=0;
  char pathname[24];
  
  /* Check existance of a sub-directory,if not exists,create it */
  /* DATA_PATHNAME  分钟数据文件夹*/
  snprintf(pathname,sizeof(pathname),"/DATA");  /* 1 minute data */
  if(f_stat(pathname,NULL)!=FR_OK)
  {
    /* Create a sub-directory */
    if((res=f_mkdir(pathname))!=FR_OK)
    {
      printf("mkdir \"%s\" failed:%d\r\n",pathname,res);
      return res;
    }
  }
	
	 /* DATA_PATHNAME/Day */
  for(month=0;month<12;month++)
  {
		
			snprintf(pathname,sizeof(pathname),"/DATA/%02d",month+1);
			if(f_stat(pathname,NULL)!=FR_OK)
				{
					/* Create a sub-directory */
					if((res=f_mkdir(pathname))!=FR_OK)
						{
							printf("mkdir \"%s\" failed:%d\r\n",pathname,res);
							return res;
						}		
		}
  }

  /* DATA_PATHNAME  x小时数据文件夹*/
  snprintf(pathname,sizeof(pathname),"/DATAH");  /* 1 minute data */
  if(f_stat(pathname,NULL)!=FR_OK)
  {
    /* Create a sub-directory */
    if((res=f_mkdir(pathname))!=FR_OK)
    {
      printf("mkdir \"%s\" failed:%d\r\n",pathname,res);
      return res;
    }
  }
	
	 /* DATA_PATHNAME/Day */
  for(month=0;month<12;month++)
  {
		
			snprintf(pathname,sizeof(pathname),"/DATAH/%02d",month+1);
			if(f_stat(pathname,NULL)!=FR_OK)
				{
					/* Create a sub-directory */
					if((res=f_mkdir(pathname))!=FR_OK)
						{
							printf("mkdir \"%s\" failed:%d\r\n",pathname,res);
							return res;
						}		
		}
  }

  
 
  return FR_OK;
}




/*在SD卡里面建立一个文件专门存储一些需要实现记忆功能的参数，比如温度订正值，轮询分钟数据的间隔，台站号*/
/**********************************************************************************************
 *parameter:0是台站号，1是轮询分钟数据间隔，2是温度订正值
 *mode_rw:读写模式选择 0是读 ，1是写
 *data_parameter :写的时候写的内容
 *
 **********************************************************************************************/
uint32_t ReadWriteParameter(uint8_t parameter,uint8_t mode_rw,uint32_t data_parameter)
{
	static FRESULT  res_p      = FR_OK;
	static uint32_t bytes_p    = 0;
	/*读*/
	if(mode_rw == 0)
	{
		res_p = f_open(&file,(const char *)"parameter", FA_OPEN_ALWAYS|FA_WRITE);
		if(res_p == FR_OK)
		{
			res_p = f_lseek(&file,parameter*sizeof(uint32_t));
			if(res_p == FR_OK)
			{
				res_p=f_read(&file,&para_data[parameter],sizeof(uint32_t),&bytes_p);
				if(res_p == FR_OK)
				{
					res_p=f_close(&file);	
					return para_data[parameter];
				}
				else
				{
					if(BSP_SD_Init()!=MSD_OK)
						printf("sd init failed!\r\n");
					else
						printf("sd init ok!\r\n");	
				}
			}
			else
				{
					if(BSP_SD_Init()!=MSD_OK)
						printf("sd init failed!\r\n");
					else
						printf("sd init ok!\r\n");	
				}
		}
		else
		{
			if(BSP_SD_Init()!=MSD_OK)
				printf("sd init failed!\r\n");
			else
				printf("sd init ok!\r\n");	
		}
	}
	/*写*/
	else if(mode_rw == 1)
	{
		res_p = f_open(&file,(const char *)"parameter", FA_OPEN_ALWAYS|FA_WRITE);
		if(res_p == FR_OK)
		{
			res_p = f_lseek(&file,parameter*sizeof(uint32_t));
			if(res_p == FR_OK)
			{
				res_p=f_write(&file,&para_data[parameter],sizeof(uint32_t),&bytes_p);
				if(res_p == FR_OK)
				{
					res_p=f_close(&file);	
					return 123456;
				}
				else
				{
					if(BSP_SD_Init()!=MSD_OK)
						printf("sd init failed!\r\n");
					else
						printf("sd init ok!\r\n");	
				}
			}
			else
				{
					if(BSP_SD_Init()!=MSD_OK)
						printf("sd init failed!\r\n");
					else
						printf("sd init ok!\r\n");	
				}
		}
		else
		{
			if(BSP_SD_Init()!=MSD_OK)
				printf("sd init failed!\r\n");
			else
				printf("sd init ok!\r\n");	
		}
	}
	return 123457;
}



/*补要数据*/
/**********************************************************************************************
 *data_type:0补要分钟数据，1是补要小时数据
 *year_d,year_d,day_d,hour_d,minute_d:补要数据的起始时间
 *number_d :补要数据的条数
 *
 **********************************************************************************************/
void DownData(int data_type,int year_d,int month_d,int day_d,int hour_d,int minute_d)
{
	static FRESULT  res_d      = FR_OK;
	static uint32_t bytes_d    = 0;
	/*补要分钟数据*/
	if(data_type == 0)
	{
		snprintf(disp_buf,sizeof(disp_buf),"/DATA/%02d/%02d_%02d.txt",month_d,day_d,hour_d);
		res_d = f_open(&file,(const char *)disp_buf,FA_READ);
		if(res_d == FR_OK)
		{
				res_d=f_lseek(&file,minute_d*252);
				if(res_d == FR_OK)
				{
					memset(disp_buf,0,sizeof(disp_buf));
					res_d=f_read(&file,&disp_buf,252,&bytes_d);
					if(res_d == FR_OK)
					{
						if((memcmp("[D",disp_buf,2) == 0)&&(strlen(disp_buf) == 252)&&(disp_buf[251] == 93))
							{
								HAL_UART_Transmit(&huart1,(uint8_t *)disp_buf,sizeof(disp_buf),0xFF);
							}
						else  /*读不到的分钟数据 填充“///”*/
							{
								/*在SD卡里面没有读到该分钟的数据就发送下面的字符串代替*/
								printf("[D%05u20%02u%02u%02u%02u%02u00////////////////////]",
								       device.station ,year_d,month_d,day_d,hour_d,minute_d);
								printf("SD data:%s\r\n",disp_buf);
							}
						res_d=f_close(&file);	
					}
					else
					{
						if(BSP_SD_Init()!=MSD_OK)
							printf("sd init failed!\r\n");
						else
							printf("sd init ok!\r\n");
					}
				}
				else
				{
					if(BSP_SD_Init()!=MSD_OK)
						printf("sd init failed!\r\n");
					else
						printf("sd init ok!\r\n");	
				}
		}
	}
	else if(data_type == 1)
	{
		snprintf(disp_buf,sizeof(disp_buf),"/DATAH/%02d/%02d.txt",month_d,day_d);
		res_d = f_open(&file,(const char *)disp_buf,FA_READ);
		if(res_d == FR_OK)
		{
				res_d=f_lseek(&file,hour_d*252);
				if(res_d == FR_OK)
				{
					memset(disp_buf,0,sizeof(disp_buf));
					res_d=f_read(&file,&disp_buf,252,&bytes_d);
					if(res_d == FR_OK)
					{
						if((memcmp("[D",disp_buf,2) == 0)&&(strlen(disp_buf) == 252)&&(disp_buf[251] == 93))
							{
								HAL_UART_Transmit(&huart1,(uint8_t *)disp_buf,sizeof(disp_buf),0xFF);
							}
						else  /*读不到的分钟数据 填充“///”*/
							{
								/*在SD卡里面没有读到该分钟的数据就发送下面的字符串代替*/
								printf("[D%05u20%02u%02u%02u%02u0000////////////////////]",
								       device.station ,year_d,month_d,day_d,hour_d);
							}
						res_d=f_close(&file);	
					}
					else
					{
						if(BSP_SD_Init()!=MSD_OK)
							printf("sd init failed!\r\n");
						else
							printf("sd init ok!\r\n");
					}
				}
				else
				{
					if(BSP_SD_Init()!=MSD_OK)
						printf("sd init failed!\r\n");
					else
						printf("sd init ok!\r\n");	
				}
		}
	}
}


