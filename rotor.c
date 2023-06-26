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

#include "em_gpio.h"

#include "ff.h"
#include "fatfs-efr32.h"

#include "pwm.h"
#include "linklist.h"
#include "serial.h"
#include "rotor.h"
#include "strutil.h"
#include "iadc.h"

#define ROTOR_CAL_MAGIC   0x458FD1E9
#define ROTOR_CUR_VERSION 1

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

void initRotors()
{
	int i;

	memset(rotors, 0, sizeof(rotors));
	for (i = 0; i < NUM_ROTORS; i++)
	{
		rotors[i].iadc = i;

		sprintf(rotors[i].motor.name, "rotor%d", i);
		rotors[i].motor.timer = TIMERS[i];
		rotors[i].motor.duty_cycle_min = 0;
		rotors[i].motor.duty_cycle_max = 1;
		rotors[i].motor.pwm_Hz = 1047;      // C5 note
		rotors[i].motor.port = -1;

		PIDController_Init(&rotors[i].pid);
		rotors[i].pid.T = 0.01;
		rotors[i].pid.int_min = -1;
		rotors[i].pid.int_max = +1;
		rotors[i].pid.out_min = -1;
		rotors[i].pid.out_max = +1;

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

// Return the degree position of the motor based on the voltage and calibrated values
float rotor_pos(struct rotor *r)
{
	struct rotor_cal *cal_min = NULL, *cal_max = NULL;

	float v, v_range, v_frac;

	int i;
	int ascending;

	if (!rotor_valid(r))
		return NAN;

	if (rotor_cal_min(r)->v < rotor_cal_max(r)->v)
		ascending = 1;
	else
		ascending = 0;

	v = iadc_get_result(r->iadc);

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

	return cal_min->deg + v_frac * (cal_max->deg - cal_min->deg);
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
}

void motor_speed(struct motor *m, float speed)
{
	float duty_cycle, aspeed;
	int pin;

	if (speed > 1)
		speed = 1;
	else if (speed < -1)
		speed = -1;


	aspeed = fabs(speed); // really fast situps!
	if (aspeed < 0.003)
	{
		duty_cycle = 0;
	}
	else
	{
		duty_cycle = (m->duty_cycle_limit - m->duty_cycle_min) * aspeed + m->duty_cycle_min;
	}

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
}

void motor_detail(struct motor *m)
{
	char port = '?';
	switch (m->port)
	{
		case gpioPortA: port = 'A'; break;
		case gpioPortB: port = 'B'; break;
		case gpioPortC: port = 'C'; break;
		case gpioPortD: port = 'D'; break;
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
		"  speed:               %.1f%%\r\n",
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
			m->speed * 100
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
		"  cal_min.v:            %f       mV\r\n"
		"  cal_min.deg:          %f       deg\r\n"
		"  cal_min.ready:        %d\r\n"
		"  cal_max.v:            %f       mV\r\n"
		"  cal_max.deg:          %f       deg\r\n"
		"  cal_max.ready:        %d\r\n"
		"  iadc:                 %d\r\n"
		"  position:             %f       deg\r\n"
		"  target:               %f       deg\r\n"
		"  target_enabled:       %d\r\n"
		"  ramp_time:            %f\r\n"
		"  pid.Kp:               %f\r\n"
		"  pid.Ki:               %f\r\n"
		"  pid.Kd:               %f\r\n"
		"  pid.tau:              %f       sec\r\n"
		"  pid.out_min:          %f\r\n"
		"  pid.out_max:          %f\r\n"
		"  pid.int_min:          %f\r\n"
		"  pid.int_max:          %f\r\n"
		"  pid.T:                %f       sec\r\n"
		"  pid.prevError:        %f\r\n"
		"  pid.prevMeasurement:  %f\r\n"
		"  pid.proportional:     %f\r\n"
		"  pid.integrator:       %f\r\n"
		"  pid.differentiator:   %f\r\n"
		"  pid.out:              %f\r\n",
			r->motor.name,
			cal_min->v * 1000,
			cal_min->deg,
			cal_min->ready,
			cal_max->v * 1000,
			cal_max->deg,
			cal_max->ready,
			r->iadc,
			rotor_pos(r),
			r->target,
			r->target_enabled,
			r->ramp_time,
			r->pid.Kp,
			r->pid.Ki,
			r->pid.Kd,
			r->pid.tau,
			r->pid.out_min,
			r->pid.out_max,
			r->pid.int_min,
			r->pid.int_max,
			r->pid.T,
			r->pid.prevError,
			r->pid.prevMeasurement,
			r->pid.proportional,
			r->pid.integrator,
			r->pid.differentiator,
			r->pid.out);
}

void rotor_cal_load()
{
	struct rotor_cal_header h;

	uint32_t i;

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
			filename, res, ff_strerror(res), br, sizeof(h));
	}

	// Load cal.bin version 1 files
	if (h.magic == ROTOR_CAL_MAGIC && h.version == 1)
	{
		for (i = 0; i < h.n; i++)
		{
			uint32_t len = h.size;
			if (sizeof(struct rotor) < len)
				len = sizeof(struct rotor);

			f_lseek(&in, sizeof(h) + i * h.size);
			res = f_read(&in, &rotors[i], len, &br);
			if (res != FR_OK || br != len)
			{
				printf("%s[%ld]: V1: read error %d: %s (bytes read=%d/%ld)\r\n",
					filename, i, res, ff_strerror(res), br, len);
			}
		}
	}
	else
	{
		// Load version 0 files if there is no header
		for (i = 0; i < NUM_ROTORS; i++)
		{
			uint32_t len = 256;
			memset(&rotors[i], 0, sizeof(struct rotor));
			if (sizeof(struct rotor) < len)
				len = sizeof(struct rotor);

			f_lseek(&in, i * 256);

			res = f_read(&in, &rotors[i], len, &br);
			if (res != FR_OK || br != len)
			{
				printf("%s[%ld]: V0: read error %d: %s (bytes read=%d/%ld)\r\n",
					filename, i, res, ff_strerror(res), br, len);
			}
		}
	}

	for (i = 0; i < NUM_ROTORS; i++)
	{
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
		printf("%s: write error %d: %s (bytes written=%d/%d)\r\n",
			filename, res, ff_strerror(res), bw, len);
	}

	len = sizeof(rotors);

	for (i = 0; i < NUM_ROTORS; i++)
		rotors[i].version = ROTOR_CUR_VERSION;

	res = f_write(&out, rotors, len, &bw);
	if (res != FR_OK || bw != len)
	{
		printf("%s: write error %d: %s (bytes written=%d/%d)\r\n",
			filename, res, ff_strerror(res), bw, len);
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

	r->cal[r->cal_count].deg = deg;
	r->cal[r->cal_count].v = iadc_get_result(r->iadc);
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
