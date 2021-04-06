#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "em_device.h"
#include "em_chip.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"

// Size of the buf for received data
#define BUFLEN  80

#define LED_PORT gpioPortB
#define LED0_PIN 0
#define LED1_PIN 1

// Receive data buf
unsigned char *bufrx = NULL;
unsigned char *buftx = NULL;
int buftxlen = 0, bufrxlen = 0;

void initGpio(void)
{
	// Configure PA5 as an output (TX)
	GPIO_PinModeSet(gpioPortA, 5, gpioModePushPull, 0);

	// Configure PA6 as an input (RX)
	GPIO_PinModeSet(gpioPortA, 6, gpioModeInput, 0);

	// Configure PB4 as output and set high (VCOM_ENABLE)
	// comment out next line to disable VCOM
	GPIO_PinModeSet(gpioPortD, 4, gpioModePushPull, 1);

	// turn on LED0 
	GPIO_PinModeSet(LED_PORT, LED0_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(LED_PORT, LED1_PIN, gpioModePushPull, 1);
}

void initCmu(void)
{
	// Enable clock to GPIO and USART1
	CMU_ClockEnable(cmuClock_GPIO, true);
	CMU_ClockEnable(cmuClock_USART1, true);

}

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
	GPIO_PinOutToggle(LED_PORT, LED0_PIN);
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

		GPIO_PinOutToggle(LED_PORT, LED1_PIN);
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

void serial_write(unsigned char *s, int len)
{
	buftx = s;
	buftxlen = len;
	
	// Enable transmit buffer level interrupt
	USART_IntEnable(USART1, USART_IEN_TXBL);
	while (buftx != NULL)
		EMU_EnterEM1();

}

void serial_read(unsigned char *s, int len)
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

void help()
{
	char *h =
		"\x0c\r\n" 
		"mv (phi|theta) <([+-]deg|n|e|s|w)>                    # moves antenna\r\n"
		"cam (on|off)                                          # turns camera on or off\r\n"
		"(help|?|h)                                            # list of commands\r\n"
		"status                                                # display system status\r\n"
		"cal (theta|phi) <deg>                                 # Calibrates phi or theta\r\n"
		"speed (theta|phi) (0-100)                             # 0 means stop\r\n"
		"sat (load|track|list|search)                          # satellite commands\r\n";
	print(h);
}

enum KEYS 
{
	KEY_UP = 1, 
	KEY_DN,
	KEY_RT,
	KEY_LT,
	KEY_HOME,
	KEY_END
};

unsigned char *vt102[] = 
{
	(unsigned char[]){KEY_UP, 0x1b, '[', 'A', 0},
	(unsigned char[]){KEY_DN, 0x1b, '[', 'B', 0},
	(unsigned char[]){KEY_RT, 0x1b, '[', 'C', 0},
	(unsigned char[]){KEY_LT, 0x1b, '[', 'D', 0},
	(unsigned char[]){KEY_HOME, 0x1b, '[', '1', '~', 0},
	(unsigned char[]){KEY_END, 0x1b, 'O', 'F', 0},
	NULL	 
};

void dump_vt_keys(char **keys)
{
	int i, j;
	char buf[128];

	for (i = 0; keys[i] != NULL; i++)
	{
		for (j = 0; keys[i][j] != 0; j++)
		{
			snprintf(buf, sizeof(buf)-1, "%02x %c\t", keys[i][j], keys[i][j]); 
			print(buf);
		}
		print("\r\n");
	}
}

int input(char *buf, int len)
{
	unsigned char in[10];

	char c;

	int i = 0, end = 0, pos = 0;
	
	c = 0;

	while (end < len && c != '\r' && c != '\n')
	{
		serial_read(&c, 1);
		
		if (isprint(c))
		{
			serial_write(&c, 1);
			buf[pos] = c;
			pos++;
			end++;
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
				serial_write("\x07\x07\x07", 1);
			}
		}
	}

	buf[end] = 0;
	return end;
}

int main(void)
{
	char buf[128];
	char c;
	
	// Chip errata
	CHIP_Init();

	// Initialize GPIO and USART1
	initCmu();
	initGpio();
	initUsart1();

	help();
//	dump_vt_keys(vt102);

	while (1)
	{
		print("Zeke&Daddy@console ");
		input(buf, sizeof(buf)-1);
		print("\r\n");
		print(buf);
		print("\r\n");
	}
}
