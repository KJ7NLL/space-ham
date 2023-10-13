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

void ads111x_init(ads111x_t *adc);
void ads111x_config(ads111x_t *adc, uint16_t addr);
