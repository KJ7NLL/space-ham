/*
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
 */

#include <stdio.h>

#include "em_device.h"
#include "em_chip.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_i2c.h"

#include "i2c.h"
#include "serial.h"

volatile I2C_TransferReturn_TypeDef result;
volatile int count = 0;

void I2C0_IRQHandler()
{
	result = I2C_Transfer(I2C0);
	count++;
}

void initI2C()
{
	CMU_ClockEnable(cmuClock_I2C0, true);

	// Using default settings
	I2C_Init_TypeDef i2cInit = I2C_INIT_DEFAULT;

	// Route GPIO pins to I2C module
	GPIO->I2CROUTE[0].SDAROUTE =
		(GPIO->I2CROUTE[0].SDAROUTE & ~_GPIO_I2C_SDAROUTE_MASK) |
		((I2C_SDA_PORT << _GPIO_I2C_SDAROUTE_PORT_SHIFT) |
		 (I2C_SDA << _GPIO_I2C_SDAROUTE_PIN_SHIFT));
	GPIO->I2CROUTE[0].SCLROUTE =
		(GPIO->I2CROUTE[0].SCLROUTE & ~_GPIO_I2C_SCLROUTE_MASK) |
		((I2C_SCL_PORT << _GPIO_I2C_SCLROUTE_PORT_SHIFT) |
		 (I2C_SCL << _GPIO_I2C_SCLROUTE_PIN_SHIFT));
	GPIO->I2CROUTE[0].ROUTEEN =
		GPIO_I2C_ROUTEEN_SDAPEN | GPIO_I2C_ROUTEEN_SCLPEN;

	I2C_Init(I2C0, &i2cInit);

	I2C0->CTRL = I2C_CTRL_AUTOSN;

	I2C_Enable(I2C0, true);

	I2C_IntClear(I2C0, _I2C_IF_MASK);
	I2C_IntEnable(I2C0,
		I2C_IEN_RXDATAV |
		I2C_IEN_ACK |
		I2C_IEN_NACK |
		I2C_IEN_SSTOP |
		I2C_IEN_TXC |
		I2C_IEN_BITO |
		I2C_IEN_CLTO
	);
	NVIC_EnableIRQ(I2C0_IRQn);
}

I2C_TransferReturn_TypeDef
i2c_master_read(uint16_t slaveAddress, uint8_t targetAddress,
		uint8_t * rxBuff, uint8_t numBytes)
{
	// Transfer structure
	I2C_TransferSeq_TypeDef i2cTransfer;

	// Initializing I2C transfer
	i2cTransfer.addr = slaveAddress;
	i2cTransfer.flags = I2C_FLAG_WRITE_READ;
	// i2cTransfer.flags = I2C_FLAG_READ;

	i2cTransfer.buf[0].data = &targetAddress;
	i2cTransfer.buf[0].len = 1;
	i2cTransfer.buf[1].data = rxBuff;
	i2cTransfer.buf[1].len = numBytes;

	result = I2C_TransferInit(I2C0, &i2cTransfer);

	// Sending data
	while (result == i2cTransferInProgress)
	{
		EMU_EnterEM1();
	}
	printf("read result: %d (count=%d)\r\n", result, count);

	return result;
}


void i2c_master_write(uint16_t slaveAddress, uint8_t targetAddress,
		     uint8_t * txBuff, uint8_t numBytes)
{
	// Transfer structure
	I2C_TransferSeq_TypeDef i2cTransfer;
	uint8_t txBuffer[I2C_TXBUFFER_SIZE + 1];

	if (numBytes > I2C_TXBUFFER_SIZE)
	{
		printf("Warning: I2C tx: numbytes exceeds buffer size, truncating: %d > %d\r\n",
			numBytes, I2C_TXBUFFER_SIZE);
		numBytes = I2C_TXBUFFER_SIZE;
	}

	txBuffer[0] = targetAddress;
	for (int i = 0; i < numBytes; i++)
	{
		txBuffer[i + 1] = txBuff[i];
	}

	// Initializing I2C transfer
	i2cTransfer.addr = slaveAddress;
	i2cTransfer.flags = I2C_FLAG_WRITE;
	i2cTransfer.buf[0].data = txBuffer;
	i2cTransfer.buf[0].len = numBytes + 1;
	i2cTransfer.buf[1].data = NULL;
	i2cTransfer.buf[1].len = 0;

	result = I2C_TransferInit(I2C0, &i2cTransfer);

	// Sending data
	while (result == i2cTransferInProgress)
	{
		EMU_EnterEM1();
	}
	printf("write result: %d (count=%d)\r\n", result, count);
}
