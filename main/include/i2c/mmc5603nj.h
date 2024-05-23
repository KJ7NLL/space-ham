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

#define MMC5603NJ_SAMPLE_AVG 128

// This structure is cast-compatible with i2c_req_t.
typedef struct mmc5603nj
{
	// This must be the first entry in the struct for casting to be possible
	i2c_req_t req;

	uint8_t control_reg_odr;

	uint8_t control_reg_0_take_meas_m:1;
	uint8_t control_reg_0_take_meas_t:1;
	uint8_t control_reg_0_start_mdt:1;
	uint8_t control_reg_0_do_set:1;
	uint8_t control_reg_0_do_reset:1;
	uint8_t control_reg_0_auto_sr_en:1;
	uint8_t control_reg_0_auto_st_en:1;
	uint8_t control_reg_0_cmm_freq_en:1;

	uint8_t control_reg_1_bw:2;
	uint8_t control_reg_1_x_inhibit:1;
	uint8_t control_reg_1_y_inhibit:1;
	uint8_t control_reg_1_z_inhibit:1;
	uint8_t control_reg_1_st_enp:1;
	uint8_t control_reg_1_st_enm:1;
	uint8_t control_reg_1_sw_reset:1;

	uint8_t control_reg_2_prd_set:3;
	uint8_t control_reg_2_en_prd_set:1;
	uint8_t control_reg_2_cmm_en:1;
	uint8_t control_reg_2_int_mdt_en:1;
	uint8_t control_reg_2_int_meas_done_en:1;
	uint8_t control_reg_2_hpower:1;

	int8_t  x_idx, y_idx, z_idx; 

	struct {
		uint16_t min_x, max_x;
		uint16_t min_y, max_y;
		uint16_t min_z, max_z;
	} cal;

	uint16_t x[MMC5603NJ_SAMPLE_AVG], y[MMC5603NJ_SAMPLE_AVG], z[MMC5603NJ_SAMPLE_AVG];
	bool swap_xy;
	bool swap_xz;
	bool swap_yz;
	bool invert_y;
	bool invert_x;
	bool invert_z;
	bool calibrate;
} mmc5603nj_t;

enum {
	MMC5603NJ_DATA_X = 0x00,
	MMC5603NJ_DATA_Y = 0x02,
	MMC5603NJ_DATA_Z = 0x04,
	MMC5603NJ_REG_ODR = 0x1A,
	MMC5603NJ_CONTROL_REG_ODR = 0x1A,

	MMC5603NJ_BW_75HZ = 0,
	MMC5603NJ_BW_150HZ,
	MMC5603NJ_BW_255HZ,
	MMC5603NJ_BW_1000HZ,

	MMC5603NJ_PRD_SET_1 = 0,
	MMC5603NJ_PRD_SET_25,
	MMC5603NJ_PRD_SET_75,
	MMC5603NJ_PRD_SET_100,
	MMC5603NJ_PRD_SET_250,
	MMC5603NJ_PRD_SET_500,
	MMC5603NJ_PRD_SET_1000,
	MMC5603NJ_PRD_SET_2000,

	MMC5603NJ_PLANE_XY = 0,
	MMC5603NJ_PLANE_XZ,
	MMC5603NJ_PLANE_YZ,
};

void mmc5603nj_init(mmc5603nj_t *mag);
void mmc5603nj_config_write(mmc5603nj_t *mag);
uint16_t mmc5603nj_measure_req_raw(mmc5603nj_t *mag, uint8_t data_reg);
float mmc5603nj_measure_req(mmc5603nj_t *mag, uint8_t data_reg);
float mmc5603nj_measure_req_plane(mmc5603nj_t *mag, uint8_t plane);
mmc5603nj_t *mmc5603nj_measure_req_alloc(int devaddr);
void mmc5603nj_measure_req_free(mmc5603nj_t *req);
FRESULT mmc5603nj_cal_save(mmc5603nj_t *mag, char *filename);
FRESULT mmc5603nj_cal_load(mmc5603nj_t *mag, char *filename);
