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
unsigned char bufrx[BUFLEN] = {0};
unsigned char bufecho[BUFLEN] = {0};
unsigned char *buftx = NULL;
int buftxlen = 0, bufrxidx = 0, bufechorxidx = 0, bufechotxidx = 0;

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

	// Get the character just received
	if (bufecho[bufechorxidx] == 0)
	{
		bufecho[bufechorxidx] = USART1->RXDATA;
		bufechorxidx++;
		if (bufechorxidx >= BUFLEN)
		{
			bufechorxidx = 0;
		}
	}


	USART_IntEnable(USART1, USART_IEN_TXBL);
}

void USART1_TX_IRQHandler(void)
{

	if (buftx != NULL && buftxlen > 0)
	{
		USART1->TXDATA = *buftx;
		buftx++;
		buftxlen--;

		if (buftxlen == 0)
		{
			buftx = NULL;

		}
	}
	else if (bufecho[bufechotxidx] != 0)
	{
		USART1->TXDATA = bufecho[bufechotxidx];
		bufecho[bufechotxidx] = 0;
		bufechotxidx++;
		
		if (bufechotxidx >= BUFLEN)
		{
			bufechotxidx = 0;
		}
	}
	else
	{
		GPIO_PinOutToggle(LED_PORT, LED1_PIN);
		/*
		 * Need to disable the transmit buf level interrupt in this IRQ
		 * handler when done or it will immediately trigger again upon exit
		 * even though there is no data left to send.
		 */
		USART_IntDisable(USART1, USART_IEN_TXBL);
	}

}

void print(char *s, int len)
{
	buftx = s;
	buftxlen = len;
	USART_IntEnable(USART1, USART_IEN_TXBL);
	while (buftx != NULL)
		EMU_EnterEM1();

}

void help()
{
	char *h =
		"\r\n" 
		"mv (phi|theta) <([+-]deg|n|e|s|w)>                    # moves antenna\r\n"
		"cam (on|off)                                          # turns camera on or off\r\n"
		"(help|?|h)                                            # list of commands\r\n"
		"status                                                # display system status\r\n"
		"cal (theta|phi) <deg>                                 # Calibrates phi or theta\r\n"
		"speed (theta|phi) (0-100)                             # 0 means stop\r\n"
		"sat (load|track|list|search)                          # satellite commands\r\n";
	print(h, strlen(h));
}

int main(void)
{
	uint32_t i;

	// Chip errata
	CHIP_Init();

	// Initialize GPIO and USART1
	initCmu();
	initGpio();
	initUsart1();

	// Enable receive data valid interrupt
	USART_IntEnable(USART1, USART_IEN_RXDATAV);

	//print("Hello World", 11);
	help();
	// Enable transmit buffer level interrupt
	// USART_IntEnable(USART1, USART_IEN_TXBL);
	while (1)
	{
		EMU_EnterEM1();
	}
}
