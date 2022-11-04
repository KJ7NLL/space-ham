//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
// 
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Library General Public License for more details.
// 
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
// 
//  Copyright (C) 2022- by Ezekiel Wheeler, KJ7NLL and Eric Wheeler, KJ7LNW.
//  All rights reserved.
//
//  The official website and doumentation for space-ham is available here:
//    https://www.kj7nll.radio/
//
#include <stdio.h>

#include "em_chip.h"

#include "ff.h"
#include "xmodem.h"

#include "serial.h"
#include "systick.h"

#include "fatfs-efr32.h"


// msec timeout
int _inbyte(unsigned short timeout)
{
	unsigned char c = 0;

	if (serial_read_timeout(&c, 1, timeout / 1000.0) == 0)
		return -1;

	return c;
}

void _outbyte(int c)
{
	serial_write(&c, 1);
}

void xmodem_rx_chunk(void *ctx, void *buf, int len)
{
	FIL *out = (FIL*)ctx;
	UINT bw;
	FRESULT fr;

	fr = f_write(out, buf, len, &bw);

	if (fr != FR_OK)
		printf("Chunk Write Error %d: %s\r\n", fr, ff_strerror(fr));

	if ((int)bw < len)
		printf("Chunk Write Error: Only %d of %d bytes were written\r\n", bw, len);
}

int xmodem_rx(char *filename)
{
	FIL out;
	FRESULT fr;          /* FatFs function common result code */
	int len;

	fr = f_open(&out, filename, FA_WRITE | FA_CREATE_ALWAYS);

	if (fr != FR_OK)
	{
		printf("error %d: %s: %s\r\n", fr, filename, ff_strerror(fr));
		return -(fr+10);
	}


	// IADC and systick handlers drown the CPU in IRQ triggers
	// so turn them off while transferring:
	int iadc = NVIC_GetEnableIRQ(IADC_IRQn);
	if (iadc)
	{
		NVIC_DisableIRQ(IADC_IRQn);
		NVIC_ClearPendingIRQ(IADC_IRQn);
	}
	systick_bypass(1);
	
	len = XmodemReceive(xmodem_rx_chunk, &out, 1024*1024, 1, 0);

	systick_bypass(0);
	if (iadc) NVIC_EnableIRQ(IADC_IRQn);

	f_close(&out);

	return len;
}

void xmodem_tx_chunk(void *ctx, void *buf, int len)
{
	FIL *in = (FIL*)ctx;
	UINT br;
	FRESULT fr;

	fr = f_read(in, buf, len, &br);

	if (fr != FR_OK)
		printf("Chunk Read Error %d: %s\r\n", fr, ff_strerror(fr));

	if ((int)br < len)
		printf("Chunk Read Error: Only %d of %d bytes were read\r\n", br, len);
}


int xmodem_tx(char *filename)
{
	FIL in;
	FRESULT fr;          /* FatFs function common result code */
	int len;

	fr = f_open(&in, filename, FA_READ);

	if (fr != FR_OK)
	{
		printf("error %d: %s: %s\r\n", fr, filename, ff_strerror(fr));
		return -(fr+10);
	}
	printf("sending %s: %d bytes\r\n", filename, (int)f_size(&in));


	// IADC and systick handlers drown the CPU in IRQ triggers
	// so turn them off while transferring:
	int iadc = NVIC_GetEnableIRQ(IADC_IRQn);
	if (iadc)
	{
		NVIC_DisableIRQ(IADC_IRQn);
		NVIC_ClearPendingIRQ(IADC_IRQn);
	}
	systick_bypass(1);

	len = XmodemTransmit(xmodem_tx_chunk, &in, (int)f_size(&in), 1, 0);

	systick_bypass(0);
	if (iadc) NVIC_EnableIRQ(IADC_IRQn);

	f_close(&in);

	return len;
}
