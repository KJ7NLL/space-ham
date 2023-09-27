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
#include "pid.h"

// The efr32mg21 supports 4 timers. Timers 0 and 1 work on any port, 
// but timers 2 only work on ports a/b and timer 3 works on c/d
#define NUM_ROTORS 4

// WARNING: Changing these values changes the size of `struct rotors`. This may
// break backwards compatibility of cal.bin files.
#define ROTOR_CAL_NUM 90
#define PID_HIST_LEN 100

enum {
	ADC_TYPE_INTERNAL,
	ADC_TYPE_I2C,
};

struct motor
{
	union {
		struct {
			char name[20];

			TIMER_TypeDef *timer;
			int port;
			int pin1;
			int pin2;

			int pwm_Hz;

			// The port, pins, and PWM have been assigned. If a motor
			// is used before it is ready, it will crash the MCU.
			int online;

			// Set this duty cycle upon initialization
			float duty_cycle_at_init;

			// speeds below min_duty_cycle stop the motor
			float duty_cycle_min;

			// speeds above max_duty_cycle run the motor at full speed
			float duty_cycle_max;

			// Temporarily limit the duty cycle below the variable speed. 
			// If this above duty_cycle_max, then no limit.
			// Warning: setting this below duty_cycle_min will stop the motor
			//          next time motor_speed is called. 
			float duty_cycle_limit;

			// -1 to 1, informational only, does not set the speed:
			float speed;
		};

		char pad[80];
	};
};

struct rotor_cal
{
	float v, deg;
	int ready;
};

struct rotor
{
	// Version 0
	// Do not re-order
	struct motor motor;

	// The below calibrations are no longer used, but keep them so we can
	// load and upgrade old calibration files:
	struct rotor_cal old_cal1, old_cal2;

	// This is a packed anonymous structure because it takes the place of
	// where an int used to be.
	struct
	{
		uint8_t adc_type:4;
		uint8_t adc_bus:4;
		uint16_t adc_addr;
		uint8_t adc_channel;
	} __attribute__((packed));

	float target;

	char target_enabled;

	PIDController oldpid;

	float ramp_time;

	char target_absolute;

	// Version 1
	char version;

	float offset;

	struct rotor_cal cal[ROTOR_CAL_NUM];

	int cal_count;

	float speed_exp;

	struct {
		float kp, ki, kvfb, kvff, kaff, k1, k2, k3, k4;
		float P, I, D, FF, S, out;

		int k;
		float pos[PID_HIST_LEN];
		float target[PID_HIST_LEN];
		float target_prev, target_cur, target_slope;

		int target_prev_count, target_cur_count;

		// the rotor will not move if the err is less than `stationary`
		float stationary;

		// Will only allow the rotor to move in the same direction as
		// the satellite/celestial object while the err is less than
		// `one_dir_motion`.
		float one_dir_motion;

	} pid;
};

extern struct rotor rotors[NUM_ROTORS];

// These motors will reference the motor in each rotor.
extern struct motor *motors[NUM_ROTORS];

void initRotors();

void motor_init(struct motor *m);
void motor_speed(struct motor *m, float speed);

float rotor_pos(struct rotor *r);

struct motor *motor_get(char *name);
struct rotor *rotor_get(char *name);

void rotor_detail(struct rotor *r);
void motor_detail(struct motor *m);

void rotor_cal_load();
void rotor_cal_save();
void rotor_cal_add(struct rotor *r, float deg);
void rotor_cal_remove(struct rotor *r, int idx);
int rotor_cal_trim(struct rotor *r, float trim_deg);

void rotor_suspend_all();

void rotor_pid_reset(struct rotor *r);
float rotor_pid_update(struct rotor *r, float target, float pos);

static inline int motor_valid(struct motor *m)
{
	return m != NULL && m->pwm_Hz > 0 &&
		(m->port == gpioPortA ||
			m->port == gpioPortB ||
			m->port == gpioPortC ||
			m->port == gpioPortD);
}

static inline int motor_online(struct motor *m)
{
	return m != NULL && m->online && motor_valid(m);
}

static inline int rotor_valid(struct rotor *r)
{
	return r != NULL && motor_valid(&r->motor) && r->cal_count >= 2;
}

static inline int rotor_online(struct rotor *r)
{
	return r != NULL && rotor_valid(r) && motor_online(&r->motor) && r->target_enabled;
}

// rotor_cal_min/max return the calibration structure with the minimum or
// maximum degree angle. If there are no calibrations, then both functions
// return NULL.
static inline struct rotor_cal *rotor_cal_min(struct rotor *r)
{
	if (r->cal_count == 0)
		return NULL;

	return &r->cal[0];
}

static inline struct rotor_cal *rotor_cal_max(struct rotor *r)
{
	if (r->cal_count == 0)
		return NULL;

	return &r->cal[r->cal_count - 1];
}
