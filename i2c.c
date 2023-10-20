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
#include <string.h>

#include "em_device.h"
#include "em_chip.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_i2c.h"

#include "i2c.h"
#include "serial.h"
#include "linklist.h"

#define I2C_REQ_CONT_ARRAY_SIZE 128

volatile int count = 0;
volatile i2c_req_t *req_active = NULL;
volatile struct linklist i2c_req_once = { .head = NULL, .tail = NULL };
volatile struct linklist i2c_req_cont = { .head = NULL, .tail = NULL };
volatile struct llnode *i2c_req_cont_pos = NULL;
volatile i2c_req_t *i2c_req_cont_array[I2C_REQ_CONT_ARRAY_SIZE] = {NULL};

I2C_TransferSeq_TypeDef i2c0_transfer;

// This only works with I2C0. Refactor if you need I2C1
i2c_req_t *i2c_handle_req(i2c_req_t *req)
{
	// This may seem strange, but a status value of i2cTransferDone
	// indicates that a new I2C request should be started. This
	// allows us to reuse a complete request as a new request
	// without setting it back up.
	if (req->status == i2cTransferDone)
	{
		req->status = i2cTransferInProgress;

		// Initializing I2C transfer
		i2c0_transfer.addr = req->addr;
		if (i2c0_transfer.addr & 0x01)
			i2c0_transfer.flags = I2C_FLAG_WRITE_READ;
		else
			i2c0_transfer.flags = I2C_FLAG_WRITE_WRITE;

		i2c0_transfer.buf[0].data = (void*)&req->target;
		i2c0_transfer.buf[0].len = 1;
		i2c0_transfer.buf[1].data = req->data;
		i2c0_transfer.buf[1].len = req->n_bytes;

		req->status = I2C_TransferInit(I2C0, &i2c0_transfer);
	}
	else if (req->status == i2cTransferInProgress)
		req->status = I2C_Transfer(I2C0);

	// Mark the request as complete whether or not it is sucessful
	if (req->status == i2cTransferDone || req->status != i2cTransferInProgress)
		req->complete = 1;

	// We completed successfully:
	if (req->status == i2cTransferDone)
	{
		if (req->result != NULL && req->result != req->data)
			memcpy(req->result, req->data, req->n_bytes);
	}

	return req;
}

void I2C0_IRQHandler()
{
	if (req_active != NULL)
	{
		i2c_handle_req(req_active);

		if (req_active->complete)
			req_active = NULL;
	}

	// See if there is a new "once" request pending
	if (req_active == NULL)
	{
		req_active = delete_node(&i2c_req_once, i2c_req_once.head);

		if (req_active != NULL)
		{
			req_active->status = 0;
			req_active->complete = 0;
			i2c_handle_req(req_active);
		}
	}

	// See if there is a continuous request pending
	if (req_active == NULL)
	{
		if (i2c_req_cont_pos == NULL)
			i2c_req_cont_pos = i2c_req_cont.head;

		if (i2c_req_cont_pos != NULL)
		{
			req_active = i2c_req_cont_pos->data;
			i2c_req_cont_pos = i2c_req_cont_pos->next;
		}

		if (req_active != NULL)
		{
			req_active->status = 0;
			req_active->complete = 0;
			i2c_handle_req(req_active);
		}
	}

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

i2c_req_t *i2c_req_get_cont(uint16_t devaddr)
{
	if (devaddr < I2C_REQ_CONT_ARRAY_SIZE)
		return i2c_req_cont_array[devaddr];
	else
		return NULL;
}

void i2c_req_set_cont(uint16_t devaddr, i2c_req_t *req)
{
	if (devaddr < I2C_REQ_CONT_ARRAY_SIZE)
		i2c_req_cont_array[devaddr] = req;
}

void i2c_req_add_cont(i2c_req_t *req)
{
	uint16_t devaddr = req->addr >> 1;

	req->status = 0;
	req->complete = 0;

	// Allow reverse lookups by device address
	if (devaddr < I2C_REQ_CONT_ARRAY_SIZE)
	{
		i2c_req_cont_array[devaddr] = req;
	}

	add_tail_node(&i2c_req_cont, req);

	// It might be strange to call the IRQ handler directly, but it is
	// responsible for setting up the transfer via I2C_TransferInit(), and
	// I2C_TransferInit() is safe to run inside _or_ outside of an
	// interrupt handler.  We cannot setup the new request here because an
	// existing transfer may be in progress so we cannot trigger
	// I2C_TransferInit().
	I2C0_IRQHandler();
}

void i2c_req_submit_async(i2c_req_t *req)
{
	req->status = 0;
	req->complete = 0;
	add_tail_node(&i2c_req_once, req);

	// It might be strange to call the IRQ handler directly, but it is
	// responsible for setting up the transfer via I2C_TransferInit(), and
	// I2C_TransferInit() is safe to run inside _or_ outside of an
	// interrupt handler.  We cannot setup the new request here because an
	// existing transfer may be in progress so we cannot trigger
	// I2C_TransferInit().
	I2C0_IRQHandler();
}

I2C_TransferReturn_TypeDef i2c_req_submit_sync(i2c_req_t *req)
{
	i2c_req_submit_async(req);
	while (!req->complete)
		EMU_EnterEM1();

	return req->status;
}

I2C_TransferReturn_TypeDef i2c_master_read(uint16_t slaveAddress, uint8_t targetAddress,
		uint8_t * rxBuff, uint8_t numBytes)
{
	i2c_req_t req;

	req.name = NULL;
	req.addr = slaveAddress | 1;
	req.target = targetAddress;
	req.n_bytes = numBytes;
	req.data = rxBuff;
	req.result = NULL;

	// Sending data
	i2c_req_submit_sync(&req);
	printf("read result: %d (count=%d)\r\n", req.status, count);

	return req.status;
}

I2C_TransferReturn_TypeDef i2c_master_write(uint16_t slaveAddress, uint8_t targetAddress,
		     uint8_t * txBuff, uint8_t numBytes)
{
	i2c_req_t req;

	req.name = NULL;
	req.addr = slaveAddress & 0xFFFE;
	req.target = targetAddress;
	req.n_bytes = numBytes;
	req.data = txBuff;
	req.result = NULL;

	i2c_req_submit_sync(&req);
	printf("write result: %d (count=%d)\r\n", req.status, count);

	return req.status;
}
