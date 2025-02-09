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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "i2c.h"
#include "i2c/mxc4005xc.h"

void mxc4005xc_init(mxc4005xc_t *mag)
{
	// Nothing to do
}

void mxc4005xc_config_write(mxc4005xc_t *mag)
{
	uint8_t data[4];
	uint16_t devaddr = mag->req.addr >> 1;

	// You may wish to configure these
	data[0] = (mag->control_reg_pd << 0) |
		  (mag->control_reg_fsr << 5);


	printf("mxc4005xc config: %x: %x\r\n", devaddr, data[0]);

	// In order for the user to be able to set full-scale range, the FSRE
	// (FSR enable) bit in register address 0x19 must be set to 1 by
	// factory programming. If FSRE is 0, the FSR[1:0] bits are ignored,
	// and MXC400xXC is set to the ±2g range.
	//
	// https://www.memsic.com/thermal-accelerometer-79

	i2c_master_write(devaddr << 1, MXC4005XC_CONTROL_REG, data, 4);
}

int16_t mxc4005xc_measure_req_raw(mxc4005xc_t *mag, uint8_t data_reg)
{
	uint8_t *data = mag->req.result;

	if (data == NULL || !mag->req.valid)
		return 0;

	// The enum starts at 0x03 while data[] starts at 0. subtrackt 3 so
	// that they match.
	data_reg = data_reg - 3;
	return (data[data_reg+0] << 8) | data[data_reg+1];
}

int16_t mxc4005xc_measure_req_avg(int16_t *arr)
{
	int32_t avg = 0;

	int i;
	
	for (i = 0; i < MXC4005XC_SAMPLE_AVG; i++)
	{
		avg += (int32_t)arr[i];
	}

	avg /= MXC4005XC_SAMPLE_AVG;

	return (int16_t)avg;
}

float mxc4005xc_measure_req(mxc4005xc_t *mag, uint8_t data_reg)
{
	int16_t value = 0;

	if (data_reg == MXC4005XC_DATA_X)
	{
		value = mxc4005xc_measure_req_avg(mag->x);
	}
	else if (data_reg == MXC4005XC_DATA_Y)
	{
		value = mxc4005xc_measure_req_avg(mag->y);
	}
	else if (data_reg == MXC4005XC_DATA_Z)
	{
		value = mxc4005xc_measure_req_avg(mag->z);
	}
	else
		return NAN;

	return (value/16)/2047.0;
}

float mxc4005xc_measure_req_plane(mxc4005xc_t *mag, uint8_t plane)
{
	float a, b;
	bool t;

	if (plane == MXC4005XC_PLANE_XY)
	{
		a = mxc4005xc_measure_req(mag, MXC4005XC_DATA_X);
		b = mxc4005xc_measure_req(mag, MXC4005XC_DATA_Y);

		if (mag->invert_x)
			a = -a;

		if (mag->invert_y)
			b = -b;

		if (mag->swap_xy)
		{
			t = a;
			a = b;
			b = t;
		}
	}
	else if (plane == MXC4005XC_PLANE_XZ)
	{
		a = mxc4005xc_measure_req(mag, MXC4005XC_DATA_X);
		b = mxc4005xc_measure_req(mag, MXC4005XC_DATA_Z);

		if (mag->invert_x)
			a = -a;

		if (mag->invert_z)
			b = -b;

		if (mag->swap_xz)
		{
			t = a;
			a = b;
			b = t;
		}
	}
	else if (plane == MXC4005XC_PLANE_YZ)
	{
		a = mxc4005xc_measure_req(mag, MXC4005XC_DATA_Y);
		b = mxc4005xc_measure_req(mag, MXC4005XC_DATA_Z);

		if (mag->invert_y)
			a = -a;

		if (mag->invert_z)
			b = -b;

		if (mag->swap_yz)
		{
			t = a;
			a = b;
			b = t;
		}
	}
	else
		return NAN;

	float ang = -90 + atan2(a, b) * 180 / M_PI;
	if (ang < -180)
		ang += 360;
	else if (ang > 180)
		ang -= 360;
	return ang;
}

void mxc4005xc_calibration_callback(mxc4005xc_t *mag)
{
	int16_t *xp, *yp, *zp;
	int16_t x, y, z;
	mag->x_idx = (mag->x_idx + 1) % MXC4005XC_SAMPLE_AVG;
	mag->y_idx = (mag->y_idx + 1) % MXC4005XC_SAMPLE_AVG;
	mag->z_idx = (mag->z_idx + 1) % MXC4005XC_SAMPLE_AVG;

	xp = &mag->x[mag->x_idx];
	yp = &mag->y[mag->y_idx];
	zp = &mag->z[mag->z_idx];

	*xp = mxc4005xc_measure_req_raw(mag, MXC4005XC_DATA_X);
	*yp = mxc4005xc_measure_req_raw(mag, MXC4005XC_DATA_Y);
	*zp = mxc4005xc_measure_req_raw(mag, MXC4005XC_DATA_Z);
}

mxc4005xc_t *mxc4005xc_measure_req_alloc(int devaddr)
{
	mxc4005xc_t *mag;

	mag = (mxc4005xc_t *)i2c_req_alloc(sizeof(mxc4005xc_t), 6, (devaddr << 1) | 1);

	if (mag == NULL)
		return NULL;

	i2c_req_t *req = &mag->req;

	req->name = "mxc4005xc";
	req->target = MXC4005XC_DATA_X;
	req->callback = (void (*)(i2c_req_t *))mxc4005xc_calibration_callback;

	mxc4005xc_init(mag);

	return mag;
}

void mxc4005xc_measure_req_free(mxc4005xc_t *mag)
{
	if (&mag->req == i2c_req_get_cont(mag->req.addr >> 1))
	{
		i2c_req_set_cont(mag->req.addr >> 1, NULL);
	}

	free(mag->req.data);
	free(mag->req.result);
	free(mag);
}
