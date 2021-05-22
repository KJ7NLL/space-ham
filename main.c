#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "em_device.h"
#include "em_chip.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"

#include "linklist.h"
#include "serial.h"
#include "strutil.h"
#include "iadc.h"
#include "pwm.h"

// Size of the buf for received data
#define BUFLEN  80

#define LED_PORT gpioPortB
#define LED0_PIN 0
#define LED1_PIN 1

#define MAX_ARGS 10

struct motor
{
	TIMER_TypeDef *timer;
	int port;
	int pin1;
	int pin2;

	char *name;

	// -1 to 1
	float speed;

	// speeds below min_duty_cycle stop the motor
	// speeds above max_duty_cycle run the motor at full speed
	float initial_duty_cycle, min_duty_cycle, max_duty_cycle;
};

struct rotor
{
	struct motor motor;
	double target;

	struct cal
	{
		double v, deg;
		int ready;
	} cal1, cal2;

} phi, theta;

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

void initGpio(void)
{
	// Configure PA5 as an output (TX)
	GPIO_PinModeSet(gpioPortA, 5, gpioModePushPull, 0);

	// Configure PA6 as an input (RX)
	GPIO_PinModeSet(gpioPortA, 6, gpioModeInput, 0);

	// Configure PD4 as output and set high (VCOM_ENABLE)
	// comment out next line to disable VCOM
	GPIO_PinModeSet(gpioPortD, 4, gpioModePushPull, 1);
	
	// pwm ports
	GPIO_PinModeSet(gpioPortC, 0, gpioModePushPull, 0);
	GPIO_PinModeSet(gpioPortC, 1, gpioModePushPull, 0);
	GPIO_PinModeSet(gpioPortC, 2, gpioModePushPull, 0);
	GPIO_PinModeSet(gpioPortC, 3, gpioModePushPull, 0);

	// turn on LED0 
	GPIO_PinModeSet(LED_PORT, LED0_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(LED_PORT, LED1_PIN, gpioModePushPull, 1);
}

void initCmu(void)
{
	// Enable clock to GPIO and USART1
	CMU_ClockEnable(cmuClock_GPIO, true);
	CMU_ClockEnable(cmuClock_USART1, true);
	CMU_ClockEnable(cmuClock_TIMER0, true);
}

void help()
{
	char *h =
		"mv [--speed 0-100] (phi|theta) <([+-]deg|n|e|s|w)>    # moves antenna\r\n"
		"motor (theta|phi) <[+-]0-100>                         # turn on motor at speed\r\n"
		"cam (on|off)                                          # turns camera on or off\r\n"
		"(help|?|h)                                            # list of commands\r\n"
		"status                                                # display system status\r\n"
		"cal (theta|phi) <deg>                                 # Calibrates phi or theta\r\n"
		"sat (load|track|list|search)                          # satellite commands\r\n";
	print(h);
}

void dump_history(struct linklist *history)
{
	while (history != NULL)
	{
		print(history->s);
		print("\r\n");
		history = history->next;
	}
}

void status()
{
	char s[128];
	int i;

	for (i = 0; i < IADC_NUM_INPUTS; i++)
	{
		sprintf(s, "iadc[%d]: %f\r\n", i, (float)iadc_get_result(i));
		print(s);
	}
}

void motor(int argc, char **args)
{
	struct motor *m = NULL;

	float speed;

	if (argc < 3)
	{
		print("Usage: motor <motor_name> <speed>\r\n"
			"speed can be between -100 and 100, 0 is stopped\r\n");
		return;
	}
	if (match(args[1], "theta"))
	{
		m = &theta.motor;
	}
	else if (match(args[1], "phi"))
	{
		m = &phi.motor;
	}
	else
	{
		print("Unkown Motor\r\n");
		return;
	}

	speed = atof(args[2]) / 100;
	if (speed < -1 || speed > 1)
	{
		print("Speed is out of bounds\r\n");

		return;
	}

	motor_speed(m, speed);
}

int main()
{
	struct linklist *history = NULL;
	char buf[128], *args[MAX_ARGS];
	char c;
	
	int i, argc;

	// Chip errata
	CHIP_Init();

	// Initialize GPIO and USART1
	initCmu();
	initGpio();
	initUsart1();
	initIADC();

	memset(&theta, 0, sizeof(theta));
	memset(&phi, 0, sizeof(phi));

	theta.motor.name = "theta";
	theta.motor.timer = TIMER0;
	theta.motor.port = gpioPortC;
	theta.motor.pin1 = 0;
	theta.motor.pin2 = 1;
	theta.motor.min_duty_cycle = 0.1;
	theta.motor.max_duty_cycle = 1.0;
	theta.motor.initial_duty_cycle = 0.0;

	phi.motor.name = "phi";
	phi.motor.timer = TIMER1;
	phi.motor.port = gpioPortC;
	phi.motor.pin1 = 2;
	phi.motor.pin2 = 3;
	phi.motor.min_duty_cycle = 0.25;
	phi.motor.max_duty_cycle = 1.0;
	phi.motor.initial_duty_cycle = 0.0;

	motor_init(&theta.motor);
	motor_init(&phi.motor);

	print("\x0c\r\n");
	help();
	status();
	
	/*
	void initIADC(void);
	double get_result(int i);
	*/

	for (;;)
	{
		print("[Zeke&Daddy@console]# ");
		input(buf, sizeof(buf)-1, &history);
		print("\r\n");
		
		argc = parse_args(buf, args, MAX_ARGS);
		
		if (match(args[0], "history") || match(args[0], "hist"))
		{
			dump_history(history);
		}

		else if (match(args[0], "h") || match(args[0], "?") || match(args[0], "help"))
		{
			help();
		}

		else if (match(args[0], "debug-keys"))
		{
			print("press CTRL+C to end\r\n");
			c = 0;
			while (c != 3)
			{
				serial_read(&c, 1);
				sprintf(buf, "you typed: %3d (0x%02x): '%c'\r\n", c, c, isprint(c) ? c : '?');
				print(buf);
			}
		}
		
		else if (match(args[0], "stop"))
		{
			motor_speed(&theta.motor, 0);
			motor_speed(&phi.motor, 0);
		}

		else if (match(args[0], "status") || match(args[0], "stat"))
		{
			status();
		}

		else if (match(args[0], "motor"))
		{
			motor(argc, args);
		}

		// This must be the last else if:
		else if (!match(args[0], ""))
		{
			print("Unkown command: ");
			print(args[0]);
			print("\r\n\n");
		}
	}
}
