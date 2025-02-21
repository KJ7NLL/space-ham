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
//
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "platform.h"

#include "ff.h"
#include "fatfs-util.h"

#include "pwm.h"
#include "linklist.h"
#include "serial.h"
#include "rotor.h"
#include "strutil.h"
#include "iadc.h"
#include "i2c.h"
#include "i2c/ads111x.h"
#include "i2c/drv8830.h"
#include "i2c/mxc4005xc.h"
#include "i2c/mmc5603nj.h"
#include "rtcc.h"

#define ROTOR_CAL_MAGIC   0x458FD1E9
#define ROTOR_CUR_VERSION 3

// Rotor calibration file header (cal.bin)
struct rotor_cal_header
{
	uint32_t magic;

	uint32_t version;
	uint32_t size;
	uint32_t n;

	char pad[96];
};

struct rotor rotors[NUM_ROTORS];

// These motors will reference the motor in each rotor.
struct motor *motors[NUM_ROTORS];

static inline int kwrap(int k);
static inline float CP(struct rotor *r, int k);
static inline float AP(struct rotor *r, int k);
static inline float CV(struct rotor *r, int k);
static inline float CA(struct rotor *r, int k);
static inline float AV(struct rotor *r, int k);
static inline float err(struct rotor *r, int k);
static inline float err_sum(struct rotor *r);
static inline float SMC_S(struct rotor *r, int k);

void initRotors()
{
	int i;

	memset(rotors, 0, sizeof(rotors));
	for (i = 0; i < NUM_ROTORS; i++)
	{
		rotors[i].adc_channel = i;

		sprintf(rotors[i].motor.name, "rotor%d", i);
		rotors[i].motor.timer = TIMERS[i];
		rotors[i].motor.duty_cycle_min = 0;
		rotors[i].motor.duty_cycle_max = 1;
		rotors[i].motor.pwm_Hz = 1047;      // C5 note
		rotors[i].motor.port = -1;

		rotor_pid_reset(&rotors[i]);

		rotors[i].speed_exp = 1;
		rotors[i].version = ROTOR_CUR_VERSION;

		motors[i] = &rotors[i].motor;
	}
}

struct rotor *rotor_get(char *name)
{
	int i;

	// If name is a single char and it is a number, then return that rotor number
	if (strlen(name) == 1 && isdigit((int)name[0]) && name[0] >= '0' && name[0] < '0' + NUM_ROTORS)
	{
		return &rotors[name[0] - '0'];
	}

	for (i = 0; i < NUM_ROTORS; i++)
	{
		if (rotors[i].motor.name[0] && match(name, rotors[i].motor.name))
		{
			return &rotors[i];
		}
	}

	return NULL;
}

struct motor *motor_get(char *name)
{
	struct rotor *r = rotor_get(name);

	if (r != NULL)
		return &r->motor;
	else
		return NULL;
}

float rotor_get_voltage(struct rotor *r)
{
	float v;

	if (r->adc_type == ADC_TYPE_INTERNAL)
	{
		v = iadc_get_result(r->adc_channel);
	}
	else if (r->adc_type == ADC_TYPE_I2C_ADS111X)
	{
		ads111x_t *ads = (ads111x_t *)i2c_req_get_cont(r->adc_addr);

		if (rtcc_get_sec() - ads->req.complete_time >= 2)
			v = NAN;
		else
		{
			v = ads111x_measure_req(ads);

			// If the voltage is below 50 mV, then something is wrong:
			if (v < 0.050)
				v = NAN;
		}
	}
	else if (r->adc_type == ADC_TYPE_I2C_MXC4005XC)
	{
		i2c_req_t *req = i2c_req_get_cont(r->adc_addr);

		if (req == NULL)
			return NAN;

		mxc4005xc_t *acc = (mxc4005xc_t*)req;

		acc->invert_x = r->adc_vref & 0x01;
		acc->invert_y = r->adc_vref & 0x02;
		acc->invert_z = r->adc_vref & 0x04;
		v = mxc4005xc_measure_req_plane(acc, r->adc_channel);
	}
	else
	{
		return NAN;
	}

	return v;
}

float rotor_pos_adc(struct rotor *r)
{
	struct rotor_cal *cal_min = NULL, *cal_max = NULL;

	float v, v_range, v_frac;

	int i;
	int ascending;

	// Voltage-based position calibration is required below this line
	if (!rotor_valid(r) || !rotor_cal_valid(r))
		return NAN;

	if (rotor_cal_min(r)->v < rotor_cal_max(r)->v)
		ascending = 1;
	else
		ascending = 0;

	v = rotor_get_voltage(r);

	if (isnan(v))
		return NAN;

	if (ascending && v < rotor_cal_min(r)->v)
	{
		cal_min = &r->cal[0];
		cal_max = &r->cal[1];
	}
	else if (ascending && v > rotor_cal_max(r)->v)
	{
		cal_min = &r->cal[r->cal_count - 2];
		cal_max = &r->cal[r->cal_count - 1];
	}
	else if (!ascending && v > rotor_cal_min(r)->v)
	{
		cal_min = &r->cal[0];
		cal_max = &r->cal[1];
	}
	else if (!ascending && v < rotor_cal_max(r)->v)
	{
		cal_min = &r->cal[r->cal_count - 2];
		cal_max = &r->cal[r->cal_count - 1];
	}

	for (i = 1; cal_min == NULL && cal_max == NULL && i < r->cal_count; i++)
	{
		if (ascending && v >= r->cal[i-1].v && v <= r->cal[i].v)
		{
			cal_min = &r->cal[i-1];
			cal_max = &r->cal[i];
		}
		else if (!ascending && v <= r->cal[i-1].v && v >= r->cal[i].v)
		{
			cal_min = &r->cal[i-1];
			cal_max = &r->cal[i];
		}

	}

	if (cal_min == NULL || cal_max == NULL)
		return NAN;

	v_range = cal_max->v - cal_min->v;
	if (v_range != 0)
		v_frac = (v - cal_min->v) / v_range;
	else
		v_frac = 0;

	float pos = cal_min->deg + v_frac * (cal_max->deg - cal_min->deg);
	return pos;
}

// Return the degree position of the motor based on the voltage and calibrated values
float rotor_pos(struct rotor *r)
{
	float pos;

	i2c_req_t *req = i2c_req_get_cont(r->adc_addr);

	if (req == NULL)
		return NAN;

	if (r->adc_type == ADC_TYPE_I2C_MXC4005XC)
	{
		// Accelerometer is not really a voltage, but allows us to calibrate:
		pos = rotor_pos_adc(r);
	}
	else if (r->adc_type == ADC_TYPE_I2C_MMC5603NJ)
	{
		mmc5603nj_t *mag = (mmc5603nj_t*)req;

		mag->invert_x = r->adc_vref & 0x01;
		mag->invert_y = r->adc_vref & 0x02;
		mag->invert_z = r->adc_vref & 0x04;
		pos = mmc5603nj_measure_req_plane(mag, r->adc_channel);
	}
	else if (r->adc_type == ADC_TYPE_I2C_ADS111X || r->adc_type == ADC_TYPE_INTERNAL)
	{
		pos = rotor_pos_adc(r);
	}

	return pos - (r->offset - r->mag_dec);
}

void motor_init(struct motor *m)
{
	if (!motor_valid(m))
		return;

	// Sanity check: full range if not configured or misconfigured.
	if (m->duty_cycle_min == m->duty_cycle_max || 
		m->duty_cycle_max < m->duty_cycle_min ||
		m->duty_cycle_max < 0 || m->duty_cycle_max > 1 ||
		m->duty_cycle_min < 0 || m->duty_cycle_min > 1 ||
		m->duty_cycle_limit <= 0 || m->duty_cycle_limit > 1
		)
	{
		m->duty_cycle_min = 0;
		m->duty_cycle_max = 1;
		m->duty_cycle_limit = 1;
		m->speed = 0;
	}

	timer_init_pwm(m->timer, 0, m->port, m->pin1, m->pwm_Hz, m->duty_cycle_at_init);

	motor_speed(m, 0);
}

void motor_speed(struct motor *m, float speed)
{
	float duty_cycle, aspeed;
	float dir;

	if (m->invert)
		speed = -speed;

	if (speed > 1)
		speed = 1;
	else if (speed < -1)
		speed = -1;

	if (speed >= 0)
		dir = 1;
	else
		dir = -1;

	aspeed = fabs(speed); // really fast situps!

	// turn off if less than 0.003% speed:
	if (aspeed < 0.00003)
		duty_cycle = 0;
	else
		duty_cycle = (m->duty_cycle_limit - m->duty_cycle_min) * aspeed + m->duty_cycle_min;

	// If speed is 0 and the motor is valid, then always fall through so the motor stops.
	// If is is nonzero and online, then return early if the being set is the same.
	if (!motor_valid(m))
		return;
	else if (speed != 0 && !motor_online(m))
		return;
	else if (speed != 0 && m->speed == speed && duty_cycle < m->duty_cycle_limit)
		return;


	// Clamp duty_cycle if necessary
	if (duty_cycle < m->duty_cycle_min)
	{
		duty_cycle = 0;

	}
	else if (duty_cycle > m->duty_cycle_max)
	{
		duty_cycle = 1;
	}

	if (m->duty_cycle_limit < duty_cycle)
		duty_cycle = m->duty_cycle_limit;

	m->speed = speed; // For future reference

	if (m->motor_type == MOTOR_TYPE_DRV8830)
		drv8830_set_speed(m->motor_addr, duty_cycle * dir);

#ifdef __EFR32__
	int pin;

	// Choose the pin based on the direction
	if (speed >= 0)
	{
		pin = m->pin1;
		GPIO_PinOutClear(m->port, m->pin2);
	}
	else
	{
		pin = m->pin2;
		GPIO_PinOutClear(m->port, m->pin1);
	}

	// Control the pins directly if stopped or full speed, otherwise PWM
	if (duty_cycle == 0)
	{
		// Disable the route 
		timer_cc_route_clear(m->timer, 0);
		timer_disable(m->timer);

		GPIO_PinOutClear(m->port, m->pin1);
		GPIO_PinOutClear(m->port, m->pin2);
	}
	else if (duty_cycle == 1)
	{
		timer_cc_route_clear(m->timer, 0);
		timer_disable(m->timer);

		// If running at full speed, then clear the negative line and set the positive
		GPIO_PinOutSet(m->port, pin == m->pin1 ? m->pin1 : m->pin2);
		GPIO_PinOutClear(m->port, pin == m->pin1 ? m->pin2 : m->pin1);
	}
	else
	{
		timer_cc_route(m->timer, 0, m->port, pin);
		timer_cc_duty_cycle(m->timer, 0, duty_cycle);
		timer_enable(m->timer);
	}
#endif
}

void motor_detail(struct motor *m)
{
	char port = '?';
	switch (m->port)
	{
#ifdef __EFR32__
		case gpioPortA: port = 'A'; break;
		case gpioPortB: port = 'B'; break;
		case gpioPortC: port = 'C'; break;
		case gpioPortD: port = 'D'; break;
#endif
	}

	printf("%s:\r\n"
		"  port:                %c\r\n"
		"  pin1:                %d\r\n"
		"  pin2:                %d\r\n"
		"  pwm_Hz:              %d\r\n"
		"  online:              %d\r\n"
		"  duty_cycle_at_init:  %.1f%%\r\n"
		"  duty_cycle_min:      %.1f%%\r\n"
		"  duty_cycle_max:      %.1f%%\r\n"
		"  duty_cycle_limit:    %.1f%%\r\n"
		"  speed:               %.1f%%\r\n"
		"  type:                %d\r\n"
		"  bus:                 %d\r\n"
		"  addr:                %02x\r\n"
		"  channel:             %d\r\n",
			m->name,
			port,
			m->pin1,
			m->pin2,
			m->pwm_Hz,
			m->online,
			m->duty_cycle_at_init * 100,
			m->duty_cycle_min * 100,
			m->duty_cycle_max * 100,
			m->duty_cycle_limit * 100,
			m->speed * 100,
			m->motor_type,
			m->motor_bus,
			m->motor_addr,
			m->motor_channel
			);
}

void rotor_detail(struct rotor *r)
{
	struct rotor_cal *cal_min, *cal_max, dummy = {0,0,0};

	cal_min = rotor_cal_min(r);
	cal_max = rotor_cal_max(r);

	if (cal_min == NULL)
		cal_min = &dummy;

	if (cal_max == NULL)
		cal_max = &dummy;

	printf("%s:\r\n"
		"  error_count:           %d\r\n"
		"  error_count_max        %d\r\n"
		"  cal_min.v:             %13.9f       mV\r\n"
		"  cal_min.deg:           %13.9f       deg\r\n"
		"  cal_min.ready:         %3d\r\n"
		"  cal_max.v:             %13.9f       mV\r\n"
		"  cal_max.deg:           %13.9f       deg\r\n"
		"  cal_max.ready:         %3d\r\n"
		"  adc_addr:            0x%02x\r\n"
		"  adc_channel:           %3u\r\n"
		"  adc_type:              %3u\r\n"
		"  adc_bus:               %3u\r\n"
		"  adc_vref:              %3u\r\n"
		"  position:              %13.9f       deg\r\n"
		"  offset:                %13.9f       deg\r\n"
		"  mag_dec:               %13.9f       deg\r\n"
		"  target:                %13.9f       deg\r\n"
		"  target_enabled:        %3d\r\n"
		"  ramp_time:             %13.9f\r\n"
		"  speed_exp:             %13.9f\r\n"
		"  pid.target_cur:        %13.9f\r\n"
		"  pid.target_prev:       %13.9f\r\n"
		"  pid.target_cur_count:  %3d\r\n"
		"  pid.target_prev_count: %3d\r\n"
		"  pid.target_slope       %13.9f\r\n"
		"  pid.stationary         %13.9f\r\n"
		"  pid.one_dir_motion     %13.9f\r\n"
		"  pid.Kp:                %13.9f\r\n"
		"  pid.Ki:                %13.9f\r\n"
		"  pid.Kvfb:              %13.9f\r\n"
		"  pid.Kvff:              %13.9f\r\n"
		"  pid.Kaff:              %13.9f\r\n"
		"  pid.K1:                %13.9f\r\n"
		"  pid.K2:                %13.9f\r\n"
		"  pid.K3:                %13.9f\r\n"
		"  pid.K4:                %13.9f\r\n"
		"  pid.proportional:      %13.9f\r\n"
		"  pid.integrator:        %13.9f\r\n"
		"  pid.damping:           %13.9f\r\n"
		"  pid.feed-forward:      %13.9f\r\n"
		"  pid.SMC:               %13.9f\r\n"
		"  pid.out:               %13.9f\r\n",
			r->motor.name,
			r->error_count,
			r->error_count_max,
			cal_min->v * 1000,
			cal_min->deg,
			cal_min->ready,
			cal_max->v * 1000,
			cal_max->deg,
			cal_max->ready,
			r->adc_addr,
			r->adc_channel,
			r->adc_type,
			r->adc_bus,
			r->adc_vref,
			rotor_pos(r),
			r->offset,
			r->mag_dec,
			r->target,
			r->target_enabled,
			r->ramp_time,
			r->speed_exp,
			r->pid.target_cur,
			r->pid.target_prev,
			r->pid.target_cur_count,
			r->pid.target_prev_count,
			r->pid.target_slope,
			r->pid.stationary,
			r->pid.one_dir_motion,
			r->pid.kp,
			r->pid.ki,
			r->pid.kvfb,
			r->pid.kvff,
			r->pid.kaff,
			r->pid.k1,
			r->pid.k2,
			r->pid.k3,
			r->pid.k4,
			r->pid.P,
			r->pid.I,
			r->pid.D,
			r->pid.FF,
			r->pid.S,
			r->pid.out);


	int k = r->pid.k;

	printf("  rotor online:     %d\r\n", rotor_online(r));
	printf("  k:                %d\r\n", k);
	printf("  kwrap(k):         %d\r\n", kwrap(k));
	printf("  AP(k):            %+16.9f\r\n",  AP(r, k));
	printf("  AV(k):            %+16.9f\r\n",  AV(r, k));
	printf("  CP(k):            %+16.9f\r\n",  CP(r, k));
	printf("  CV(k):            %+16.9f\r\n",  CV(r, k));
	printf("  CA(k):            %+16.9f\r\n",  CA(r, k));
	printf("  err(k):           %+16.9f\r\n",  err(r, k));
	printf("  err_sum(k):       %+16.9f\r\n",  err_sum(r));
	printf("  SMC_S(k):         %+16.9f\r\n",  SMC_S(r, k));

	// Debug code for troubleshooting PID controller behavior
	/*
	int k, i;
	float cp, cp_prev;
	float cv, cv_prev;
	float ca, prev_target = 0;
	for (i = 0; i < PID_HIST_LEN; i++)
	{
		k = r->pid.k;

		cp = CP(r, k);

		cp_prev = CP(r, k-1);

		cv = CV(r, k);
		cv_prev = CV(r, k-1);

		ca = CA(r, k);

		rotor_pid_update(r, r->pid.target[k], r->pid.pos[k]);

		float target = r->pid.target_prev +
			(r->pid.target_cur_count + r->pid.target_prev_count) * r->pid.target_slope;
				;
		printf("%3d. pos: %13.9f target: %13.9f / %13.9f cur: %+f prev: %+f diff: %+f slope: %+f C: %d P: %d cv: %+f cv_prev: %+f ca: %+f\r\n",
			i, r->pid.pos[k], r->pid.target[k], target,
			r->pid.target_cur,
			r->pid.target_prev,
			prev_target - target,
			r->pid.target_slope,
			r->pid.target_cur_count,
			r->pid.target_prev_count,
			cv,
			cv_prev,
			ca
			);

		prev_target = target;
	}
	*/
}

void rotor_adc_init(struct rotor *r)
{
	if (r->adc_type == ADC_TYPE_I2C_ADS111X)
	{
		ads111x_t *adc_req;

		adc_req = ads111x_measure_req_alloc(r->adc_addr);

		adc_req->os = ADS111X_OS_START_SINGLE;
		adc_req->mux = r->adc_channel;
		adc_req->pga = r->adc_vref;
		adc_req->mode = ADS111X_MODE_CONT;

		ads111x_config_write(adc_req);

		if (i2c_req_get_cont(r->adc_addr) == NULL)
			i2c_req_add_cont(&adc_req->req);
	}
}

void rotor_cal_load()
{
	struct rotor_cal_header h;

	unsigned int i;

	char *filename = "cal.bin";

	FRESULT res = FR_OK;  /* API result code */
	FIL in;              /* File object */
	UINT br;          /* Bytes written */

	res = f_open(&in, filename, FA_READ);
	if (res != FR_OK)
	{
		printf("%s: open error %d: %s\r\n", filename, res, ff_strerror(res));
		return;
	}

	res = f_read(&in, &h, sizeof(h), &br);
	if (res != FR_OK || br != sizeof(h))
	{
		printf("%s[header]: read error %d: %s (bytes read=%d/%d)\r\n",
			filename, res, ff_strerror(res), br, (int)sizeof(h));
	}

	// Load cal.bin version 1 files
	if (h.magic == ROTOR_CAL_MAGIC && h.version == 1)
	{
		for (i = 0; i < h.n; i++)
		{
			unsigned int len = h.size;
			if (sizeof(struct rotor) < len)
				len = sizeof(struct rotor);

			f_lseek(&in, sizeof(h) + i * h.size);
			memset(&rotors[i], 0, sizeof(struct rotor));
			res = f_read(&in, &rotors[i], len, &br);
			if (res != FR_OK || br != len)
			{
				printf("%s[%d]: V1: read error %d: %s (bytes read=%d/%d)\r\n",
					filename, i, res, ff_strerror(res), br, len);
			}
		}
	}
	else
	{
		// Load version 0 files if there is no header
		for (i = 0; i < NUM_ROTORS; i++)
		{
			unsigned int len = 256;
			memset(&rotors[i], 0, sizeof(struct rotor));
			if (sizeof(struct rotor) < len)
				len = sizeof(struct rotor);

			f_lseek(&in, i * 256);

			res = f_read(&in, &rotors[i], len, &br);
			if (res != FR_OK || br != len)
			{
				printf("%s[%d]: V0: read error %d: %s (bytes read=%d/%d)\r\n",
					filename, i, res, ff_strerror(res), br, len);
			}
		}
	}

	for (i = 0; i < NUM_ROTORS; i++)
	{
		if (rotors[i].speed_exp < 1)
			rotors[i].speed_exp = 1;

		if (rotors[i].pid.k < 0 || rotors[i].pid.k >= PID_HIST_LEN)
			rotors[i].pid.k = 0;

		rotors[i].pid.target_cur = 0;
		rotors[i].pid.target_prev = 0;
		rotors[i].pid.target_cur_count = 0;
		rotors[i].pid.target_prev_count = 0;
		rotors[i].pid.target_slope = 0;

		rotors[i].error_count = 0;

		// Upgrade rotor structures if they are an old version
		if (rotors[i].version < 1)
		{
			rotors[i].cal[0] = rotors[i].old_cal1;
			rotors[i].cal[1] = rotors[i].old_cal2;

			if (rotors[i].cal[0].ready && rotors[i].cal[1].ready)
				rotors[i].cal_count = 2;
			else if (rotors[i].cal[0].ready)
				rotors[i].cal_count = 1;
			else
				rotors[i].cal_count = 0;

			rotors[i].version = 1;
		}

		if (rotors[i].version < 2)
		{
			rotors[i].adc_type = ADC_TYPE_INTERNAL;
			rotors[i].adc_bus = 0;
			rotors[i].adc_addr = 0;
			rotors[i].adc_channel = i;
			rotors[i].adc_vref = 0;

			rotors[i].version = 2;

		}

		if (rotors[i].version < 3)
		{
			rotors[i].error_count = 0;
			rotors[i].error_count_max = 300; // 3s at 100ticks/sec
			rotors[i].version = 3;
		}


		rotor_adc_init(&rotors[i]);
	}

	f_close(&in);
}

void rotor_cal_save()
{
	struct rotor_cal_header h;

	FRESULT res = FR_OK;  /* API result code */
	FIL out;              /* File object */
	UINT bw;          /* Bytes written */

	char *filename = "cal.bin";

	size_t len;

	int i;

	res = f_open(&out, filename, FA_CREATE_ALWAYS | FA_WRITE);
	if (res != FR_OK)
	{
		printf("%s: open error %d: %s\r\n", filename, res, ff_strerror(res));
		return;
	}

	memset(&h, 0, sizeof(h));

	// Write calibration file version 1 format:
	//   The structure defines the number of records
	//   and the size of each record.
	h.magic = ROTOR_CAL_MAGIC;
	h.version = 1;
	h.size = sizeof(struct rotor);
	h.n = NUM_ROTORS;

	len = sizeof(h);
	res = f_write(&out, &h, len, &bw);
	if (res != FR_OK || bw != len)
	{
		printf("%s: write error %d: %s (bytes written=%d/%ld)\r\n",
			filename, res, ff_strerror(res), bw, (long)len);
	}

	len = sizeof(rotors);

	for (i = 0; i < NUM_ROTORS; i++)
		rotors[i].version = ROTOR_CUR_VERSION;

	res = f_write(&out, rotors, len, &bw);
	if (res != FR_OK || bw != len)
	{
		printf("%s: write error %d: %s (bytes written=%d/%ld)\r\n",
			filename, res, ff_strerror(res), bw, (long)len);
	}

	f_close(&out);
}

// Set the target of the rotor to the current position and set motor speed to 0.
// This is a temporary change that will be overriden if systick is enabled.
// This function is used by `sat reset` to stop all motion and by xmodem
// to prevent rotor drift during transfer.
void rotor_suspend_all()
{
	int i;

	for (i = 0; i < NUM_ROTORS; i++)
	{
		rotors[i].target = rotor_pos(&rotors[i]);
		motor_speed(motors[i], 0);
	}
}

// Squish all ready calibrations together starting at index 0
void rotor_cal_squish(struct rotor *r)
{
	int i;
	int found;

	do
	{
		found = 0;
		for (i = 1; i < ROTOR_CAL_NUM; i++)
		{
			if (!r->cal[i-1].ready && r->cal[i].ready)
			{
				r->cal[i-1] = r->cal[i];
				memset(&r->cal[i], 0, sizeof(struct rotor_cal));

				found++;
			}
		}
	} while (found);
}

// Sort calibration structs by degree
void rotor_cal_sort(struct rotor *r)
{
	int i;
	int found;

	struct rotor_cal swap;

	rotor_cal_squish(r);

	do
	{
		found = 0;
		for (i = 1; i < ROTOR_CAL_NUM; i++)
		{
			if (!r->cal[i].ready)
				break;

			if (r->cal[i-1].deg > r->cal[i].deg)
			{
				swap = r->cal[i];
				r->cal[i] = r->cal[i-1];
				r->cal[i-1] = swap;

				found++;
			}
		}
	} while (found);
}

// Add a new calibration at the current IADC position for this rotor
void rotor_cal_add(struct rotor *r, float deg)
{
	if (r->cal_count >= ROTOR_CAL_NUM)
	{
		printf("No room for additional calibrations\r\n");

		return;
	}

	r->cal[r->cal_count].ready = 0;
	r->cal[r->cal_count].deg = deg;

	r->cal[r->cal_count].v = rotor_get_voltage(r);

	r->cal[r->cal_count].ready = 1;

	r->cal_count++;

	rotor_cal_sort(r);
}

// Remove calibration index `idx`
void rotor_cal_remove(struct rotor *r, int idx)
{
	if (idx < r->cal_count && idx >= 0)
	{
		r->cal[idx].ready = 0;
		r->cal_count--;
	}
	else
		printf("Calibration index is out of range. Choose a value between 0 and %d\r\n", r->cal_count);

	rotor_cal_squish(r);
}

// Re-calculate calibration voltages to adjust by trim_deg. Calibrations keep
// the same degree angle but voltages are adjusted to slide all calibrations by
// the same amount. Returns 1 on success or 0 on failure.
int rotor_cal_trim(struct rotor *r, float trim_deg)
{
	struct rotor_cal *cal_min = NULL, *cal_max = NULL;

	float vdeg;
	float trim_v;

	int i;

	if (!rotor_valid(r))
		return 0;

	cal_min = rotor_cal_min(r);
	cal_max = rotor_cal_max(r);

	if (cal_min == NULL || cal_max == NULL)
		return 0;

	// Avoid divide by zero
	if (cal_max->deg == cal_min->deg)
		return 0;

	// volts per degree
	vdeg = (cal_max->v - cal_min->v) / (cal_max->deg - cal_min->deg);
	trim_v = vdeg * trim_deg;

	for (i = 0; i < r->cal_count; i++)
		r->cal[i].v = r->cal[i].v + trim_v;

	return 1;
}

void rotor_pid_reset(struct rotor *r)
{
	int en = r->target_enabled;
	r->target_enabled = 0;

	r->pid.P = 0;
	r->pid.I = 0;
	r->pid.D = 0;
	r->pid.FF = 0;
	r->pid.S = 0;
	r->pid.out = 0;

	r->pid.target_cur = 0;
	r->pid.target_prev = 0;
	r->pid.target_cur_count = 0;
	r->pid.target_prev_count = 0;
	r->pid.target_slope = 0;

	r->pid.k = 0;

	memset(r->pid.pos, 0, sizeof(r->pid.pos));
	memset(r->pid.target, 0, sizeof(r->pid.target));

	r->target_enabled = en;
}

// Investigation of control algorithm for long-stroke fast tool servo system
// https://doi.org/10.1016/j.precisioneng.2022.01.006

static inline int kwrap(int k)
{
	k = k % PID_HIST_LEN;
	while (k < 0)
		k += PID_HIST_LEN;

	return k;
}

static inline float CP(struct rotor *r, int k)
{
	return r->pid.target[kwrap(k)];
}

static inline float AP(struct rotor *r, int k)
{
	return r->pid.pos[kwrap(k)];
}

static inline float CV(struct rotor *r, int k)
{
	return CP(r, k) - CP(r, k-1);
}

static inline float CA(struct rotor *r, int k)
{
	return CV(r, k) - CV(r, k-1);
}

static inline float AV(struct rotor *r, int k)
{
	return AP(r, k) - AP(r, k-1);
}

static inline float err(struct rotor *r, int k)
{
	return CP(r, k) - AP(r, k-1);
}

static inline float err_sum(struct rotor *r)
{
	int k;

	float ret = 0;

	for (k = 0; k < PID_HIST_LEN; k++)
		ret += err(r, k);

	return ret;
}

static inline float SMC_S(struct rotor *r, int k)
{
	return r->pid.k1 * err(r, k) + r->pid.k2 * (err(r, k) - err(r, k-1));
}

float rotor_pid_update(struct rotor *r, float target, float pos)
{
	int i;
	int k = r->pid.k;

	float S;
	float S_sign;
	float S_sum = 0;

	r->pid.pos[k] = pos;

	// position calculations like those for tracking satellites and
	// celestial bodies may not keep up with the PID updat tick rate of
	// ~100 ticks/sec, so extrapolate the target based on the target
	// position the last time it changed:
	if (r->pid.target_cur_count >= PID_HIST_LEN*2 || fabs(target - r->pid.target_cur) > 10)
	{
		// Safety: if target_cur_count is wrapping or the target
		// change is large, then do not do a slope calculation:
		r->pid.target_cur = target;
		r->pid.target_prev = target;
		r->pid.target_cur_count = 0;
		r->pid.target_prev_count = 0;
		r->pid.target_slope = 0;
	}
	else if (fabs(target - r->pid.target_cur) > 1e-9)
	{
		// If we get here, then the target changed by a small amount,
		// so calculate the slope for extraploation:
		r->pid.target_prev = r->pid.target_cur;
		r->pid.target_cur = target;

		r->pid.target_prev_count = r->pid.target_cur_count + 1;
		r->pid.target_cur_count = 0;

		r->pid.target_slope =
				(r->pid.target_cur - r->pid.target_prev)
					/   // over
				(r->pid.target_prev_count);
	}
	else
		r->pid.target_cur_count++;


	// Extrapolate the adjusted target value based on slope. 
	// If slope is 0, then there will be no extrapolation:
	r->pid.target[k] = r->pid.target_prev +
		(r->pid.target_cur_count + r->pid.target_prev_count) * r->pid.target_slope;

	float e = err(r, k);

	r->pid.P = r->pid.kp * e;

	r->pid.I = r->pid.ki * err_sum(r) / (float)PID_HIST_LEN;

	// FF scales based on target (control) velocity and acceleration.
	// This is good for tracking:
	r->pid.FF = (r->pid.kvff * CV(r, k) + r->pid.kaff * CA(r, k));

	r->pid.D = -(r->pid.kvfb * AV(r, k)); // negative is intentional

	// SMC
	for (i = 0; i < PID_HIST_LEN; i++)
	{
		S = SMC_S(r, i);

		if (S < 0)
			S_sign = -1;
		else
			S_sign = 1;

		S_sum += r->pid.k4 * S_sign * fabs(err(r, i));
	}

	r->pid.S = r->pid.k3 * SMC_S(r, k) + S_sum;

	r->pid.out = r->pid.P + r->pid.I + r->pid.D + r->pid.FF + r->pid.S;

	if (fabs(e) < r->pid.stationary)
		r->pid.out = 0;
	else if (fabs(e) < r->pid.one_dir_motion)
	{
		if (r->pid.target_slope < 0 && r->pid.out > 0)
			r->pid.out = 0;
		else if (r->pid.target_slope > 0 && r->pid.out < 0)
			r->pid.out = 0;
	}

	// increment the history index
	r->pid.k = kwrap(k + 1);

	return r->pid.out;
}
