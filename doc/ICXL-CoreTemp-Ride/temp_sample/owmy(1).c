#ifndef _OWMY_C_
#define _OWMY_C_


#include "owmy.h"
#include "delay.h"



#define unit unsigned int

sbit DQ = P1^1 ;

#define ow_Delay_us			Delay_us


void OW_Init(void)
{
	P11F = OUTPUT|PD_EN;		//P11设置为PP输出
	P11F = INPUT|PU_EN;     //P11设置为PP输入
	
	ow_DQ_set();
 	ow_Delay_us(500);
//	P00F = (1<<5)|2;
	ow_DQ_reset();
	
}


void OW_Reset(void)
{
	P11F = OUTPUT|PD_EN;		//P11设置为PP输出
	ow_DQ_reset(); 	// Drive DQ low
	ow_Delay_us(480);	
	ow_DQ_set(); 		// Release DQ
}

uchar OW_Presence(void)
{
	unsigned char dq;
	
	P11F = INPUT|PU_EN;     //P11设置为PP输入
	ow_Delay_us(70);
	ow_DQ_get(); 
     dq = DQ;
	ow_Delay_us(410); // Complete the reset sequence recovery
	
	return dq; 				
}

uchar OW_ResetPresence(void)
{
	unsigned char dq;
	P11F = OUTPUT|PD_EN;		//P11设置为PP输出
	ow_DQ_reset(); 	// Drive DQ low
	ow_Delay_us(480);	
	ow_DQ_set(); 		// Release DQ	

	P11F = INPUT|PU_EN;     //P11设置为PP输入
	ow_Delay_us(70);
//	ow_DQ_get(); 
     dq = DQ; 									// Get presence pulse from slave
	ow_Delay_us(410); 	// Complete the reset-presensce
	
	return dq; 	
}



// Send a bit1 to DQ. Provide 10us recovery time.
void OW_WriteBit(unsigned char bit1)
{
	P11F = OUTPUT|PD_EN;		//P11设置为PP输出
	if (bit1)
	{
		// Write '1' to DQ
		ow_DQ_reset(); 							// Initialte write '1' time slot.
		ow_Delay_us(2);
		ow_DQ_set(); 		
		ow_Delay_us(68); // Complete the write '1' time slot.
	}
	else
	{
		// Write '0' to DQ
		ow_DQ_reset();  						// Initialte write '0' time slot
		ow_Delay_us(60);
		ow_DQ_set(); 		
		ow_Delay_us(10); // Complete the write '0' time slot: recovery
	}
}

//---------------------------------------------------------------------------
// Read a bit from DQ. Provide 10us recovery time.
//
int OW_ReadBit(void)
{
	int bit1;
	P11F = OUTPUT|PD_EN;		//P11设置为PP输出
	ow_DQ_reset(); 								// Initialte read time slot
	ow_Delay_us(2);
	ow_DQ_set(); 	
	P11F = INPUT|PU_EN;     //P11设置为PP输入
	ow_Delay_us(10);
//	ow_DQ_get();
	bit1 =  DQ;				 		// Sample DQ to get the bit from the slave	
	ow_Delay_us(58); // Complete the read time slot with 10us recovery
	
	return (bit1 != 0);
}

/*Send a byte to DQ. LSB first, MSB last.*/
void OW_WriteByte(unsigned char data1)
{
	int bit1;
	P11F = OUTPUT|PD_EN;		//P11设置为PP输出
	for (bit1 = 0; bit1 < 8; bit1++)
	{
		OW_WriteBit(data1 & 0x01);
		data1 >>= 1;
	}
}
//---------------------------------------------------------------------------
// Read a byte from DQ and return it. LSB first, MSB last.
//
unsigned char OW_ReadByte(void)
{
	unsigned char bit1, byte=0;
		P11F = OUTPUT|PD_EN;		//P11设置为PP输出
	for (bit1 = 0; bit1 < 8; bit1++)
	{
		byte >>= 1;
		if (OW_ReadBit())
			byte |= 0x80;
	}
	
	return byte;
}

/* Single read time slot for polling slave ready.*/
OW_SLAVESTATUS OW_ReadStatus(void)
{
	int status;
		P11F = OUTPUT|PD_EN;	//P11设置为PP输出
	ow_DQ_reset(); 				  // Initiate read time slot
	ow_Delay_us(2);
	ow_DQ_set(); 
	P11F = INPUT|PU_EN;     //P11设置为PP输入
	ow_Delay_us(10);
//	ow_DQ_get();
	status = DQ;            // Get the status from DQ: '0' busy, '1' idle.
	ow_Delay_us(58);        // Complete the read time slot and recovery
	
	return (status ? READY : BUSY);
}
//---------------------------------------------------------------------------
//Multi-Drop 1-Wire network function: get a bit value and its complement.
//---------------------------------------------------------------------------
unsigned char OW_Read2Bits(void)  
{    
		unsigned char i, dq, data1;	
	  data1 = 0;
		P11F = OUTPUT|PD_EN;		//P11设置为PP输出
		for(i=0; i<2; i++) 
		{
			dq = OW_ReadBit();		
			data1 = (data1) | (dq<<i);
		}
	
		return data1;
}





#endif
