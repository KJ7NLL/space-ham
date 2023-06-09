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

#define ROTOR_CAL_MAGIC 0x458FD1E9

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

int motor_valid(struct motor *m)
{
	return m != NULL && m->pwm_Hz > 0 &&
		(m->port == gpioPortA || 
			m->port == gpioPortB || 
			m->port == gpioPortC || 
			m->port == gpioPortD);
}

int motor_online(struct motor *m)
{
	return m != NULL && m->online && motor_valid(m);
}

int rotor_valid(struct rotor *r)
{
	return r != NULL && motor_valid(&r->motor) && r->cal1.ready && r->cal2.ready;
}

int rotor_online(struct rotor *r)
{
	return r != NULL && rotor_valid(r) && motor_online(&r->motor) && r->target_enabled;
}

// Return the degree position of the motor baised on the voltage and calibrated values
float rotor_pos(struct rotor *r)
{
	float v, v_range, v_frac;

	v = iadc_get_result(r->iadc);

	v_range = r->cal2.v - r->cal1.v;
	if (v_range != 0)
		v_frac = (v - r->cal1.v) / v_range;
	else
		v_frac = 0;

	return r->cal1.deg + v_frac * (r->cal2.deg - r->cal1.deg);
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
	printf("%s:\r\n"
		"  cal1.v:               %f       mV\r\n"
		"  cal1.deg:             %f       deg\r\n"
		"  cal1.ready:           %d\r\n"
		"  cal2.v:               %f       mV\r\n"
		"  cal2.deg:             %f       deg\r\n"
		"  cal2.ready:           %d\r\n"
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
			r->cal1.v * 1000,
			r->cal1.deg,
			r->cal1.ready,
			r->cal2.v * 1000,
			r->cal2.deg,
			r->cal2.ready,
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
