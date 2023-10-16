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
#include <math.h>

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

float ads111x_measure(ads111x_t *adc, uint16_t addr)
{
	uint8_t data[2];
	i2c_req_t req;

	req.addr = (addr << 1) | 1;
	req.target = 0;
	req.n_bytes = 2;
	req.data = data;
	req.result = data;

	i2c_req_submit_sync(&req);

	return ads111x_measure_req(adc, &req);
}

float ads111x_measure_req(ads111x_t *adc, i2c_req_t *req)
{
	uint8_t *data = req->result;

	int ivalue;
	float value;

	if (data == NULL)
		return NAN;

	ivalue = (data[0] << 8) + data[1];

	value = (float)ivalue/32767.0;

	printf("value: %3.12f (%5d)\r\n", value, ivalue);

	switch (adc->pga)
	{
		case ADS111X_PGA_6144MV:
			return 6.144 * value;
		case ADS111X_PGA_4096MV:
			return 4.096 * value;
		case ADS111X_PGA_2048MV:
			return 2.048 * value;
		case ADS111X_PGA_1024MV:
			return 1.024 * value;
		case ADS111X_PGA_0512MV:
			return 0.512 * value;
		case ADS111X_PGA_0256MV:
			return 0.256 * value;
	}

	return NAN;
}
