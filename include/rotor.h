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
	union {
		struct {
			struct motor motor;
			struct rotor_cal cal1, cal2;
			
			int iadc;

			float target;

			char target_enabled;

			PIDController pid;

			float ramp_time;
		};

		char pad[256];
	};
};

extern struct rotor rotors[NUM_ROTORS];

// These motors will reference the motor in each rotor.
extern struct motor *motors[NUM_ROTORS];

void initRotors();

void motor_init(struct motor *m);
void motor_speed(struct motor *m, float speed);

int motor_valid(struct motor *m);
int motor_online(struct motor *m);
int rotor_valid(struct rotor *r);
int rotor_online(struct rotor *r);

float rotor_pos(struct rotor *r);

struct motor *motor_get(char *name);
struct rotor *rotor_get(char *name);

void rotor_detail(struct rotor *r);
void motor_detail(struct motor *m);

void rotor_cal_load();
void rotor_cal_save();

void rotor_suspend_all();
