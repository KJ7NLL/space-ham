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
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "i2c.h"
#include "i2c/qmc5883l.h"

void qmc5883l_init(qmc5883l_t *mag)
{
	mag->control_reg_1_mode = 0x00;
	mag->control_reg_1_odr  = 0x00;
	mag->control_reg_1_rng  = 0x00;
	mag->control_reg_1_osr  = 0x00;

	// Initializes max value to the smallest possible value to
	// ensure that the mins and maxes are changed to proper values
	mag->min_x = 32767;
	mag->max_x = -32768;
	mag->min_y = 32767;
	mag->max_y = -32768;
	mag->min_z = 32767;
	mag->max_z = -32768;
}

void qmc5883l_config_write(qmc5883l_t *mag)
{
	uint8_t data[1];
	uint16_t devaddr = mag->req.addr >> 1;

	data[0] = (mag->control_reg_1_mode << 0) |
		(mag->control_reg_1_odr << 2) |
		(mag->control_reg_1_rng << 4) |
		(mag->control_reg_1_osr << 6);

	printf("qmc5883l config: %x: %x\r\n", devaddr, data[0]);
	i2c_master_write(devaddr << 1, QMC5883L_CONTROL_REG_1, data, 1);
}

int16_t qmc5883l_measure_req_raw(qmc5883l_t *mag, uint8_t data_reg)
{
	uint8_t *data = mag->req.result;

	if (data == NULL || !mag->req.valid)
		return -32768;

	return (data[data_reg+1] << 8) | data[data_reg+0];
}

int16_t qmc5883l_measure_req_avg(int16_t *arr)
{
	int32_t avg = 0;

	int i;
	
	for (i = 0; i < QMC5883L_SAMPLE_AVG; i++)
	{
		avg += (int32_t)arr[i];
	}

	avg /= QMC5883L_SAMPLE_AVG;

	return (int16_t)avg;
}

float qmc5883l_measure_req(qmc5883l_t *mag, uint8_t data_reg)
{
	int16_t value = 0;
	int16_t min, max;

	if (data_reg == QMC5883L_DATA_X)
	{
		value = qmc5883l_measure_req_avg(mag->x);
		min = mag->min_x;
		max = mag->max_x;
	}
	else if (data_reg == QMC5883L_DATA_Y)
	{
		value = qmc5883l_measure_req_avg(mag->y);
		min = mag->min_y;
		max = mag->max_y;
	}
	else if (data_reg == QMC5883L_DATA_Z)
	{
		value = qmc5883l_measure_req_avg(mag->z);
		min = mag->min_z;
		max = mag->max_z;
	}
	else
		return NAN;

	
	return ((1.0 / (max - min)) * (value - min)) * 2 - 1;
}

float qmc5883l_measure_req_plane(qmc5883l_t *mag, uint8_t plane)
{
	float a, b;

	if (plane == QMC5883L_PLANE_XY)
	{
		a = qmc5883l_measure_req(mag, QMC5883L_DATA_X);
		b = qmc5883l_measure_req(mag, QMC5883L_DATA_Y);
	}
	else if (plane == QMC5883L_PLANE_XZ)
	{
		a = qmc5883l_measure_req(mag, QMC5883L_DATA_X);
		b = qmc5883l_measure_req(mag, QMC5883L_DATA_Z);
	}
	else if (plane == QMC5883L_PLANE_YZ)
	{
		a = qmc5883l_measure_req(mag, QMC5883L_DATA_Y);
		b = qmc5883l_measure_req(mag, QMC5883L_DATA_Z);
	}
	else
		return NAN;

	return 180 + atan2(b, a) * 180 / M_PI;
}

void qmc5883l_calibration_callback(qmc5883l_t *mag)
{
	int16_t *x, *y, *z;
	mag->x_idx = (mag->x_idx + 1) % QMC5883L_SAMPLE_AVG;
	mag->y_idx = (mag->y_idx + 1) % QMC5883L_SAMPLE_AVG;
	mag->z_idx = (mag->z_idx + 1) % QMC5883L_SAMPLE_AVG;

	x = &mag->x[mag->x_idx];
	y = &mag->y[mag->y_idx];
	z = &mag->z[mag->z_idx];

	*x = qmc5883l_measure_req_raw(mag, QMC5883L_DATA_X);
	*y = qmc5883l_measure_req_raw(mag, QMC5883L_DATA_Y);
	*z = qmc5883l_measure_req_raw(mag, QMC5883L_DATA_Z);
	
	// Sets max to current value
	if (*x > mag->max_x)
		mag->max_x = *x;
	if (*y > mag->max_y)
		mag->max_y = *y;
	if (*z > mag->max_z)
		mag->max_z = *z;

	// Sets min to current value
	if (*x < mag->min_x)
		mag->min_x = *x;
	if (*y < mag->min_y)
		mag->min_y = *y;
	if (*z < mag->min_z)
		mag->min_z = *z;
}

qmc5883l_t *qmc5883l_measure_req_alloc(int devaddr)
{
	qmc5883l_t *mag;

	mag = (qmc5883l_t *)i2c_req_alloc(sizeof(qmc5883l_t), 6);

	if (mag == NULL)
		return NULL;

	i2c_req_t *req = &mag->req;

	req->name = "qmc5883l";
	req->addr = (devaddr << 1) | 1;
	req->target = QMC5883L_DATA_X;
	req->callback = (void (*)(i2c_req_t *))qmc5883l_calibration_callback;

	qmc5883l_init(mag);

	return mag;
}

void qmc5883l_measure_req_free(qmc5883l_t *mag)
{
	if (&mag->req == i2c_req_get_cont(mag->req.addr >> 1))
	{
		i2c_req_set_cont(mag->req.addr >> 1, NULL);
	}

	free(mag->req.data);
	free(mag->req.result);
	free(mag);
}
