#ifndef _OWMY_H_
#define _OWMY_H_

#include "ca51f003xsfr.h"
#include "gpiodef_f003.h"
#include "ca51f003sfr.h"

#define ow_DQ_set()   		{ P11=1; }
#define ow_DQ_reset() 		{ P11=0; }
#define ow_DQ_get()   		{ P11F = INPUT;}
   
#define uchar unsigned char

typedef enum {
  READY       = 0,
  BUSY    		= 1
} OW_SLAVESTATUS;

/* Exported_Functions----------------------------------------------------------*/
void OW_Init(void);
uchar OW_ResetPresence(void);
void OW_WriteByte(unsigned char data1);
unsigned char OW_ReadByte(void);
OW_SLAVESTATUS OW_ReadStatus(void);

unsigned char OW_Read2Bits(void) ;


#endif
