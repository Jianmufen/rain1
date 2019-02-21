/* Includes ------------------------------------------------------------------*/
#include "sensor.h"
#include "usart_module.h"
#include "crc.h"

/*结构体变量声明声明*/
RAIN rain;
Device_State device;

char  time_buf[17];
char  success_time[17];
char  down_data[512];			/*补要数据专用缓存*/
char  real_data[512];			/*实时分钟数据专用缓存*/

static const int MONTH_DAY[13]= {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


/*填充小时数据*/
int OutputHourDataFill(char *Data, RTC_DateTypeDef *date, RTC_TimeTypeDef *time)
{
				unsigned short crc = 0;
				int count = 0;
				
				if(Data==NULL)
				{
					return 0;
				}
				
				count =  snprintf(Data, 8,"1 AHQX ");
				count += snprintf(Data + count, 7,"%c%c%c%c%c ", device.station[0], device.station[1], device.station[2], device.station[3], device.station[4]);
				count +=  snprintf(Data + count, 18,"20%02u-%02u-%02u %02u:%02u ", date->Year, date->Month, date->Date, time->Hours, time->Minutes);
				count +=  snprintf(Data + count, 91,"10000000000000000000000000001110000000000000000000000000000000000000000000000000000000000 ");
				count +=  snprintf(Data + count, 5,"%d ", device.board_voltage_10);
				count +=  snprintf(Data + count, 4,"%d ", rain.RAIN_1M);
				count +=  snprintf(Data + count, 5,"%d ", rain.RAIN_1H);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[1]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[2]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[3]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[4]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[5]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[6]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[7]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[8]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[9]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[10]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[11]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[12]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[13]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[14]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[15]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[16]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[17]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[18]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[19]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[20]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[21]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[22]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[23]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[24]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[25]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[26]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[27]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[28]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[29]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[30]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[31]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[32]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[33]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[34]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[35]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[36]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[37]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[38]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[39]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[40]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[41]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[42]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[43]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[44]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[45]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[46]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[47]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[48]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[49]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[50]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[51]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[52]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[53]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[54]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[55]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[56]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[57]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[58]);
				count +=  snprintf(Data + count, 3,"%02d", rain.RAIN_M[59]);
				count +=  snprintf(Data + count, 4,"%02d ", rain.RAIN_M[0]);
				count +=  snprintf(Data + count, 6,"0000 ");
				
				crc = CalcCRC((unsigned char *)(Data + 2) , (count -2));
				
				count +=  snprintf(Data + count, 5,"%04X ", crc);
				
				Data[count - 1] = '\r';
				Data[count ] 		= '\n';
				Data[count + 1] = '\0';
					
				DEBUG("count=%d\r\n", count);
				return (count);
}



/*说明：计算出一年的当前月日时分所对应的扇区号。譬如：1月1日0时0分=0（扇区） 1月1日0时1分=1（扇区） 1月1日0时2分=0（扇区）
 *
 *
 *
 *
 *
 */
uint32_t CalculateSector(RTC_DateTypeDef *date, RTC_TimeTypeDef *time)
{
		//扇区号
		uint32_t sectors = 0;
		uint8_t yue = 0, ri = 0, shi = 0, fen = 0;
		if(date)
		{
				
				yue  = date->Month ;
				ri   = date->Date ;
		}
		if(time)
		{
				shi = time->Hours ;
				fen = time->Minutes ;
		}
		if(yue> 12)		yue = 1;
		if(ri> 31)		ri  = 1;
		if(shi> 23)		shi = 0;
		if(fen> 59)		fen = 0;
		
		DEBUG("%d-%d %d:%d\r\n", yue, ri, shi, fen);
		DEBUG("sectors=%d\r\n", sectors);
		//当月的前几个月的扇区号 （譬如1月前面没有月，2月前面的1月，3月的前面1、2月等等）
		yue -= 1;
		DEBUG("yue=%d\r\n", yue);
		while(yue--)
		{
				DEBUG("当月的天数=%d\r\n", MONTH_DAY[yue+1]);
				sectors += MONTH_DAY[yue+1] * 24 * 60 ;	//一天24小时，一小时60分钟
				DEBUG("YUE\r\n");
				DEBUG("sectors=%d\r\n", sectors);
		}
		
		//当月的前几天的扇区号
		ri -= 1;
		DEBUG("ri=%d\r\n", ri);
		sectors += ri * 24 * 60;									//一天24小时，一小时60分钟
		DEBUG("sectors=%d\r\n", sectors);
		
		//当天的前几个小时的扇区号
		//shi -= 1;
		DEBUG("shi=%d\r\n", shi);
		sectors += shi * 60;											//一小时60分钟
		DEBUG("sectors=%d\r\n", sectors);
		
		//当前小时的分钟数的扇区号
		DEBUG("fen=%d\r\n", fen);
		sectors += (fen + 1);
		DEBUG("sectors=%d\r\n", sectors);
		return sectors;
}	




