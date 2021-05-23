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
	float initial_duty_cycle, min_duty_cycle, max_duty_cycle, duty_cycle_limit;
};

struct rotor_cal
{
	float v, deg;
	int ready;
};

struct rotor
{
	struct motor motor;
	struct rotor_cal cal1, cal2;
	
	int iadc;

	float target;


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
		"mv (phi|theta) <([+-]deg|n|e|s|w)> [<speed=0-100>]    # moves antenna\r\n"
		"motor (theta|phi) <[+-]0-100>                         # turn on motor at speed\r\n"
		"cam (on|off)                                          # turns camera on or off\r\n"
		"(help|?|h)                                            # list of commands\r\n"
		"status                                                # display system status\r\n"
		"cal (theta|phi|reset) <(deg|reset)>                   # Calibrates phi or theta\r\n"
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
	int i;

	for (i = 0; i < IADC_NUM_INPUTS; i++)
	{
		printf("iadc[%d]: %f volts\r\n", i, (float)iadc_get_result(i));
	}
	print("\r\n");
	
	printf("theta cal: [%.2f, %.2f] deg = [%.2f, %.2f] volts, %.4f V/deg\r\n", 
		theta.cal1.deg, theta.cal2.deg,
		theta.cal1.v, theta.cal2.v,
		(theta.cal2.v - theta.cal1.v) / (theta.cal2.deg - theta.cal1.deg)
		);
	printf("  phi cal: [%.2f, %.2f] deg = [%.2f, %.2f] volts, %.4f V/deg\r\n\n", 
		phi.cal1.deg, phi.cal2.deg,
		phi.cal1.v, phi.cal2.v,
		(phi.cal2.v - phi.cal1.v) / (phi.cal2.deg - phi.cal1.deg)
		);

	printf("theta pos: target = %.2f deg\r\n", theta.target);
	printf("  phi pos: target = %.2f deg\r\n", phi.target);
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

void cal(int argc, char **args)
{
	struct rotor *r;
	struct rotor_cal cal;

	float deg, v;

	if (argc < 2)
	{
		print("Usage: cal <rotor_name> <deg>\r\n"
			"Calibration assumes a linear voltage slope between degrees.\r\n"
			"You must make 2 calibrations, and each calibration is used as\r\n"
			"the maximum extent for the rotor. Use the `motor` command to\r\n"
			"move the rotor between calibrations.\r\n");
		return;
	}
	if (match(args[1], "theta"))
	{
		r = &theta;
	}
	else if (match(args[1], "phi"))
	{
		r = &phi;
	}
	else if (match(args[1], "reset"))
	{
		memset(&phi.cal1, 0, sizeof(phi.cal1));
		memset(&phi.cal2, 0, sizeof(phi.cal2));
		memset(&theta.cal1, 0, sizeof(theta.cal1));
		memset(&theta.cal2, 0, sizeof(theta.cal2));

		print("All calibrations have been reset\r\n");

		return;
	}
	else
	{
		printf("Unkown Rotor: %s\r\n", args[1]);
		return;
	}

	if (argc < 3)
	{
		print("Missing Argument: only reset can be called with 2 arguments\r\n");

		return;
	}

	if (match(args[2], "reset"))
	{
		memset(&r->cal1, 0, sizeof(r->cal1));
		memset(&r->cal2, 0, sizeof(r->cal2));

		printf("Calibration reset: %s\r\n", r->motor.name);

		return;
	}
	deg = atof(args[2]);
	v = iadc_get_result(r->iadc);
	
	cal.deg = deg;
	cal.v = v;
	cal.ready = 1;

	if (deg < 0)
	{
		print("Degree angles must start at 0");

		return;
	}

	if (!r->cal1.ready && !r->cal2.ready)
	{
		r->cal1 = cal;
	}
	else if (r->cal1.ready && !r->cal2.ready)
	{
		if (deg > r->cal1.deg)
		{
			r->cal2 = cal;
		}
		else
		{
			r->cal2 = r->cal1;
			r->cal1 = cal;
		}
	}
	else if (!r->cal1.ready && r->cal2.ready)
	{
		if (deg > r->cal2.deg)
		{
			r->cal1 = r->cal2;
			r->cal2 = cal;
		}
		else
		{
			r->cal1 = cal;
		}
	}
	else
	{
		if (deg < r->cal2.deg)
		{	
			r->cal1 = cal;
		}
		else
		{
			r->cal2 = cal;
		}
	}

	if (!r->cal1.ready || !r->cal2.ready)
	{
		print("~~~Not done yet! Please enter cal 2.~~~\r\n");
	}
	else if (r->cal1.ready && r->cal2.ready)
	{
		print("~~~Done calibrating!~~~\r\n");
	}
}

void mv(int argc, char **args)
{
	struct rotor *r;

	float deg, speed;

	if (argc < 3)
	{
		print("mv (phi|theta) <([+-]deg|n|e|s|w)> [<speed=0-100>]\r\n"
			"Move rotor to a degree angle. North is 0 deg.\r\n");
		return;
	}

	if (match(args[1], "theta"))
	{
		r = &theta;
	}
	else if (match(args[1], "phi"))
	{
		r = &phi;
	}
	else
	{
		printf("Unkown Rotor: %s\r\n", args[1]);
		return;
	}

	if (!r->cal1.ready || !r->cal2.ready)
	{
		printf("Rotor %s is not calibrated, please calibrate and try again.\r\n", r->motor.name);

		return;
	}

	deg = atof(args[2]);
	switch (args[2][0])
	{
		case '+':
			deg = r->target + deg;
			break;

		case '-':
			deg = r->target - deg;
			break;

		case 'w':
			deg = 270;
			break;

		case 'e':
			deg = 90;
			break;

		case 's':
			deg = 180;
			break;

		case 'n':
			deg = 0;
			break;
	}

	if (deg < r->cal1.deg || deg > r->cal2.deg)
	{
		printf("Cannot move rotor outside of calibrated range: %.2f !< %.2f !< %.2f\r\n",
			r->cal1.deg, deg, r->cal2.deg);

		return;
	}
	
	if (argc >= 4)
	{
		speed = atof(args[3]) / 100;
		if (speed < r->motor.min_duty_cycle)
		{
			printf("Rotor speed %.2f is too low the minimum duty cycle of %.2f for this rotor\r\n", 
				speed * 100, r->motor.min_duty_cycle);

			return;
		}
		else if (speed > r->motor.max_duty_cycle)
		{
			printf("Rotor speed %.2f is too low the maximum duty cycle of %.2f for this rotor\r\n", 
				speed * 100, r->motor.max_duty_cycle);

			return;
		}
		else
		{
			r->motor.duty_cycle_limit = speed;
		}
	}

	// Set the target degree angle
	r->target = deg;
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

	theta.iadc = 0;
	theta.motor.name = "theta";
	theta.motor.timer = TIMER0;
	theta.motor.port = gpioPortC;
	theta.motor.pin1 = 0;
	theta.motor.pin2 = 1;
	theta.motor.min_duty_cycle = 0.1;
	theta.motor.max_duty_cycle = 1.0;
	theta.motor.initial_duty_cycle = 0.0;

	phi.iadc = 1;
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
	print("\r\n");
	status();
	print("\r\n");
	
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

		else if (match(args[0], "cal"))
		{
			cal(argc, args);
		}

		else if (match(args[0], "mv"))
		{
			mv(argc, args);
		}
		
		// This must be the last else if:
		else if (!match(args[0], ""))
		{
			print("Unkown command: ");
			print(args[0]);
			print("\r\n");
		}


		if (!match(args[0], ""))
		{
			print("\r\n");
		}
	}
}
