#ifndef _MY_H_
#define _MY_H_


//#include "owmy.h"

typedef struct
{
	unsigned char Res[3];
	unsigned char Ch_Sel;					/*电容通道选择寄存器，RW*/
	unsigned char Cos;						/*偏置电容配置寄存器，RW*/
	unsigned char Res1;				
	unsigned char T_coeff[3];			
	unsigned char Cfb;						/*量程电容配置寄存器，RW*/									
	unsigned char Res2;
	unsigned char Res3[2];
	unsigned char dummy8;
	unsigned char crc_para;				/*CRC for byte0-13, RO*/
} SCRPARAMETERS;

typedef struct
{
	unsigned char T_lsb;					/*The LSB of 温度结果, RO*/
	unsigned char T_msb;					/*The MSB of 温度结果, RO*/
	unsigned char C1_lsb;					/*The LSB of 电容通道C1, RO*/
	unsigned char C1_msb;					/*The MSB of 电容通道C1, Ro*/	
	unsigned char Tha_set_lsb;		
	unsigned char Tla_set_lsb;		
	unsigned char Cfg;						/*系统配置寄存器, RW*/
	unsigned char Status;					/*系统状态寄存器, RO*/
	unsigned char crc_scr;				/*CRC for byte0-7, RO*/
} SCRATCHPAD_READ;

typedef struct
{	
	signed char Tha_set_lsb;				
	signed char Tla_set_lsb;			
	unsigned char Cfg;						/*系统配置寄存器, RW*/
} SCRATCHPAD_WRITE;

void  ConvertTemp(void);
#endif