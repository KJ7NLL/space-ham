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
//  Copyright (C) 2022- by Ezekiel Wheeler, KJ7NLL.
//  All rights reserved.
//
//  The official website and doumentation for space-ham is available here:
//    https://www.kj7nll.radio/

#include <math.h>

#include "platform.h"

#include "i2c.h"
#include "i2c/drv8830.h"

void drv8830_set_speed(uint16_t devaddr, float speed)
{
	uint8_t cr0 = 0;

	uint8_t ispeed = 0;
	uint8_t direct = 0;
	
	ispeed = 63 * fabs(speed);

	if (ispeed == 0)
		direct = DRV8830_COAST;
	else if (speed < 0)
		direct = DRV8830_REVERSE;
	else
		direct = DRV8830_FORWARD;

	cr0 = (ispeed << 2) + direct;

	i2c_master_write(devaddr << 1, 0, &cr0, 1);
}
