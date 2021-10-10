#include "ff.h"
#include "xmodem.h"
#include "serial.h"


// msec timeout
int _inbyte(unsigned short timeout)
{
	unsigned char c;

	if (serial_read_timeout(&c, 1, timeout / 1000.0) == 0)
		return -1;

	return c;
}

void _outbyte(int c)
{
	serial_write(&c, 1);
}
