#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "em_gpio.h"

#include "pwm.h"
#include "linklist.h"
#include "serial.h"
#include "rotor.h"
#include "strutil.h"

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

		motors[i] = &rotors[i].motor;
	}
}

struct rotor *rotor_get(char *name)
{
	int i;

	// If name is a single char and it is a number, then return that rotor number
	if (strlen(name) == 1 && isdigit(name[0]) && name[0] >= '0' && name[0] < '0' + NUM_ROTORS)
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
	return r != NULL && motor_valid(&r->motor);
}

int rotor_online(struct rotor *r)
{
	return r != NULL && rotor_valid(r) && motor_online(&r->motor);
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
	float duty_cycle = fabs(speed); // really fast situps!
	
	int pin;

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
