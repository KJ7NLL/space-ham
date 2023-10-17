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

typedef struct ads111x
{
	// High byte
	uint16_t os:1;
	uint16_t mux:3;
	uint16_t pga:3;
	uint16_t mode:1;

	// Low byte
	uint16_t dr:3;
	uint16_t comp_mode:1;
	uint16_t comp_pol:1;
	uint16_t comp_lat:1;
	uint16_t comp_que:2;
} ads111x_t;

enum {
	// Address pointer register fields
	ADS111X_REG_CONV = 0,
	ADS111X_REG_CONFIG,
	ADS111X_REG_LO_THRESH,
	ADS111X_REG_HI_THRESH,

	// Operational status or single-shot conversion start
	ADS111X_OS_NOOP = 0,
	ADS111X_OS_START_SINGLE,

	ADS111X_MUX_A0_A1 = 0,
	ADS111X_MUX_A0_A3,
	ADS111X_MUX_A1_A3,
	ADS111X_MUX_A2_A3,
	ADS111X_MUX_A0_GND,
	ADS111X_MUX_A1_GND,
	ADS111X_MUX_A2_GND,
	ADS111X_MUX_A3_GND,

	// Programmable gain amplifier
	ADS111X_PGA_6144MV = 0,
	ADS111X_PGA_4096MV,
	ADS111X_PGA_2048MV,
	ADS111X_PGA_1024MV,
	ADS111X_PGA_0512MV,
	ADS111X_PGA_0256MV,

	// Sample mode
	ADS111X_MODE_CONT = 0,
	ADS111X_MODE_SINGLE,

	// Data rate
	ADS111X_DR_8_SPS = 0,
	ADS111X_DR_16_SPS,
	ADS111X_DR_32_SPS,
	ADS111X_DR_64_SPS,
	ADS111X_DR_128_SPS,
	ADS111X_DR_250_SPS,
	ADS111X_DR_475_SPS,
	ADS111X_DR_860_SPS,
};

void ads111x_init(ads111x_t *adc);
void ads111x_config(ads111x_t *adc, uint16_t addr);
float ads111x_measure(ads111x_t *adc, uint16_t addr);
float ads111x_measure_req(ads111x_t *adc, i2c_req_t *req);
