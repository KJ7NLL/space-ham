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
#include "em_i2c.h"

#define I2C_SCL_PORT gpioPortC
#define I2C_SDA_PORT gpioPortC
#define I2C_SCL 0
#define I2C_SDA 1
#define I2C_TXBUFFER_SIZE 32

void initI2C();

I2C_TransferReturn_TypeDef i2c_master_read(uint16_t slaveAddress, uint8_t targetAddress,
	uint8_t * rxBuff, uint8_t numBytes);
void i2c_master_write(uint16_t slaveAddress, uint8_t targetAddress, uint8_t * txBuff, uint8_t numBytes);

