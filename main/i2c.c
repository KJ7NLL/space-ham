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
#include <stdio.h>
#include <string.h>

#include "platform.h"

// Only Espressif's LVGL port includes I2C locking, so use their lock to make
// i2c thread safe:
#include "esp_lvgl_port.h"

#include "i2c.h"
#include "lcd.h"
#include "serial.h"
#include "linklist.h"
#include "rtcc.h"
#include "config.h"

#define I2C_REQ_CONT_ARRAY_SIZE 128

volatile int count = 0;
volatile i2c_req_t *req_active = NULL;
volatile struct linklist i2c_req_once = { .head = NULL, .tail = NULL };
volatile struct linklist i2c_req_cont = { .head = NULL, .tail = NULL };
volatile struct llnode *i2c_req_cont_pos = NULL;
volatile i2c_req_t *i2c_req_cont_array[I2C_REQ_CONT_ARRAY_SIZE] = {NULL};

#ifdef __EFR32__
I2C_TransferSeq_TypeDef i2c0_transfer;
#endif

#ifdef __ESP32__
i2c_master_bus_handle_t i2c_bus_handle;
#endif

i2c_master_bus_handle_t i2c_get_bus_handle()
{
	return i2c_bus_handle;
}

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

#ifdef __ESP32__
		if (req->addr & 0x01)
		{
			esp_err_t e;
			if (lvgl_port_lock(0))
			{
				e = i2c_master_transmit_receive(req->dev_handle, (void *) &req->target, 1,
					req->data, req->n_bytes, I2C_TIMEOUT_MS);
				lvgl_port_unlock();
			}
			else
			{
				printf("%s: failed to hold semaphore\r\n", __func__);
				req->status = i2cTransferError;
			}

			if (e == ESP_OK)
				req->status = i2cTransferDone;
			else
			{
				ESP_ERROR_CHECK(e);
				req->status = i2cTransferError;
			}
		}
		else
		{
			// TODO: Implement write
			req->status = i2cTransferNotImplemented;
			req->complete = 1;
		}

#endif

#ifdef __EFR32__
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
#endif
	}
#ifdef __EFR32__
	else if (req->status == i2cTransferInProgress)
		req->status = I2C_Transfer(I2C0);
#endif

	// Mark the request as complete whether or not it is sucessful
	if (req->status == i2cTransferDone || req->status != i2cTransferInProgress)
		req->complete = 1;

	// We completed successfully:
	if (req->status == i2cTransferDone)
	{
		req->complete_time = rtcc_get_sec();
		req->sample_count++;
		req->valid = 1;
		if (req->result != NULL && req->result != req->data)
			memcpy(req->result, req->data, req->n_bytes);

		if (req->callback != NULL)
			req->callback(req);
	}
	else if (req->complete && req->status != i2cTransferInProgress)
	{
		req->err_count++;
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
		req_active = delete_node((struct linklist*)&i2c_req_once, i2c_req_once.head);

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
		{
			i2c_req_cont_pos = i2c_req_cont.head;

			count++;
		}

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

#ifdef __ESP32__

	// If there are no active requests, sleep for 100 ms
	if (req_active == NULL)
	{
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
#endif
}

void i2c_req_task(void *p)
{
	while (1)
	{
		I2C0_IRQHandler();
		vTaskDelay(1);
	}
}

// This function is mostly coppied from a Silicon Labs example. If anyone
// cares, this function may still be licensed as zlib.
void initI2C()
{
#ifdef __ESP32__
	i2c_master_bus_config_t i2c_mst_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.i2c_port = -1,
		.scl_io_num = I2C_SCL,
		.sda_io_num = I2C_SDA,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};

	ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

	init_lcd();

	i2c_bus_mutex = xSemaphoreCreateMutex();

	xTaskCreate(i2c_req_task,
		"i2c_req_task",
		4096,
		NULL,
		0,
		NULL);
#endif
#ifdef __EFR32__
	CMU_ClockEnable(cmuClock_I2C0, true);

	// Using default settings
	I2C_Init_TypeDef i2cInit = I2C_INIT_DEFAULT;
	i2cInit.freq = config.i2c_freq;

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
#endif
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
	req->valid = 0;

	// Allow reverse lookups by device address
	if (devaddr < I2C_REQ_CONT_ARRAY_SIZE)
	{
		i2c_req_cont_array[devaddr] = req;
	}

	add_tail_node((struct linklist*)&i2c_req_cont, (void*)req);

#ifdef __EFR32__
	// It might be strange to call the IRQ handler directly, but it is
	// responsible for setting up the transfer via I2C_TransferInit(), and
	// I2C_TransferInit() is safe to run inside _or_ outside of an
	// interrupt handler.  We cannot setup the new request here because an
	// existing transfer may be in progress so we cannot trigger
	// I2C_TransferInit().

	I2C0_IRQHandler();
#endif
}

void i2c_req_submit_async(i2c_req_t *req)
{
	req->status = 0;
	req->complete = 0;
	req->valid = 0;
	add_tail_node((struct linklist*)&i2c_req_once, (void*)req);

#ifdef __EFR32__
	// It might be strange to call the IRQ handler directly, but it is
	// responsible for setting up the transfer via I2C_TransferInit(), and
	// I2C_TransferInit() is safe to run inside _or_ outside of an
	// interrupt handler.  We cannot setup the new request here because an
	// existing transfer may be in progress so we cannot trigger
	// I2C_TransferInit().
	I2C0_IRQHandler();
#endif
}

I2C_TransferReturn_TypeDef i2c_req_submit_sync(i2c_req_t *req)
{
	i2c_req_submit_async(req);
	while (!req->complete)
		platform_sleep();

	return req->status;
}

I2C_TransferReturn_TypeDef i2c_master_read(uint16_t slaveAddress, uint8_t targetAddress,
		uint8_t *rxBuff, uint8_t numBytes)
{
	i2c_req_t req;

	memset((void*)&req, 0, sizeof(i2c_req_t));

	req.addr = slaveAddress | 1;
	req.target = targetAddress;
	req.n_bytes = numBytes;
	req.data = rxBuff;

#ifdef __ESP32__
	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = slaveAddress >> 1,
		.scl_speed_hz = 10000,
	};

	i2c_master_dev_handle_t dev_handle;

	if (lvgl_port_lock(0))
	{
		ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));

		req.status = i2c_master_transmit_receive(dev_handle, &targetAddress, 1, rxBuff, numBytes, -1);
		ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));

		lvgl_port_unlock();
	}
	else
	{
		printf("%s: failed to hold semaphore\r\n", __func__);
		req.status = -1;
	}
#else
	i2c_req_submit_sync(&req);
#endif
	printf("read result: %d (count=%d)\r\n", req.status, count);

	return req.status;
}

I2C_TransferReturn_TypeDef i2c_master_write(uint16_t slaveAddress, uint8_t targetAddress,
		     uint8_t * txBuff, uint8_t numBytes)
{
	i2c_req_t req;

	memset((void*)&req, 0, sizeof(i2c_req_t));

	req.addr = slaveAddress & 0xFFFE;
	req.target = targetAddress;
	req.n_bytes = numBytes;
	req.data = txBuff;
#if defined(__EFR32__)
	i2c_req_submit_sync(&req);
#elif defined(__ESP32__)
	req.data = malloc(numBytes + 1);

	if (req.data != NULL)
	{
		req.data[0] = targetAddress;
		memcpy(req.data + 1, txBuff, numBytes);

		i2c_device_config_t dev_cfg = {
			.dev_addr_length = I2C_ADDR_BIT_LEN_7,
			.device_address = slaveAddress >> 1,
			.scl_speed_hz = 10000,
		};

		i2c_master_dev_handle_t dev_handle;

		if (lvgl_port_lock(0))
		{
			ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));

			req.status = i2c_master_transmit(dev_handle, req.data, numBytes + 1, -1);
			ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));

			req.status = 0;
			free(req.data);

			lvgl_port_unlock();
		}
		else
		{
			printf("%s: failed to hold semaphore\r\n", __func__);
			req.status = -1;
		}
	}
	else
		req.status = -1;
#endif
	printf("write result: %d (count=%d)\r\n", req.status, count);

	return req.status;
}

// This is intended to display in status. It zeros but returns the count of
// complete i2c_req_cont iterations since the previous call. If you call this
// once per second, then you can estimate how many measurements per second are
// made across all i2c devices.
int i2c_get_count()
{
	int c = count;

	count = 0;

	return c;
}

i2c_req_t *i2c_req_alloc(size_t reqtype_size, size_t n_bytes, uint16_t busaddr)
{
	i2c_req_t *req;

	req = malloc(reqtype_size);
	if (req == NULL)
		return NULL;

	memset((void*)req, 0, reqtype_size);

	req->name = "i2c_req_alloc";
	req->addr = busaddr;
	req->n_bytes = n_bytes;

	req->data = malloc(req->n_bytes);
	if (req->data == NULL)
		goto out_req;

	req->result = malloc(req->n_bytes);
	if (req->result == NULL)
		goto out_data;

#ifdef __ESP32__
	req->dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
	req->dev_cfg.device_address = busaddr >> 1;
	req->dev_cfg.scl_speed_hz = config.i2c_freq;

	esp_err_t e;

	e = i2c_master_bus_add_device(i2c_bus_handle, &req->dev_cfg, &req->dev_handle);
	if (e != ESP_OK)
	{
		ESP_ERROR_CHECK(e);
		goto out_result;
	}
#endif

	memset(req->data, 0, req->n_bytes);
	memset(req->result, 0, req->n_bytes);

	return req;
out_result:
	free(req->result);
out_data:
	free(req->data);
out_req:
	free((void*)req);

	return NULL;
}

void i2c_req_free(i2c_req_t *req)
{
	if (req == i2c_req_get_cont(req->addr >> 1))
	{
		i2c_req_set_cont(req->addr >> 1, NULL);
	}


#ifdef __ESP32__
	ESP_ERROR_CHECK(i2c_master_bus_rm_device(req->dev_handle));
#endif

	free(req->data);
	free(req->result);
	free((void*)req);
}

const volatile struct linklist *i2c_req_cont_list()
{
	return &i2c_req_cont;
}


void dump_req(i2c_req_t *req, char *msg)
{
	printf("\r\n%s: req=%p\r\n"
		"  req.name=%s\r\n"
		"  req.addr=%02x devaddr=%02x rw=%c\r\n"
		"  req.target=%d\r\n"
		"  req.n_bytes=%d\r\n"
		"  req.status=%d\r\n"
		"  req.complete=%d\r\n"
		"  req.valid=%d\r\n"
		"  req.complete_time=%d\r\n",
			msg,
			req,
			req->name,
			req->addr, req->addr >> 1, req->addr & 0x01 ? 'R' : 'W',
			req->target,
			req->n_bytes,
			req->status,
			req->complete,
			req->valid,
			(int)req->complete_time);

	printf("  req.data: %p\r\n", req->data);
	for (int i = 0; i < req->n_bytes; i++)
		printf("    %d. %02X\r\n", i, req->data[i]);

	printf("  req.result: %p\r\n", req->result);
	for (int i = 0; i < req->n_bytes; i++)
		printf("    %d. %02X\r\n", i, req->result[i]);
}
