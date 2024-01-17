
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

#define QMC5883L_SAMPLE_AVG 128

// This structure is cast-compatible with i2c_req_t.
typedef struct qmc5883l
{
	// This must be the first entry in the struct for casting to be possible
	i2c_req_t req;

	uint8_t control_reg_1_mode:2;
	uint8_t control_reg_1_odr:2;
	uint8_t control_reg_1_rng:2;
	uint8_t control_reg_1_osr:2;

	int16_t x[QMC5883L_SAMPLE_AVG], y[QMC5883L_SAMPLE_AVG], z[QMC5883L_SAMPLE_AVG];
	int8_t  x_idx, y_idx, z_idx; 
	int16_t min_x, max_x;
	int16_t min_y, max_y;
	int16_t min_z, max_z;
} qmc5883l_t;

enum {
	QMC5883L_DATA_X = 0x00,
	QMC5883L_DATA_Y = 0x02,
	QMC5883L_DATA_Z = 0x04,
	QMC5883L_CONTROL_REG_1 = 0x09,

	QMC5883L_MODE_STANDBY = 0,
	QMC5883L_MODE_CONTINUOUS,

	QMC5883L_DATA_RATE_10HZ = 0,
	QMC5883L_DATA_RATE_50HZ,
	QMC5883L_DATA_RATE_100HZ,
	QMC5883L_DATA_RATE_200HZ,

	QMC5883L_OVER_SAMPLE_512 = 0,
	QMC5883L_OVER_SAMPLE_256,
	QMC5883L_OVER_SAMPLE_128,
	QMC5883L_OVER_SAMPLE_64,

	QMC5883L_PLANE_XY = 0,
	QMC5883L_PLANE_XZ,
	QMC5883L_PLANE_YZ,
};

void qmc5883l_init(qmc5883l_t *mag);
void qmc5883l_config_write(qmc5883l_t *mag);
int16_t qmc5883l_measure_req_raw(qmc5883l_t *mag, uint8_t data_reg);
float qmc5883l_measure_req(qmc5883l_t *mag, uint8_t data_reg);
float qmc5883l_measure_req_plane(qmc5883l_t *mag, uint8_t plane);
qmc5883l_t *qmc5883l_measure_req_alloc(int devaddr);
void qmc5883l_measure_req_free(qmc5883l_t *req);
