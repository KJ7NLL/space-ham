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

#include "ff.h"
#include "fatfs-util.h"

#include "i2c.h"
#include "i2c/mmc5603nj.h"

#include "config.h"

void mmc5603nj_init(mmc5603nj_t *mag)
{
	// Initializes max value to the smallest possible value to
	// ensure that the mins and maxes are changed to proper values
	mag->cal.min_x = 65535;
	mag->cal.max_x = 0;
	mag->cal.min_y = 65535;
	mag->cal.max_y = 0;
	mag->cal.min_z = 65535;
	mag->cal.max_z = 0;
}

void mmc5603nj_config_write(mmc5603nj_t *mag)
{
	uint8_t data[4];
	uint16_t devaddr = mag->req.addr >> 1;

	data[0] = (mag->control_reg_odr << 0);

	data[1] = (mag->control_reg_0_take_meas_m << 0) |
		  (mag->control_reg_0_take_meas_t << 1) |
		  (mag->control_reg_0_start_mdt << 2) |
		  (mag->control_reg_0_do_set << 3) |
		  (mag->control_reg_0_do_reset << 4) |
		  (mag->control_reg_0_auto_sr_en << 5) |
		  (mag->control_reg_0_auto_st_en << 6) |
		  (mag->control_reg_0_cmm_freq_en << 7);

	data[2] = (mag->control_reg_1_bw << 0) |
		  (mag->control_reg_1_x_inhibit << 2) |
		  (mag->control_reg_1_y_inhibit << 3) |
		  (mag->control_reg_1_z_inhibit << 4) |
		  (mag->control_reg_1_st_enp << 5) |
		  (mag->control_reg_1_st_enm << 6) |
		  (mag->control_reg_1_sw_reset << 7);

	data[3] = (mag->control_reg_2_prd_set << 0) |
		  (mag->control_reg_2_en_prd_set << 3) |
		  (mag->control_reg_2_cmm_en << 4) |
		  (mag->control_reg_2_int_mdt_en << 5) |
		  (mag->control_reg_2_int_meas_done_en << 6) |
		  (mag->control_reg_2_hpower << 7);

	printf("mmc5603nj config: %x: %x\r\n", devaddr, data[0]);
	printf("mmc5603nj config: %x: %x\r\n", devaddr, data[1]);
	printf("mmc5603nj config: %x: %x\r\n", devaddr, data[2]);
	printf("mmc5603nj config: %x: %x\r\n", devaddr, data[3]);
	i2c_master_write(devaddr << 1, MMC5603NJ_CONTROL_REG_ODR, data, 4);
}

uint16_t mmc5603nj_measure_req_raw(mmc5603nj_t *mag, uint8_t data_reg)
{
	uint8_t *data = mag->req.result;

	if (data == NULL || !mag->req.valid)
		return 0;

	return (data[data_reg+0] << 8) | data[data_reg+1];
}

uint16_t mmc5603nj_measure_req_avg(uint16_t *arr)
{
	uint32_t avg = 0;

	int i;
	
	for (i = 0; i < MMC5603NJ_SAMPLE_AVG; i++)
	{
		avg += (uint32_t)arr[i];
	}

	avg /= MMC5603NJ_SAMPLE_AVG;

	return (uint16_t)avg;
}

float mmc5603nj_measure_req(mmc5603nj_t *mag, uint8_t data_reg)
{
	uint16_t value = 0;
	uint16_t min, max;

	if (data_reg == MMC5603NJ_DATA_X)
	{
		value = mmc5603nj_measure_req_avg(mag->x);
		min = mag->cal.min_x;
		max = mag->cal.max_x;
	}
	else if (data_reg == MMC5603NJ_DATA_Y)
	{
		value = mmc5603nj_measure_req_avg(mag->y);
		min = mag->cal.min_y;
		max = mag->cal.max_y;
	}
	else if (data_reg == MMC5603NJ_DATA_Z)
	{
		value = mmc5603nj_measure_req_avg(mag->z);
		min = mag->cal.min_z;
		max = mag->cal.max_z;
	}
	else
		return NAN;

	
	return ((1.0 / (max - min)) * (value - min)) * 2 - 1;
}

float mmc5603nj_measure_req_plane(mmc5603nj_t *mag, uint8_t plane)
{
	float a, b;
	bool t;

	if (plane == MMC5603NJ_PLANE_XY)
	{
		a = mmc5603nj_measure_req(mag, MMC5603NJ_DATA_X);
		b = mmc5603nj_measure_req(mag, MMC5603NJ_DATA_Y);

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
	else if (plane == MMC5603NJ_PLANE_XZ)
	{
		a = mmc5603nj_measure_req(mag, MMC5603NJ_DATA_X);
		b = mmc5603nj_measure_req(mag, MMC5603NJ_DATA_Z);

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
	else if (plane == MMC5603NJ_PLANE_YZ)
	{
		a = mmc5603nj_measure_req(mag, MMC5603NJ_DATA_Y);
		b = mmc5603nj_measure_req(mag, MMC5603NJ_DATA_Z);

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

	return 180 + atan2(a, b) * 180 / M_PI;
}

void mmc5603nj_calibration_callback(mmc5603nj_t *mag)
{
	uint16_t *xp, *yp, *zp;
	uint16_t x, y, z;
	uint16_t xa, ya, za;
	mag->x_idx = (mag->x_idx + 1) % MMC5603NJ_SAMPLE_AVG;
	mag->y_idx = (mag->y_idx + 1) % MMC5603NJ_SAMPLE_AVG;
	mag->z_idx = (mag->z_idx + 1) % MMC5603NJ_SAMPLE_AVG;

	xp = &mag->x[mag->x_idx];
	yp = &mag->y[mag->y_idx];
	zp = &mag->z[mag->z_idx];

	x = mmc5603nj_measure_req_raw(mag, MMC5603NJ_DATA_X);
	y = mmc5603nj_measure_req_raw(mag, MMC5603NJ_DATA_Y);
	z = mmc5603nj_measure_req_raw(mag, MMC5603NJ_DATA_Z);


	if (x != 0 && x != 65535)
	{
		*xp = x;

		if (mag->calibrate)
			xa = mmc5603nj_measure_req_avg(mag->x);

		if (mag->calibrate && xa > mag->cal.max_x)
			mag->cal.max_x = xa;

		if (mag->calibrate && xa < mag->cal.min_x)
			mag->cal.min_x = xa;
	}

	if (y != 0 && y != 65535)
	{
		*yp = y;

		if (mag->calibrate)
			ya = mmc5603nj_measure_req_avg(mag->y);

		if (mag->calibrate && ya > mag->cal.max_y)
			mag->cal.max_y = ya;

		if (mag->calibrate && ya < mag->cal.min_y)
			mag->cal.min_y = ya;
	}

	if (z != 0 && z != 65535)
	{
		*zp = z;

		if (mag->calibrate)
			za = mmc5603nj_measure_req_avg(mag->z);

		if (mag->calibrate && za > mag->cal.max_z)
			mag->cal.max_z = za;

		if (mag->calibrate && za < mag->cal.min_z)
			mag->cal.min_z = za;
	}
}

mmc5603nj_t *mmc5603nj_measure_req_alloc(int devaddr)
{
	mmc5603nj_t *mag;

	mag = (mmc5603nj_t *)i2c_req_alloc(sizeof(mmc5603nj_t), 6, (devaddr << 1) | 1);

	if (mag == NULL)
		return NULL;

	i2c_req_t *req = &mag->req;

	req->name = "mmc5603nj";
	req->target = MMC5603NJ_DATA_X;
	req->callback = (void (*)(i2c_req_t *))mmc5603nj_calibration_callback;

	mmc5603nj_init(mag);

	return mag;
}

void mmc5603nj_measure_req_free(mmc5603nj_t *mag)
{
	if (&mag->req == i2c_req_get_cont(mag->req.addr >> 1))
	{
		i2c_req_set_cont(mag->req.addr >> 1, NULL);
	}

	free(mag->req.data);
	free(mag->req.result);
	free(mag);
}

FRESULT mmc5603nj_cal_save(mmc5603nj_t *mag, char *filename)
{
	return f_write_file(filename, &mag->cal, sizeof(mag->cal));
}

FRESULT mmc5603nj_cal_load(mmc5603nj_t *mag, char *filename)
{
	return f_read_file(filename, &mag->cal, sizeof(mag->cal));
}
