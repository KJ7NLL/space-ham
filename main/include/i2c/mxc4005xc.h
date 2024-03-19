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

#define MXC4005XC_SAMPLE_AVG 128

// This structure is cast-compatible with i2c_req_t.
typedef struct mxc4005xc
{
	// This must be the first entry in the struct for casting to be possible
	i2c_req_t req;

	uint8_t control_reg_odr;

	uint8_t control_reg_fsr:2;
	uint8_t control_reg_pd:1;

	int8_t  x_idx, y_idx, z_idx; 

	int16_t x[MXC4005XC_SAMPLE_AVG], y[MXC4005XC_SAMPLE_AVG], z[MXC4005XC_SAMPLE_AVG];
	bool swap_xy;
	bool swap_xz;
	bool swap_yz;
	bool invert_y;
	bool invert_x;
	bool invert_z;
} mxc4005xc_t;

enum {
	MXC4005XC_DATA_X = 0x03,
	MXC4005XC_DATA_Y = 0x05,
	MXC4005XC_DATA_Z = 0x07,
	MXC4005XC_DATA_TEMP = 0x09,

	MXC4005XC_CONTROL_REG = 0x0D,

	MXC4005XC_CONTROL_REG_FSR_2G = 0,
	MXC4005XC_CONTROL_REG_FSR_4G,
	MXC4005XC_CONTROL_REG_FSR_8G,

	MXC4005XC_PLANE_XY = 0,
	MXC4005XC_PLANE_XZ,
	MXC4005XC_PLANE_YZ,
};

void mxc4005xc_init(mxc4005xc_t *mag);
void mxc4005xc_config_write(mxc4005xc_t *mag);
int16_t mxc4005xc_measure_req_raw(mxc4005xc_t *mag, uint8_t data_reg);
float mxc4005xc_measure_req(mxc4005xc_t *mag, uint8_t data_reg);
float mxc4005xc_measure_req_plane(mxc4005xc_t *mag, uint8_t plane);
mxc4005xc_t *mxc4005xc_measure_req_alloc(int devaddr);
void mxc4005xc_measure_req_free(mxc4005xc_t *req);
