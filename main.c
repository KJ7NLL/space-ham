
																			  /*****************************************************************************
 * @file main.c
 * @brief This project demonstrates use of the USART with interrupts.
 *
 * After initialization, the MCU goes into EM1 where the receive interrupt
 * handler buffers incoming data until the reception of 80 characters or a
 * CR (carriage return, ASCII 0x0D).
 *
 * Once the CR or 80 characters is hit, the receive data valid interrupt is
 * disabled, and the transmit buffer level interrupt, which, by default, is set
 * after power-on, is is enabled.  Each entry into the transmit interrupt
 * handler causes a character to be sent until all the characters originally
 * received have been echoed.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *******************************************************************************
 * # Evaluation Quality
 * This code has been minimally tested to ensure that it builds and is suitable 
 * as a demonstration for evaluation purposes only. This code will be maintained
 * at the sole discretion of Silicon Labs.
 ******************************************************************************/

#include "em_device.h"
#include "em_chip.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"

// Size of the buffer for received data
#define BUFLEN  80

// Receive data buffer
uint8_t buffer[BUFLEN];

// Current position ins buffer
uint32_t inpos = 0;
uint32_t outpos = 0;

// True while receiving data (waiting for CR or BUFLEN characters)
bool receive = true;

/****************************************************************************
 * @brief
 *    GPIO initialization
 *****************************************************************************/
void initGpio(void)
{
	// Configure PA5 as an output (TX)
	GPIO_PinModeSet(gpioPortA, 5, gpioModePushPull, 0);

	// Configure PA6 as an input (RX)
	GPIO_PinModeSet(gpioPortA, 6, gpioModeInput, 0);

	// Configure PB4 as output and set high (VCOM_ENABLE)
	// comment out next line to disable VCOM
	GPIO_PinModeSet(gpioPortD, 4, gpioModePushPull, 1);
}

/****************************************************************************
 * @brief
 *    CMU initialization
 *****************************************************************************/
void initCmu(void)
{
	// Enable clock to GPIO and USART1
	CMU_ClockEnable(cmuClock_GPIO, true);
	CMU_ClockEnable(cmuClock_USART1, true);

}

/****************************************************************************
 * @brief
 *    USART1 initialization
 *****************************************************************************/
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

/****************************************************************************
 * @brief
 *    The USART1 receive interrupt saves incoming characters.
 *****************************************************************************/
void USART1_RX_IRQHandler(void)
{
	// Get the character just received
	buffer[inpos] = USART1->RXDATA;

	// Exit loop on new line or buffer full
	if ((buffer[inpos] != '\r') && (inpos < BUFLEN))
		inpos++;
	else
		receive = false;	// Stop receiving on CR
}

/****************************************************************************
 * @brief
 *    The USART1 transmit interrupt outputs characters.
 *****************************************************************************/
void USART1_TX_IRQHandler(void)
{
	// Send a previously received character
	if (outpos <= inpos)
		USART1->TXDATA = buffer[outpos++];
	else
		/*
		 * Need to disable the transmit buffer level interrupt in this IRQ
		 * handler when done or it will immediately trigger again upon exit
		 * even though there is no data left to send.
		 */
	{
		receive = true;	// Go back into receive when all is sent
		USART_IntDisable(USART1, USART_IEN_TXBL);
	}
}

/****************************************************************************
 * @brief
 *    Main function
 *****************************************************************************/
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
	// Enable transmit buffer level interrupt
	USART_IntEnable(USART1, USART_IEN_TXBL);
	while (1)
	{
		EMU_EnterEM1();
	}
}
