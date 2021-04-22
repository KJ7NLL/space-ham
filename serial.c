#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "em_device.h"
#include "em_chip.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"

#include "linklist.h"
#include "strutil.h"

// Receive data buf
unsigned char *bufrx = NULL;
unsigned char *buftx = NULL;
int buftxlen = 0, bufrxlen = 0;

enum KEYS 
{
	KEY_UP = 1, 
	KEY_DN,
	KEY_RT,
	KEY_LT,
	KEY_HOME,
	KEY_END,

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
	NULL	 
};


void initUsart1(void)
{
	// Default asynchronous initializer (115.2 Kbps, 8N1, no flow
	// control)
	USART_InitAsync_TypeDef init = USART_INITASYNC_DEFAULT;

	// Route USART1 TX and RX to PA5 and PA6 pins, respectively
	GPIO->USARTROUTE[1].TXROUTE =
		(gpioPortA << _GPIO_USART_TXROUTE_PORT_SHIFT) | (5 <<
								 _GPIO_USART_TXROUTE_PIN_SHIFT);
	GPIO->USARTROUTE[1].RXROUTE =
		(gpioPortA << _GPIO_USART_RXROUTE_PORT_SHIFT) | (6 <<
								 _GPIO_USART_RXROUTE_PIN_SHIFT);

	// Enable RX and TX signals now that they have been routed
	GPIO->USARTROUTE[1].ROUTEEN =
		GPIO_USART_ROUTEEN_RXPEN | GPIO_USART_ROUTEEN_TXPEN;

	// Configure and enable USART1
	USART_InitAsync(USART1, &init);

	// Enable NVIC USART sources
	NVIC_ClearPendingIRQ(USART1_RX_IRQn);
	NVIC_EnableIRQ(USART1_RX_IRQn);
	NVIC_ClearPendingIRQ(USART1_TX_IRQn);
	NVIC_EnableIRQ(USART1_TX_IRQn);
}

void USART1_RX_IRQHandler(void)
{
	if (bufrx != NULL && bufrxlen > 0)
	{
		*bufrx = USART1->RXDATA;
		bufrx++;
		bufrxlen--;

		if (bufrxlen == 0)
		{
			bufrx = NULL;
		}
	}
	else
		USART_IntDisable(USART1, USART_IEN_RXDATAV);
}

void USART1_TX_IRQHandler(void)
{

	if (buftx != NULL && buftxlen > 0)
	{
		USART1->TXDATA = *buftx;
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
		USART_IntDisable(USART1, USART_IEN_TXBL);
		USART_IntClear(USART1, USART_IEN_TXBL);
	}
}

void serial_write(void *s, int len)
{
	buftx = s;
	buftxlen = len;
	
	// Enable transmit buffer level interrupt
	USART_IntEnable(USART1, USART_IEN_TXBL);
	while (buftx != NULL)
		EMU_EnterEM1();

}

void serial_read(void *s, int len)
{
	bufrx = s;
	bufrxlen = len;

	// Enable receive data valid interrupt
	USART_IntEnable(USART1, USART_IEN_RXDATAV);
	while (bufrx != NULL)
		EMU_EnterEM1();

}

void print(char *s)
{
	serial_write(s, strlen(s));
}

void print_back(char *s)
{
	int i = 0;

	serial_write(s, strlen(s));
	for (i = strlen(s); i > 0; i--)
	{
		print("\x08");
	}
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

		esc[escidx + 1] = 0;
			
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
			 * enum name (eg, KEY_UP) that we wish to return fron this function.
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

int input(char *buf, int len, struct linklist *history)
{
	char c;

	int end = 0, pos = 0, key;
	
	c = 0;

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
			if (end != 0)
			{
				serial_write("\x08 \x08", 3);
				pos--;
				end--;
			}
			else
			{
				// BEEP BEEP BEEEP
				serial_write("\x07\x07\x07", 3);
			}
		}
		else if (c == 27)	// Escape
		{
			key = esc_key(vt102);
			switch (key)
			{
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
					// strncpy(buf, history[histidx], len);

					break;
				
				case KEY_DN:
					break;

				case KEY_HOME:
					break;

				case KEY_END:
					break;
			}
		}
	}

	buf[end] = 0;
	return end;
}

