#include <math.h>

#include "em_gpio.h"

#include "pwm.h"
#include "serial.h"
#include "rotor.h"

void motor_init(struct motor *m)
{
	// Sanity check: full range if not configured or misconfigured.
	if (m->min_duty_cycle == m->max_duty_cycle || m->max_duty_cycle < m->min_duty_cycle)
	{
		m->min_duty_cycle = 0;
		m->max_duty_cycle = 1;
	}

	timer_init_pwm(m->timer, 0, m->port, m->pin1, m->initial_duty_cycle);
}

void motor_speed(struct motor *m, float speed)
{
	float duty_cycle;

	int pin;

	duty_cycle = fabs(speed); // really fast situps!

	// Clamp duty_cycle if necessary
	if (duty_cycle < m->min_duty_cycle)
	{
		duty_cycle = 0;

	}
	else if (duty_cycle > m->max_duty_cycle)
	{
		duty_cycle = 1;
	}

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
		print(m->name);
		print(": Motor Stopped\r\n");
	}
	else if (duty_cycle == 1)
	{
		timer_cc_route_clear(m->timer, 0);
		timer_disable(m->timer);

		print(m->name);
		print(": Motor Full Speed\r\n");

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
