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

#include "platform.h"

#ifdef __EFR32__

#define I2C_SCL_PORT gpioPortC
#define I2C_SDA_PORT gpioPortC
#define I2C_SCL 4
#define I2C_SDA 5

#else

#ifdef __ESP32__

#define I2C_SCL 11
#define I2C_SDA 10
#define I2C_TIMEOUT_MS 100

#endif

enum {
	i2cTransferDone = 0,
	i2cTransferInProgress,
	i2cTransferError,
	i2cTransferTimeout,
	i2cTransferNotImplemented,
};
#endif

#define I2C_TXBUFFER_SIZE 32

typedef volatile struct i2c_req_t
{
#ifdef __ESP32__
	i2c_device_config_t dev_cfg;
	i2c_master_dev_handle_t dev_handle;
#endif
	// The name of the request, in case it is useful.
	char *name;

	// Target i2c address in bus-address format.  
	// ie: AAAAAAAR where R is the read bit.
	uint16_t addr;

	// The register to read/write
	uint8_t target;

	// Number of bytes to transfer:
	uint8_t n_bytes;

	// Incremental buffer to read into:
	uint8_t *data;

	// Result buffer to copy into after the transfer completes.
	// If the i2c_req_t structure is reused, then the result
	// pointer always points to a valid previous measurement.
	//  - The result member can be NULL or it can point at data.
	//  - If result is NULL, then it is unused.
	//  - If result points at data, then `memcpy` is not invoked and result
	//    may contain partial data until the transfer succeeds.
	//    successfully.
	uint8_t *result;

	// This function is called when a result has completed
	void (*callback)(volatile struct i2c_req_t *);

	// The status of the current request.  Set this to 0
	// when starting a new request:
	volatile I2C_TransferReturn_TypeDef status;

	// This is true if the request has been completed.
	// It must be cleared before submitting a request
	volatile uint8_t complete:1;

	// Valid is true if result contains valid data
	volatile uint8_t valid:1;

	uint64_t complete_time;

	int sample_count, err_count;
} i2c_req_t;

void initI2C();

I2C_TransferReturn_TypeDef i2c_master_read(uint16_t slaveAddress, uint8_t targetAddress,
	uint8_t * rxBuff, uint8_t numBytes);

I2C_TransferReturn_TypeDef i2c_master_write(uint16_t slaveAddress, uint8_t targetAddress, uint8_t * txBuff, uint8_t numBytes);

I2C_TransferReturn_TypeDef i2c_req_submit_sync(i2c_req_t *req);
void i2c_req_submit_async(i2c_req_t *req);
void i2c_req_add_cont(i2c_req_t *req);
i2c_req_t *i2c_req_get_cont(uint16_t devaddr);
void i2c_req_set_cont(uint16_t devaddr, i2c_req_t *req);
int i2c_get_count();
i2c_req_t *i2c_req_alloc(size_t reqtype_size, size_t n_bytes, uint16_t busaddr);
void i2c_req_free(i2c_req_t *req);
const volatile struct linklist *i2c_req_cont_list();

void dump_req(i2c_req_t *req, char *msg);

i2c_master_bus_handle_t i2c_get_bus_handle();
