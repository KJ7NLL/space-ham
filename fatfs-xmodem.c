#include <stdio.h>

#include "ff.h"
#include "xmodem.h"

#include "serial.h"
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

	len = XmodemReceive(xmodem_rx_chunk, &out, 1024*1024, 1, 0);

	f_close(&out);

	return len;
}
