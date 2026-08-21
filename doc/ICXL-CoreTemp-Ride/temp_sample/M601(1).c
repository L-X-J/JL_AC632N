#ifndef _MY_C_
#define _MY_C_

#include "ca51f003_config.h"
#include "owmy.h"
#include "M601.h"
#include "delay.h"
#include "uart.h"

void  ConvertTemp(void)
{	 
	int i;
	unsigned char  scr[8];
	unsigned short  Temp_u16;
	float temp;
	OW_ResetPresence();	
  OW_WriteByte(0xcc);
  OW_WriteByte(0x44);
	Delay_ms(15);
	
	OW_ResetPresence();	
  OW_WriteByte(0xcc);
  OW_WriteByte(0xbe);
	for(i=0; i <sizeof(SCRATCHPAD_READ); i++)
  {
		scr[i] = OW_ReadByte();
	}

	Temp_u16 = scr[1]<<8 | scr[0];

//	uart_printf(" Temp_u16:%x  ",Temp_u16);
	temp = ((short)Temp_u16)/256.0+40.0;
	
	uart_printf("temp:%.2f \n",temp);
	
 
}


/**
  * @brief  读芯片寄存器的暂存器组
  * @param  scr：字节数组指针， 长度为 @sizeof（SCRATCHPAD_READ）
  * @retval 读状态
*/
int ReadScratchpad_SkipRom(unsigned char *scr)
{
    int i;

	/*size < sizeof(SCRATCHPAD_READ)*/
    if(OW_ResetPresence() == 1)					
			return 0;
		
    OW_WriteByte(0xcc);
    OW_WriteByte(0xbe);//READ_SCRATCHPAD
		
		for(i=0; i < sizeof(SCRATCHPAD_READ); i++)
    {
		*scr++ = OW_ReadByte();
	}

    return 1;
}
/**
  * @brief  写芯片寄存器的暂存器组
  * @param  scr：字节数组指针， 长度为 @sizeof（SCRATCHPAD_WRITE）
  * @retval 写状态
**/
int WriteScratchpad_SkipRom(unsigned char *scr)
{
    int i;

    if(OW_ResetPresence() == 1)						
			return 0;
		
    OW_WriteByte(0xcc);
    OW_WriteByte(0x4e);//WRITE_SCRATCHPAD
		
		for(i=0; i < sizeof(SCRATCHPAD_WRITE); i++)
    {
			OW_WriteByte(*scr++);
	}

    return 1;
}

/**
  * @brief  把16位二进制补码表示的温度输出转换为以摄氏度为单位的温度读数
  * @param  out：有符号的16位二进制温度输出
  * @retval 以摄氏度为单位的浮点温度
*/
float OutputtoTemp(unsigned short out)
{
	return (((short)out)/256.0 + 40.0);	
}

#endif
