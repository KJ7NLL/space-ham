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
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "em_device.h"
#include "em_chip.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"

#include "rtcc.h"
#include "linklist.h"
#include "strutil.h"

// RX ring buffer used internally in the RX interrupt
// At least 128 byte ring buffer is necessary for xmodem, so set it to 256:
#define READ_BUF_BITS	8
#define READ_BUF_SIZE	(1 << READ_BUF_BITS)
#define READ_BUF_MASK	(READ_BUF_SIZE-1)

volatile unsigned char read_buf[READ_BUF_SIZE];
volatile int read_buf_cur = 0, read_buf_next = 0;
volatile int bytes_read = 0;

// Receive data buf provided by user code
volatile unsigned char *bufrx = NULL;
volatile unsigned char *buftx = NULL;
volatile int buftxlen = 0, bufrxlen = 0;

enum KEYS 
{
	KEY_UP = 1, 
	KEY_DN,
	KEY_RT,
	KEY_LT,
	KEY_HOME,
	KEY_END,
	KEY_DEL,

	// Start control characters at 1000
	KEY_CTRL_C = 1003
};

char *vt102[] = 
{
	(char[]){KEY_UP, 0x1b, '[', 'A', 0},
	(char[]){KEY_DN, 0x1b, '[', 'B', 0},
	(char[]){KEY_RT, 0x1b, '[', 'C', 0},
	(char[]){KEY_LT, 0x1b, '[', 'D', 0},
	(char[]){KEY_HOME, 0x1b, '[', '1', '~', 0},
	(char[]){KEY_END, 0x1b, 'O', 'F', 0},
	(char[]){KEY_DEL, 0x1b, '[', '3', '~', 0},
	NULL	 
};


void initUsart0(void)
{
	// Default asynchronous initializer (115.2 Kbps, 8N1, no flow
	// control)
	USART_InitAsync_TypeDef init = USART_INITASYNC_DEFAULT;

	CMU_ClockEnable(cmuClock_USART0, true);

	memset((unsigned char*)read_buf, 0, sizeof(read_buf));

	// Route USART0 TX and RX to PA5 and PA6 pins, respectively
	GPIO->USARTROUTE[0].TXROUTE =
		(gpioPortA << _GPIO_USART_TXROUTE_PORT_SHIFT) | (5 <<
								 _GPIO_USART_TXROUTE_PIN_SHIFT);
	GPIO->USARTROUTE[0].RXROUTE =
		(gpioPortA << _GPIO_USART_RXROUTE_PORT_SHIFT) | (6 <<
								 _GPIO_USART_RXROUTE_PIN_SHIFT);

	// Enable RX and TX signals now that they have been routed
	GPIO->USARTROUTE[0].ROUTEEN =
		GPIO_USART_ROUTEEN_RXPEN | GPIO_USART_ROUTEEN_TXPEN;

	// Configure and enable USART0
	USART_InitAsync(USART0, &init);

	// Enable NVIC USART sources
	NVIC_ClearPendingIRQ(USART0_RX_IRQn);
	NVIC_EnableIRQ(USART0_RX_IRQn);
	NVIC_ClearPendingIRQ(USART0_TX_IRQn);
	NVIC_EnableIRQ(USART0_TX_IRQn);

	// Enable receive data valid interrupt
	USART_IntEnable(USART0, USART_IEN_RXDATAV);
}

int _read_buf_to_bufrx()
{
	int i = 0;

	while (bufrx != NULL && bufrxlen > 0 && read_buf_cur != read_buf_next)
	{
		*bufrx = read_buf[read_buf_cur];
		read_buf_cur = ((read_buf_cur+1) & READ_BUF_MASK); 

		bufrx++;
		bufrxlen--;

		if (bufrxlen == 0)
		{
			bufrx = NULL;
		}

		i++;
	}

	bytes_read += i;

	return i;
}

int read_buf_to_bufrx()
{
	int count;

	if (bufrx == NULL || bufrxlen == 0 || read_buf_cur == read_buf_next)
		return 0;

	USART_IntDisable(USART0, USART_IEN_RXDATAV);
	count = _read_buf_to_bufrx();
	USART_IntEnable(USART0, USART_IEN_RXDATAV);

	return count;
}

void USART0_RX_IRQHandler(void)
{
	unsigned char c = USART0->RXDATA;
	if (((read_buf_next+1) & READ_BUF_MASK) != read_buf_cur)
	{
		read_buf[read_buf_next] = c;
		read_buf_next = ((read_buf_next+1) & READ_BUF_MASK); 
	}

	_read_buf_to_bufrx();
}

void USART0_TX_IRQHandler(void)
{

	if (buftx != NULL && buftxlen > 0)
	{
		USART0->TXDATA = *buftx;
		buftx++;
		buftxlen--;
	}

	if (buftxlen == 0)
	{
		buftx = NULL;

		/*
		 * Need to disable the transmit buf level interrupt in this IRQ
		 * handler when done or it will immediately trigger again upon exit
		 * even though there is no data left to send.
		 */
		USART_IntDisable(USART0, USART_IEN_TXBL);
		USART_IntClear(USART0, USART_IEN_TXBL);
	}
}

void serial_write(void *s, int len)
{
	buftx = s;
	buftxlen = len;

	if (s == NULL)
		return;
	
	// Enable transmit buffer level interrupt
	USART_IntEnable(USART0, USART_IEN_TXBL);
	while (buftx != NULL)
		EMU_EnterEM1();

}

void serial_read_async(void *s, int len)
{
	USART_IntDisable(USART0, USART_IEN_RXDATAV);
	bufrx = s;
	bufrxlen = len;
	bytes_read = 0;
	USART_IntEnable(USART0, USART_IEN_RXDATAV);
}

inline int serial_read_async_bytes_read()
{
	return bytes_read;
}

void serial_read_async_cancel()
{
	USART_IntDisable(USART0, USART_IEN_RXDATAV);
	bufrxlen = 0;
	bufrx = NULL;
	bytes_read = 0;
	USART_IntEnable(USART0, USART_IEN_RXDATAV);
}

int serial_read_done()
{
	return bufrx == NULL;
}

void serial_read_idle(void *s, int len, void (*idle)())
{
	serial_read_async(s, len);

	while (!serial_read_done())
	{
		// Call read_buf_to_bufrx() here in case the RX interrupt 
		// was called before bufrx configured by serial_read_async().
		read_buf_to_bufrx();
		if (!serial_read_done())
		{
			if (idle != NULL)
				idle();
			else
				EMU_EnterEM1();
		}
	}

}

void serial_read(void *s, int len)
{
	serial_read_idle(s, len, NULL);
}

int serial_read_timeout(void *s, int len, float timeout)
{
	int count = 0;
	uint64_t start;

	start = rtcc_get();
	serial_read_async(s, len);

	while (!serial_read_done() && rtcc_elapsed_sec(start) < timeout)
	{
		// Call read_buf_to_bufrx() here in case the RX interrupt 
		// was called before bufrx configured by serial_read_async().
		read_buf_to_bufrx();
		if (!serial_read_done())
			EMU_EnterEM1();
	}

	count = serial_read_async_bytes_read();
	
	serial_read_async_cancel();

	return count;
}

int serial_read_line(char *s, int len)
{
	int n = 0;
	char c;

	do
	{
		serial_read(&c, 1);

		// If ctrl-d is the first char, then exit early
		// otherwise ignore it.
		if (c == 4)
		{
			if (n == 0) return 0;
			else continue;
		}

		s[n] = c;
		n++;

	} while (n < len-1 && c != '\r');

	// Add a \0 if there is room
	if (n < len)
		s[n] = 0;
	return n;
}

int _read(int handle, char *data, int size)
{
	if (!size) return 0;

	// Wait for the first char
	serial_read(data, size);
/*	
	int n = 0;
	n++;
	serial_read_async(data + 1, size - 1);

	// Read more if there is any available
	USART_IntDisable(USART0, USART_IEN_RXDATAV);
	n += read_buf_to_bufrx();
	bufrx = NULL;
	bufrxlen = 0;
	USART_IntEnable(USART0, USART_IEN_RXDATAV);
*/

	return size;
}

int _write(int handle, char *data, int size)
{
	serial_write(data, size);

	return size;
}

void print(char *s)
{
	serial_write(s, strlen(s));
}

void print_lots(char c, int n)
{
	int i = 0;

	for (i = n; i > 0; i--)
	{
		serial_write(&c, 1);
	}
}

void print_bs(int n)
{
	print_lots('\x08', n);
}

void print_back(char *s)
{
	int len = strlen(s);

	serial_write(s, len);
	print_bs(len);
}


// Print the string s, followed by n occurances of c, then
// backspace to the beginning
void print_back_more(char *s, char c, int n)
{
	print(s);
	print_lots(c, n);
	print_bs(n + strlen(s));
}

int esc_key(char **keys)
{
	char esc[7], c;

	unsigned int esc_maxidx;
	unsigned int i, escidx = 0;
	
	esc_maxidx = longest_length(keys) - 2;

	do
	{
		serial_read(&c, 1);
		esc[escidx] = c;
		escidx++;

		esc[escidx] = 0;
			
		if (c == 3)
		{
			return KEY_CTRL_C;
		}
		
		for (i = 0; keys[i]; i++)
		{
			/*
			 * keys[i] represents the escape code that we wish to match.
			 * When esc_key() is called, the escape charecter 27 (0x1b) 
			 * has already been read. The position keys[i][0] contains the
			 * enum name (eg, KEY_UP) that we wish to return fron this function
			 * Since we need to match agenst the escape code read from the
			 * terminal (esc), and since the first index is the enum, we
			 * need to match esc against keys[i]+2.
			 */
			if (match(esc, keys[i]+2))
			{
				return keys[i][0];
			}
		}

	} while (escidx < esc_maxidx && escidx < sizeof(esc)-1);

	return 0;
}

int input(char *buf, int len, struct linklist **history, void (*idle)())
{
	struct linklist *hist = *history;

	char c;

	int end = 0, pos = 0, key;
	
	c = 0;

	buf[0] = 0;
	while (end < len && c != '\r' && c != '\n')
	{
		serial_read(&c, 1);
		
		if (isprint(c))
		{
			serial_write(&c, 1);
			
			shift_right(buf, pos);
			buf[pos] = c;
			pos++;
			end++;
			buf[end] = 0;
			print_back(&buf[pos]);
		}
		else if (c == 8)	// Backspace
		{  
			if (end != 0 && pos != 0)
			{
				serial_write("\x08 \x08", 3);
				shift_left(buf, pos - 1);
				pos--;
				end--;
				buf[end] = 0;
				print_back_more(&buf[pos], ' ', 1);
			}
			else
			{
				// BEEP BEEP BEEEP
				serial_write("\x07\x07\x07", 3);
			}
		}
		else if (c == 3)	// CTRL+C
		{
			buf[0] = 0;
			pos = 0;
			end = 0;
			print("^C");
			break;
		}
		else if (c == 27)	// Escape
		{
			key = esc_key(vt102);
			switch (key)
			{
				case KEY_DEL:
					if (end != 0 && pos != end)
					{
						shift_left(buf, pos);
						end--;
						buf[end] = 0;
						print_back_more(&buf[pos], ' ', 1);
					}
					else
					{
						// BEEP BEEP BEEEP
						serial_write("\x07\x07\x07", 3);
					}

					break;

				case KEY_LT:
					if (pos > 0)
					{
						print("\x08");
						pos--;
					}
					else
					{
						// BEEP BEEP BEEEP
						serial_write("\x07\x07\x07", 3);
					}

					break;

				case KEY_RT:
					if (pos < end)
					{
						serial_write(&buf[pos], 1);
						pos++;
					}
					else
					{
						// BEEP BEEP BEEEP
						serial_write("\x07\x07\x07", 3);
					}

					break;

				case KEY_UP:
					
					if (hist != NULL)
					{
						// Clear the existing input text
						print(&buf[pos]);
						while (end != 0)
						{
							print("\x08 \x08");
							end--;
						}
						
						// Skip items in history that match the current buffer
						while (hist->next != NULL && match(hist->s, buf))
						{
							hist = hist->next;
						}

						// Update and print the buffer from history
						strncpy(buf, hist->s, len);
						print(buf);
						end = pos = strlen(buf);
					}
					break;
				
				case KEY_DN:

					if (hist != NULL)
					{
						// Clear the existing input text
						print(&buf[pos]);
						while (end != 0)
						{
							print("\x08 \x08");
							end--;
						}

						// Skip items in history that match the current buffer
						while (hist->prev != NULL && match(hist->s, buf))
						{
							hist = hist->prev;
						}

						// Update and print the buffer from history
						strncpy(buf, hist->s, len);
						print(buf);
						end = pos = strlen(buf);

					}
					break;

				case KEY_HOME:
					
					print_bs(pos);
					pos = 0;
					break;

				case KEY_END:
					
					print(&buf[pos]);
					pos = end;
					break;
			}
		}
	}

	buf[end] = 0;
	
	if (end > 0 && (*history == NULL || !match(buf, (*history)->s)))
	{
		hist = add_node(*history, buf);
		if (hist != NULL)
		{
			*history = hist;
		}
		else
		{
			print("unable to allocate history node: ");
			print(buf);
			print("\r\n");
		}
	}
	return end;
}

