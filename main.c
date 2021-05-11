#include <math.h>
#include <stdio.h>
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
#include "serial.h"
#include "strutil.h"
#include "iadc.h"
#include "pwm.h"

// Size of the buf for received data
#define BUFLEN  80

#define LED_PORT gpioPortB
#define LED0_PIN 0
#define LED1_PIN 1

void initGpio(void)
{
	// Configure PA5 as an output (TX)
	GPIO_PinModeSet(gpioPortA, 5, gpioModePushPull, 0);

	// Configure PA6 as an input (RX)
	GPIO_PinModeSet(gpioPortA, 6, gpioModeInput, 0);

	// Configure PD4 as output and set high (VCOM_ENABLE)
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

void help()
{
	char *h =
		"mv (phi|theta) <([+-]deg|n|e|s|w)>                    # moves antenna\r\n"
		"cam (on|off)                                          # turns camera on or off\r\n"
		"(help|?|h)                                            # list of commands\r\n"
		"status                                                # display system status\r\n"
		"cal (theta|phi) <deg>                                 # Calibrates phi or theta\r\n"
		"speed (theta|phi) (0-100)                             # 0 means stop\r\n"
		"sat (load|track|list|search)                          # satellite commands\r\n";
	print(h);
}

void dump_history(struct linklist *history)
{
	while (history != NULL)
	{
		print(history->s);
		print("\r\n");
		history = history->next;
	}
}

void status()
{
	char s[128];
	int i;

	for (i = 0; i < IADC_NUM_INPUTS; i++)
	{
		sprintf(s, "iadc[%d]: %f\r\n", i, (float)iadc_get_result(i));
		print(s);
	}
}

int main()
{
	struct linklist *history = NULL, *tmp;
	char buf[128];
	char c;
	
	// Chip errata
	CHIP_Init();

	// Initialize GPIO and USART1
	initCmu();
	initGpio();
	initUsart1();
	initIADC();

	print("\x0c\r\n");
	help();
	status();
	
	/*
	void initIADC(void);
	double get_result(int i);
	*/

	while (1)
	{
		print("[Zeke&Daddy@console]# ");
		input(buf, sizeof(buf)-1, &history);
		
		print("\r\n");
		if (match(buf, "history") || match(buf, "hist"))
		{
			dump_history(history);
		}
		else if (match(buf, "h") || match(buf, "?") || match(buf, "help"))
		{
			help();
		}
		else if (match(buf, "debug-keys"))
		{
			print("press CTRL+C to end\r\n");
			c = 0;
			while (c != 3)
			{
				serial_read(&c, 1);
				sprintf(buf, "you typed: %3d (0x%02x): '%c'\r\n", c, c, isprint(c) ? c : '?');
				print(buf);
			}
		}
		
		else if (match(buf, "status") || match(buf, "stat"))
		{
			status();
		}
		// This must be the last else if:
		else if (!match(buf, ""))
		{
			print("Unkown command: ");
			print(buf);
			print("\r\n\n");
		}
	}
}
