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
#include <stdio.h>

#include "i2c.h"
#include "i2c/ads111x.h"


void ads111x_init(ads111x_t *adc)
{
	adc->os         =  ADS111X_OS_START_SINGLE;
	adc->mux        =  ADS111X_MUX_A0_A1;
	adc->pga        =  ADS111X_PGA_2048MV;
	adc->mode       =  ADS111X_MODE_SINGLE;
	adc->dr         =  ADS111X_DR_128_SPS;
	adc->comp_mode  =  0x00;
	adc->comp_pol   =  0x00;
	adc->comp_lat   =  0x00;
	adc->comp_que   =  0x03;
}

void ads111x_config(ads111x_t *adc, uint16_t addr)
{
	uint8_t data[2];

	// High byte
	data[0] = (adc->os << 7) |
		(adc->mux << 4) |
		(adc->pga << 1) |
		(adc->mode << 0);

	// Low byte
	data[1] = (adc->dr << 5) |
		(adc->comp_mode << 4) |
		(adc->comp_pol << 3) |
		(adc->comp_lat << 2) |
		(adc->comp_que << 0);

	printf("ads111x config: %x: %x %x\r\n", addr, data[0], data[1]);
	i2c_master_write(addr << 1, 1, data, 2);
}
