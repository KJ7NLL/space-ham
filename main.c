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

#define _GNU_SOURCE

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
#include "em_i2c.h"

#include "ff.h"
#include "fatfs-efr32.h"

#include "linklist.h"
#include "serial.h"
#include "strutil.h"
#include "iadc.h"
#include "rotor.h"
#include "pwm.h"
#include "flash.h"
#include "systick.h"
#include "rtcc.h"

#include "i2c.h"
#include "i2c/rtc-ds3231.h"

#include "sat.h"

#define LED_PORT gpioPortB
#define LED0_PIN 0
#define LED1_PIN 1

#define MAX_ARGS 10

time_t boot_time;

void dispatch(int argc, char **args, struct linklist *history);

FATFS fatfs;           /* Filesystem object */

struct flash_entry 
	flash_rotors = { .name = "rotors", .ptr = rotors, .len = sizeof(rotors) }, 
	*flash_table[] = {
		&flash_rotors,
		NULL
	};


void initGpio(void)
{
	CMU_ClockEnable(cmuClock_GPIO, true);

	// Configure PA5 as an output (TX)
	GPIO_PinModeSet(gpioPortA, 5, gpioModePushPull, 0);

	// Configure PA6 as an input (RX)
	GPIO_PinModeSet(gpioPortA, 6, gpioModeInput, 0);

	GPIO_PinModeSet(gpioPortA, 3, gpioModePushPull, 0);
	
	// PA04 is used for VCOMCTS and does not work for general use
	//GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 0);

	// Configure PD4 as output and set high (VCOM_ENABLE)
	// comment out next line to disable VCOM
	GPIO_PinModeSet(gpioPortD, 4, gpioModePushPull, 1);
	
	// pwm ports
	GPIO_PinModeSet(gpioPortC, 0, gpioModePushPull, 0);

	// PC01 is used for vcom rts but seems to work fine for a motor
	GPIO_PinModeSet(gpioPortC, 1, gpioModePushPull, 0);
	GPIO_PinModeSet(gpioPortC, 2, gpioModePushPull, 0);
	GPIO_PinModeSet(gpioPortC, 3, gpioModePushPull, 0);
	GPIO_PinModeSet(gpioPortC, 4, gpioModePushPull, 0);
	GPIO_PinModeSet(gpioPortC, 5, gpioModePushPull, 0);

	// PD00: Theta IADC
	GPIO_PinModeSet(gpioPortD, 2, gpioModeInput, 0);

	// PD01: Phi IADC
	GPIO_PinModeSet(gpioPortD, 3, gpioModeInput, 0);

	GPIO_PinModeSet(I2C_SDA_PORT, I2C_SDA, gpioModeWiredAndPullUpFilter, 1);
	GPIO_PinModeSet(I2C_SCL_PORT, I2C_SCL, gpioModeWiredAndPullUpFilter, 1);

	// turn on LED0 
	GPIO_PinModeSet(LED_PORT, LED0_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(LED_PORT, LED1_PIN, gpioModePushPull, 0);
}

void print_tm(struct tm *rtc)
{
	time_t now = mktime(rtc);
	printf("UTC: %02d/%02d/%04d  %02d:%02d:%02d  unix time: %s\r\n",
		rtc->tm_mon+1, rtc->tm_mday, rtc->tm_year + 1900,
		rtc->tm_hour, rtc->tm_min, rtc->tm_sec,
		lltoa(now, 10));
}

void help()
{
	char *h =
		"status [watch]                                        # Display system status\r\n"
		"mv <motor_name> <([+-]deg|n|e|s|w)> [<speed=0-100>]   # Moves antenna\r\n"
		"(help|?|h)                                            # List of commands\r\n"
		"flash (save|load)                                     # Save to flash\r\n"
		"sat (load|track|list|search)                          # Satellite commands\r\n"
		"motor <motor_name> (speed|online|offline|on|off|name|port|pin1|pin2|hz)\r\n"
		"rotor <rotor_name> (cal|detail|pid|target)            # Rotor commands\r\n"
		"hist|history                                          # History of commands\r\n"
		"date                                                  # Set or get the date\r\n"
		"debug-keys                                            # Print chars and hex\r\n"
		"reset|reboot                                          # Reset the CPU (reboot).\r\n";
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
	
	struct tm rtc;
	
	time_t now = rtcc_get_sec();
	gmtime_r(&now, &rtc);
	print_tm(&rtc);
	printf("Uptime: %0.2f hours\r\n", (float)(now-boot_time)/3600.0);

	for (i = 0; i < IADC_NUM_INPUTS; i++)
	{
		printf("iadc[%d]: %f volts\r\n", i, (float)iadc_get_result(i));
	}
	print("\r\n");
	
	for (i = 0; i < NUM_ROTORS; i++)
	{
		printf("%s cal: [%.2f, %.2f] deg = [%.2f, %.2f] volts, %.4f mV/deg\r\n", 
			rotors[i].motor.name,
			rotors[i].cal1.deg, rotors[i].cal2.deg,
			rotors[i].cal1.v, rotors[i].cal2.v,
			(rotors[i].cal2.v - rotors[i].cal1.v) / (rotors[i].cal2.deg - rotors[i].cal1.deg) * 1000
			);
	}

	print("\r\n");
	for (i = 0; i < NUM_ROTORS; i++)
	{
		char port = '?';

		switch (rotors[i].motor.port)
		{
			case gpioPortA: port = 'A'; break;
			case gpioPortB: port = 'B'; break;
			case gpioPortC: port = 'C'; break;
			case gpioPortD: port = 'D'; break;
		}

		printf("%s pos=%.2f deg, target=%.2f deg [%s, port=%c%d/%c%d]: P=%.1f%% I=%.1f%% D=%.1f%% speed=%.1f%%\r\n",
			rotors[i].motor.name,
			rotor_pos(&rotors[i]),
			rotors[i].target,
			!rotor_valid(&rotors[i]) ? "invalid" : 
				(rotor_online(&rotors[i]) ? "online" :
					(!motor_online(&rotors[i].motor) ? "motor offline" : "rotor offline")), 
			port,
			rotors[i].motor.pin1, 
			port,
			rotors[i].motor.pin2,
			rotors[i].pid.proportional * 100,
			rotors[i].pid.integrator * 100,
			rotors[i].pid.differentiator * 100,
			rotors[i].motor.speed * 100
			);

	//	motor_detail(&rotors[i].motor);
	//	rotor_detail(&rotors[i]);
	}
}

void motor(int argc, char **args)
{
	struct motor *m = NULL;

	float speed;

	if (argc < 3)
	{
		print("Usage: motor <motor_name> (speed|online|offline|on|off|name|port|pin1|pin2|hz|)\r\n"
			"speed <percent>       # Can be between -100 and 100, 0 is stopped\r\n"
			"speed limit <percent> # Limit duty cycle temporarily even if speed is higher\r\n"
			"                        Note: A limit below min speed will stop the motor\r\n"
			"online                # Set this motor online\r\n"
			"offline               # Set this motor offline\r\n"
			"on                    # Set this motor to full speed\r\n"
			"off/stop              # Set this motor speed to 0\r\n"
			"detail                # Show detailed settings\r\n"
			"\r\n"

			"Configuration options, `flash save` after changing:\r\n"
			"name <new_name>       # Set the name of the motor\r\n"
			"speed min <percent>   # Set minimum PWM speed, below this is stopped\r\n"
			"speed max <percent>   # Set maximum PWM speed, above this is full speed\r\n"
			"hz <Hz>               # Set the PWM freq for this motor\r\n"
			"port <0|a|b|c|d>      # Set the efr32 port for the motor, 0 is unconfigured\r\n"
			"pin1 <0-6>            # Set the efr32 pin for clockwise PWM\r\n"
			"pin2 <0-6>            # Set the efr32 pin for counter-clockwise PWM\r\n"
			);
		return;
	}

	m = motor_get(args[1]);
	if (m == NULL)
	{
		printf("Unkown Motor: %s\r\n", args[1]);
		return;
	}

	if (match(args[2], "stop") || match(args[2], "off"))
	{
		motor_speed(m, 0);
	}
	else if (match(args[2], "on"))
	{
		if (!motor_online(m))
		{
			printf("Motor is currently offline, please run: motor %s online\r\n", m->name);

			return;
		}

		if (argc >= 3 && args[3][0] == '<')
			motor_speed(m, -1);
		else
			motor_speed(m, 1);
	}
	else if (match(args[2], "detail"))
	{
		motor_detail(m);
	}
	else if (match(args[2], "speed") && argc == 4 && !isalpha((int)args[3][0]))
	{
		if (!motor_online(m))
		{
			printf("Motor is currently offline, please run: motor %s online\r\n", m->name);

			return;
		}

		speed = atof(args[3]) / 100;
		if (speed < -1 || speed > 1)
		{
			printf("Speed is out of bounds: %s\r\n", args[3]);

			return;
		}

		motor_speed(m, speed);

		printf("Speed sucessfully set to %.1f%%\r\n", speed * 100);
	}
	else if (match(args[2], "speed"))
	{
		if (argc < 5)
		{
			print("usage 1: motor <motor_name> speed (0-100)\r\n");
			print("usage 2: motor <motor_name> speed (min|max|limit) 0-100\r\n");
			return;
		}

		float value = atof(args[4]) / 100;
		
		if (match(args[3], "limit") && value > 0 && value <= 1)
		{
			m->duty_cycle_limit = value;
		}
		else if (match(args[3], "min") && value >= 0 && value <= 1)
		{
			if (m->duty_cycle_max < value)
			{
				print("Min must be less than max\r\n");
			}

			m->duty_cycle_min = value;
		}
		else if (match(args[3], "max") && value > 0 && value <= 1)
		{
			if (m->duty_cycle_min > value)
			{
				print("Max must be greater than min\r\n");
			}
			m->duty_cycle_max = value;
		}

		motor_speed(m, m->speed);
	}
	else if (match(args[2], "online"))
	{
		m->online = 1;
	}
	else if (match(args[2], "offline"))
	{
		m->online = 0;
		motor_speed(m, 0);
	}
	else if (match(args[2], "name"))
	{
		if (argc < 4)
		{
			print("usage: motor <motor_name> name <new_motor_name>\r\n");
			return;
		}

		strncpy(m->name, args[3], sizeof(m->name)-1);
	}
	else if (match(args[2], "hz"))
	{
		if (argc < 4)
		{
			print("usage: motor <motor_name> hz <PWM_freq>\r\n");
			return;
		}

		int hz = atoi(args[3]);

		float speed = m->speed;

		if (hz <= 0)
			print("Hz must be greater than 0\r\n");
		else
		{
			m->pwm_Hz = hz;
			m->speed = 0; // set it to zero to force it to refresh the speed
			motor_init(m);
			motor_speed(m, speed);
		}
	}
	else if (match(args[2], "port"))
	{
		if (argc < 4)
		{
			print("usage: motor <motor_name> port <0, a-d>\r\n");
			return;
		}

		switch (tolower(args[3][0]))
		{
			case '0': m->port = -1; break;
			case 'a': m->port = gpioPortA; break;
			case 'b': m->port = gpioPortB; break;
			case 'c': m->port = gpioPortC; break;
			case 'd': m->port = gpioPortD; break;
			default:
				printf("invalid port: %s\r\n", args[3]);
				return;
		}

		if (motor_valid(m))
			motor_init(m);
	}
	else if (match(args[2], "pin1"))
	{
		if (argc < 4)
		{
			print("usage: motor <motor_name> pin1 <0-6>\r\n");
			return;
		}

		int pin = atoi(args[3]);
		if (pin < 0 || pin > 6)
		{
			printf("invalid pin: %s\r\n", args[3]);

			return;
		}

		m->pin1 = pin;
		if (motor_valid(m))
			motor_init(m);
	}
	else if (match(args[2], "pin2"))
	{
		if (argc < 4)
		{
			print("usage: motor <motor_name> pin2 <0-6>\r\n");
			return;
		}

		int pin = atoi(args[3]);
		if (pin < 0 || pin > 6)
		{
			printf("invalid pin: %s\r\n", args[3]);

			return;
		}

		m->pin2 = pin;
		if (motor_valid(m))
			motor_init(m);
	}
	else 
		printf("Unkown or invalid motor sub-command: %s\r\n", args[2]);
}

void rotor_pid(struct rotor *r, int argc, char **args)
{
	float f;


	if (argc >= 2 && match(args[1], "reset"))
	{
		PIDController_Init(&r->pid);
		rotor_detail(r);
		return;
	}

	if (argc < 3)
	{
		print("Usage: pid (reset|var <value>)\r\n"
			"Set pid.var to value. See `rotor <name> detail` for available fields\r\n");
		return;
	}

	f = atof(args[2]);

	if (match(args[1], "Kp")) r->pid.Kp = f;
	else if (match(args[1], "Ki")) r->pid.Ki = f;
	else if (match(args[1], "Kd")) r->pid.Kd = f;

	else if (match(args[1], "kp")) r->pid.Kp = f;
	else if (match(args[1], "ki")) r->pid.Ki = f;
	else if (match(args[1], "kd")) r->pid.Kd = f;

	else if (match(args[1], "tau")) r->pid.tau = f;

	else if (match(args[1], "int_min")) r->pid.int_min = f;
	else if (match(args[1], "int_max")) r->pid.int_max = f;

	else if (match(args[1], "T")) r->pid.T = f;
	else if (match(args[1], "t")) r->pid.T = f;
	else
	{
		print("Not configurable\r\n");
		
		return;
	}

	rotor_detail(r);
}

void rotor_cal(struct rotor *r, int argc, char **args)
{
	struct rotor_cal cal;

	float deg, v;

	if (argc < 2)
	{
		print("Usage: cal <deg>\r\n"
			"Calibration assumes a linear voltage slope between degrees.\r\n"
			"You must make 2 calibrations, and each calibration is used as\r\n"
			"the maximum extent for the rotor. Use the `motor` command to\r\n"
			"move the rotor between calibrations.\r\n");
		return;
	}

	if (match(args[1], "reset"))
	{
		memset(&r->cal1, 0, sizeof(r->cal1));
		memset(&r->cal2, 0, sizeof(r->cal2));

		printf("Calibration reset: %s\r\n", r->motor.name);

		return;
	}
	deg = atof(args[1]);
	v = iadc_get_result(r->iadc);
	
	cal.deg = deg;
	cal.v = v;
	cal.ready = 1;

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

void rotor(int argc, char **args)
{
	struct rotor *r;

	if (argc < 3)
	{
		print("Usage: rotor <rotor_name> (cal|detail|pid|target)\r\n"
			"Calibration assumes a linear voltage slope between degrees.\r\n"
			"You must make 2 calibrations, and each calibration is used as\r\n"
			"the maximum extent for the rotor. Use the `motor` command to\r\n"
			"move the rotor between calibrations.\r\n");
		return;
	}

	r = rotor_get(args[1]);

	if (r == NULL)
	{
		printf("Unkown Rotor: %s\r\n", args[1]);
		return;
	}

	if (match(args[2], "cal"))
	{
		rotor_cal(r, argc-2, &args[2]);
	}
	else if (match(args[2], "detail"))
	{
		rotor_detail(r);
	}
	else if (match(args[2], "pid"))
	{
		rotor_pid(r, argc-2, &args[2]);
	}
	else if (match(args[2], "target"))
	{
		if (argc < 4)
		{
			print("usage: <rotor_name> target (on|off)\r\n"
				"Turning the target off will disable tracking for that rotor\r\n");

			return;
		}
		if (match(args[3], "on"))
			r->target_enabled = 1;
		else if (match(args[3], "off"))
		{
			r->target_enabled = 0;


			// Stop the motor or it will drift at any existing speed setting:
			motor_speed(&r->motor, 0);
		}
		else
			print("expected: on/off\r\n");
	}
	else
		printf("unexpected argument: %s\r\n", args[2]);
}

void mv(int argc, char **args)
{
	struct rotor *r;

	float deg;

	if (argc < 3)
	{
		print("mv <motor_name> <([+-]deg|n|e|s|w)>\r\n"
			"Move rotor to a degree angle. North is 0 deg.\r\n");
		return;
	}

	r = rotor_get(args[1]);

	if (r == NULL)
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
	switch (tolower(args[2][0]))
	{
		case '+':
		case '-':
			deg = r->target + deg;
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

	// Set the target degree angle
	r->target = deg;
}

void flash(int argc, char **args)
{
	FRESULT res = FR_OK;  /* API result code */
	FIL in, out;              /* File object */
	UINT bw, br;          /* Bytes written */

	if (argc < 2)
	{
		print("Usage: flash (save|load)\r\n");

		return;
	}

	systick_bypass(1);

	if (match(args[1], "write") || match(args[1], "save"))
	{
		res = f_open(&out, "cal.bin", FA_CREATE_ALWAYS | FA_WRITE);
		if (res != FR_OK)
		{
			printf("cal.bin: open error %d: %s\r\n", res, ff_strerror(res));
			goto out;
		}

		res = f_write(&out, rotors, sizeof(rotors), &bw);
		if (res != FR_OK || bw != sizeof(rotors))
		{
			printf("cal.bin: write error %d: %s (bytes written=%d/%d)\r\n",
				res, ff_strerror(res), bw, sizeof(rotors));
			goto out;
		}

		f_close(&out);
	}
	else if (match(args[1], "load"))
	{
		res = f_open(&in, "cal.bin", FA_READ);
		if (res != FR_OK)
		{
			printf("cal.bin: open error %d: %s\r\n", res, ff_strerror(res));
			goto out;
		}

		res = f_read(&in, rotors, sizeof(rotors), &br);
		if (res != FR_OK || br != sizeof(rotors))
		{
			printf("cal.bin: read error %d: %s (bytes written=%d/%d)\r\n",
				res, ff_strerror(res), br, sizeof(rotors));
			goto out;
		}

		f_close(&in);
	}
	else
	{
		printf("Unkown argument: %s\r\n", args[1]);

		goto out;
	}

out:
	systick_bypass(0);
}


void watch(int argc, char **args, struct linklist *history)
{
	char c;

	if (argc < 2)
	{
		print("usage: watch <command>\r\n"
			"Run <command> once per second until a key is pressed\r\n"
			);
			
			return;
	}

	serial_read_async(&c, 1);

	do
	{
		dispatch(argc-1, &args[1], history);
		rtcc_delay_sec(1);
		if (!serial_read_done())
		{
			print("\x0c");
		}
	}
	while (!serial_read_done());
}

void sat_pos(tle_t *tle)
{
	struct rotor *phi = rotor_get("phi");
	struct rotor *theta = rotor_get("theta");
	double
		tsince,            /* Time since epoch (in minutes) */
		jul_epoch,         /* Julian date of epoch          */
		jul_utc,           /* Julian UTC date               */
		eclipse_depth = 0, /* Depth of satellite eclipse    */
		/* Satellite's observed position, range, range rate */
		sat_azi, sat_ele, sat_range, sat_range_rate,
		/* Satellites geodetic position and velocity */
		sat_lat, sat_lon, sat_alt, sat_vel,
		/* Solar azimuth and elevation */
		sun_azi, sun_ele;
	struct tm utc;
	char *ephem, *sat_status;

	/* Observer's geodetic co-ordinates.      */
	/* Lat North, Lon East in rads, Alt in km */
	//geodetic_t obs_geodetic = {0.6056, 0.5761, 0.15, 0.0};
	geodetic_t obs_geodetic = {45*3.141592654/180, -122*3.141592654/180, 0.0762, 0.0};

	/* Satellite's predicted geodetic position */
	geodetic_t sat_geodetic;

	/* Zero vector for initializations */
	vector_t zero_vector = {0,0,0,0};

	/* Satellite position and velocity vectors */
	vector_t vel = zero_vector;
	vector_t pos = zero_vector;
	/* Satellite Az, El, Range, Range rate */
	vector_t obs_set;

	/* Solar ECI position vector  */
	vector_t solar_vector = zero_vector;
	/* Solar observed azi and ele vector  */
	vector_t solar_set;

	/** !Clear all flags! **/
	/* Before calling a different ephemeris  */
	/* or changing the TLE set, flow control */
	/* flags must be cleared in main().      */
	ClearFlag(ALL_FLAGS);

	/** Select ephemeris type **/
	/* Will set or clear the DEEP_SPACE_EPHEM_FLAG       */
	/* depending on the TLE parameters of the satellite. */
	/* It will also pre-process tle members for the      */
	/* ephemeris functions SGP4 or SDP4 so this function */
	/* must be called each time a new tle set is used    */
	select_ephemeris(tle);

	char c;
	serial_read_async(&c, 1);

	do  /* Loop */
	{
		/* Get UTC calendar and convert to Julian */
		UTC_Calendar_Now(&utc);
		jul_utc = Julian_Date(&utc);

		/* Convert satellite's epoch time to Julian  */
		/* and calculate time since epoch in minutes */
		jul_epoch = Julian_Date_of_Epoch(tle->epoch);
		tsince = (jul_utc - jul_epoch) * 24*60; // minutes per day

		/* Copy the ephemeris type in use to ephem string */
		if( isFlagSet(DEEP_SPACE_EPHEM_FLAG) )
			ephem = "SDP4";
		else
			ephem = "SGP4";

		/* Call NORAD routines according to deep-space flag */
		if( isFlagSet(DEEP_SPACE_EPHEM_FLAG) )
			SDP4(tsince, tle, &pos, &vel);
		else
			SGP4(tsince, tle, &pos, &vel);

		/* Scale position and velocity vectors to km and km/sec */
		Convert_Sat_State( &pos, &vel );

		/* Calculate velocity of satellite */
		Magnitude( &vel );
		sat_vel = vel.w;

		/** All angles in rads. Distance in km. Velocity in km/s **/
		/* Calculate satellite Azi, Ele, Range and Range-rate */
		Calculate_Obs(jul_utc, &pos, &vel, &obs_geodetic, &obs_set);

		/* Calculate satellite Lat North, Lon East and Alt. */
		Calculate_LatLonAlt(jul_utc, &pos, &sat_geodetic);

		/* Calculate solar position and satellite eclipse depth */
		/* Also set or clear the satellite eclipsed flag accordingly */
		Calculate_Solar_Position(jul_utc, &solar_vector);
		Calculate_Obs(jul_utc,&solar_vector,&zero_vector,&obs_geodetic,&solar_set);

		if( Sat_Eclipsed(&pos, &solar_vector, &eclipse_depth) )
			SetFlag( SAT_ECLIPSED_FLAG );
		else
			ClearFlag( SAT_ECLIPSED_FLAG );

		/* Copy a satellite eclipse status string in sat_status */
		if( isFlagSet( SAT_ECLIPSED_FLAG ) )
			sat_status = "Eclipsed";
		else
			sat_status = "In Sunlight";

		/* Convert and print satellite and solar data */
		sat_azi = Degrees(obs_set.x);
		sat_ele = Degrees(obs_set.y);
		sat_range = obs_set.z;
		sat_range_rate = obs_set.w;
		sat_lat = Degrees(sat_geodetic.lat);
		sat_lon = Degrees(sat_geodetic.lon);
		sat_alt = sat_geodetic.alt;

		sun_azi = Degrees(solar_set.x);
		sun_ele = Degrees(solar_set.y);

		print("\x0c\r\n");
		status();
		printf("\r\n Date: %02d/%02d/%04d UTC: %02d:%02d:%02d  Ephemeris: %s"
			"\r\n Azi=%6.1f Ele=%6.1f Range=%8.1f Range Rate=%6.2f"
			"\r\n Lat=%6.1f Lon=%6.1f  Alt=%8.1f  Vel=%8.3f"
			"\r\n Stellite Status: %s - Depth: %2.3f"
			"\r\n Sun Azi=%6.1f Sun Ele=%6.1f\r\n",
			utc.tm_mday, utc.tm_mon, utc.tm_year,
			utc.tm_hour, utc.tm_min, utc.tm_sec, ephem,
			sat_azi, sat_ele, sat_range, sat_range_rate,
			sat_lat, sat_lon, sat_alt, sat_vel,
			sat_status, eclipse_depth,
			sun_azi, sun_ele);

		theta->target = sat_azi;
		phi->target = sat_ele;
		rtcc_delay_sec(1);
	} while (!serial_read_done());
}

// tle: the tle object
// line: the tle line number
// tle_set: temp buffer that is filled while loading tle lines, must be >= 139 bytes
// buf: input text line
int sat_tle_line(tle_t *tle, int line, char *tle_set, char *buf)
{
	if (line == 0)
	{
		strncpy(tle->sat_name, buf, sizeof(tle->sat_name));
		tle->sat_name[sizeof(tle->sat_name)-1] = 0;
		line++;
	}
	else if (line == 1)
	{
		strncpy(tle_set, buf, 69);
		tle_set[69] = 0;
		line++;
	}
	else if (line == 2)
	{
		strncpy(tle_set+69, buf, 69);
		tle_set[138] = 0;
		if (Good_Elements(tle_set))
		{
			Convert_Satellite_Data(tle_set, tle);
			printf("\r\nParsed tle: %s\r\n", tle->sat_name);
			sat_detail(tle);
			line = 0;
		}
		else
			printf("\r\nInvalid tle: %s\r\n", tle->sat_name);
	}

	return line;
}

void sat(int argc, char **args)
{
	FRESULT res = FR_OK;  /* API result code */
	FIL in, out;              /* File object */
	UINT br, bw;          /* Bytes written */

	static tle_t tle;

	char buf[128], tle_set[139];

	int i;

	if (argc < 2)
	{
		 print("usage: sat (load|pos|detail|search|list|track)\r\n");

		 return;
	}

	if (match(args[1], "load"))
	{
		i = 0;

		print("Paste TLE data or press CTRL+D to abort.\r\n"
			"Press enter at the end if it does not complete on its own.\r\n");
		memset(&tle, 0, sizeof(tle));
		while (serial_read_line(buf, 80))
		{
			printf("line%d: %s\r\n", i, buf);
			i = sat_tle_line(&tle, i, tle_set, buf);
			if (i == 0)
				break;
		}
	}
	else if (match(args[1], "pos"))
	{
		sat_pos(&tle);
	}
	else if (match(args[1], "rx"))
	{
		print ("Begin sending your TLE text file via xmodem\r\n");
		int br = xmodem_rx("tle.txt");

		printf("Receved %d bytes\r\n", br);
		
		res = f_open(&in, "tle.txt", FA_READ);
		if (res != FR_OK)
		{
			printf("tle.txt: error %d: %s\r\n", res, ff_strerror(res));
			return;
		}

		res = f_open(&out, "tle.bin", FA_CREATE_ALWAYS | FA_WRITE);
		if (res != FR_OK)
		{
			printf("tle.bin: error %d: %s\r\n", res, ff_strerror(res));
			f_close(&in);
			return;
		}
		
		memset(&tle, 0, sizeof(tle));
		i = 0;
		while (res == FR_OK && f_gets(buf, 80, &in))
		{
			printf("line%d: %s\r\n", i, buf);
			i = sat_tle_line(&tle, i, tle_set, buf);
			if (i == 0)
			{
				res = f_write(&out, &tle, sizeof(tle), &bw);
				memset(&tle, 0, sizeof(tle));
			}
		}

		f_close(&in);
		f_close(&out);
	}
	else if (match(args[1], "list") || match(args[1], "search"))
	{
		res = f_open(&in, "tle.bin", FA_READ);
		if (res != FR_OK)
		{
			printf("tle.bin: error %d: %s\r\n", res, ff_strerror(res));
			f_close(&in);
			return;
		}
		
		i = 1;
		do
		{
			res = f_read(&in, &tle, sizeof(tle), &br);
			if (br < sizeof(tle))
				break;

			if (argc < 3 || strcasestr(tle.sat_name, args[2]))
				printf("%d. %s (%d)\r\n", i, tle.sat_name, tle.catnr);
			i++;
		} while (res == FR_OK);
	}
	else
		print("Sat: invalid argument\r\n");
}

void fat(int argc, char **args)
{
	FIL fil;              /* File object */
	FRESULT res = FR_OK;  /* API result code */
	UINT br, bw;          /* Bytes written */
	BYTE work[FF_MAX_SS]; /* Work area (larger is better for processing time) */
	MKFS_PARM mkfs = {
			.fmt = FM_ANY,
			.au_size = FLASH_PAGE_SIZE,
			.align = FLASH_PAGE_SIZE / 512,
			.n_fat = 0, 
			.n_root = 0};


	char buf[128];

	if (match(args[1], "mkfs"))
	{
		res = f_mkfs("", &mkfs, work, sizeof work);
		printf("res: %d\r\n", res);
	}
	else if (match(args[1], "mount"))
	{
		res = f_mount(&fatfs, "", 0);
	}
	else if (match(args[1], "umount"))
	{
		res = f_mount(NULL, "", 0);
	}
	else if (match(args[1], "find"))
	{
		buf[0] = 0;
		res = scan_files(buf);
	}
	else if (match(args[1], "load") && argc >= 3)
	{
		printf("Paste data into %s and press CTRL+D when done\r\n", args[2]);
		res = f_open(&fil, args[2], FA_CREATE_ALWAYS | FA_WRITE);
		
		while (res == FR_OK && serial_read_line(buf, 80))
		{
			res = f_write(&fil, buf, strlen(buf), &bw);
		}

		if (res != FR_OK)
			res = f_close(&fil);
	}
	else if (match(args[1], "cat") && argc >= 3)
	{
		res = f_open(&fil, args[2], FA_READ);
		printf("res: %d\r\n", res);
		
		do
		{
			res = f_read(&fil, buf, sizeof(buf)-1, &br);
//			printf("br: %d\r\n", br);
			serial_write(buf, br);
		}
		while (br > 0);

		printf("\r\n");
		res = f_close(&fil);
	}
	else if (match(args[1], "rx") && argc >= 3)
	{
		br = xmodem_rx(args[2]);

		printf("Receved %d bytes\r\n", br);
	}
	else
	{
		printf("usage: fat (rx <file>|cat <file>|load <file>|find|mkfs|mount|umount\r\n");

		return;
	}

	if (res != FR_OK)
		printf("error %d: %s\r\n", res, ff_strerror(res));
}

void dispatch(int argc, char **args, struct linklist *history)
{
	int i, c;

	if (match(args[0], "history") || match(args[0], "hist"))
	{
		dump_history(history);
	}

	else if (match(args[0], "h") || match(args[0], "?") || match(args[0], "help"))
	{
		help();
	}

	else if (match(args[0], "watch"))
	{
		watch(argc, args, history);
	}

	else if (match(args[0], "debug-keys"))
	{
		print("press CTRL+C to end\r\n");
		c = 0;
		while (c != 3)
		{
			serial_read(&c, 1);
			printf("you typed: %3d (0x%02x): '%c'\r\n", c, c, isprint(c) ? c : '?');
		}
	}

	else if (match(args[0], "sat"))
	{
		sat(argc, args);
	}
	
	else if (match(args[0], "stop"))
	{
		for (i = 0; i < NUM_ROTORS; i++)
		{
			rotors[i].motor.online = 0;
			motor_speed(&rotors[i].motor, 0);
		}
	}

	else if (match(args[0], "status") || match(args[0], "stat"))
	{
		status();
	}

	else if (match(args[0], "motor"))
	{
		motor(argc, args);
	}

	else if (match(args[0], "rotor"))
	{
		rotor(argc, args);
	}

	else if (match(args[0], "mv"))
	{
		mv(argc, args);
	}
	
	else if (match(args[0], "flash"))
	{
		flash(argc, args);
	}

	else if (match(args[0], "fat"))
		fat(argc, args);

	else if (match(args[0], "reset") || match(args[0], "reboot"))
	{
		NVIC_SystemReset();
	}

	else if  (match(args[0], "i2c"))
	{
		if (argc < 3)
		{
			print("usage: i2c <hex_addr> <num_bytes>\r\n"
				"Address is in hex, num_bytes is decimal\r\n");
			return;
		}

		int addr = strtol(args[1], NULL, 16);
		int n_bytes = atoi(args[2]);
		unsigned char *buf = malloc(n_bytes);

		if (!buf)
		{
			print("buf is null, out of memory?\r\n");
			return;
		}

		buf[0] = 0;
		i2c_master_read(addr << 1, 0, buf, n_bytes);
		for (i = 0; i < n_bytes; i++)
			printf("%d. %02X\r\n", i, buf[i]);

		free(buf);
	}
	else if (match(args[0], "date"))
	{
		struct tm rtc;
		memset(&rtc, 0, sizeof(rtc));

		if (argc >= 2 && match(args[1], "read"))
		{
			ds3231_read_time(&rtc);
			time_t now = mktime(&rtc);
			printf("clock skew: %d\r\n",
				(int)(rtcc_get_sec() - now));
			rtcc_set_sec(now);
		}
		else if (argc >= 3 && match(args[1], "scale"))
		{
			int scale = atoi(args[2]);
			rtcc_set_clock_scale(atoi(args[2]));

			if (scale == 1)
				printf("The clock is now running at normal speed.\r\n");
			else
				printf("The clock is now running %dx faster than normal.\r\n", scale);

			return;
		}
		else if (argc >= 2 && match(args[1], "set"))
		{
			if (argc == 8)
			{
				rtc.tm_year = atoi(args[2]) - 1900;
				rtc.tm_mon = atoi(args[3]) - 1;
				rtc.tm_mday = atoi(args[4]);
				rtc.tm_hour = atoi(args[5]);
				rtc.tm_min = atoi(args[6]);
				rtc.tm_sec = atoi(args[7]);
			}
			else if (argc == 3)
			{
				time_t t = atoll(args[2]);

				// RTC doesn't support earlier than y2k.
				// You can call this a y2k bug ;)
				if (t < 946684800)
					t = 946684800;
				gmtime_r(&t, &rtc);
			}
			else
			{
				print("i2c date set (unixtime|YYYY MM DD hh mm ss)\r\n"
					"  If a single integer is given, then it is seconds since Jan 1, 1970 UTC\r\n"
					"  If 6 integers are passed, then parse as YYYY MM DD hh mm ss\r\n"
					"  Note: clock only supports dates after the year 2000.\r\n");
				return;
			}

			rtcc_set_sec(mktime(&rtc));
			ds3231_write_time(&rtc);
		}
		else
		{
			printf("usage: date (read|set|scale)\r\n"
				"date read - read current time from RTC and set.\r\n"
				"date set  - set current time.\r\n"
				"date set (unixtime|YYYY MM DD hh mm ss)\r\n"
				"date scale <step_size> - artifically accelerate time by step_size times normal\r\n"
				);

			return;
		}

		mktime(&rtc);
		print_tm(&rtc);
	}

	// This must be the last else if:
	else if (!match(args[0], ""))
	{
		print("Unkown command: ");
		print(args[0]);
		print("\r\n");
	}
}

int main()
{
	FRESULT res;

	struct rotor *theta = &rotors[0], *phi = &rotors[1];
	struct linklist *history = NULL;
	char buf[128], *args[MAX_ARGS];
	
	int i, argc;

	// Chip errata
	CHIP_Init();

	// Initialize efr32 features
	initGpio();
	initUsart0();
	print("\x0c\r\n");

	initIADC();
	initI2C();

	
	// Initialize our features
	initRotors();

	theta->motor.port = gpioPortB;
	theta->motor.pin1 = 0;
	theta->motor.pin2 = 1;

	phi->motor.port = gpioPortC;
	phi->motor.pin1 = 4;
	phi->motor.pin2 = 5;

	help();
	print("\r\n");

	flash_read(flash_table, FLASH_DATA_BASE);

	// Initalize motors that were loaded from flash if they were valid
	for (i = 0; i < NUM_ROTORS; i++)
	{
		if (motor_valid(&rotors[i].motor))
		{
			motor_init(&rotors[i].motor);
		}
	}

	// Initalize systick after reading flash so that it does not change
	if (systick_init(100) != 0)
		print("Failed to set systick to 100 Hz\r\n");
	print("\r\n");
	rtcc_init(128);
	struct tm rtc;

	ds3231_read_time(&rtc);
	boot_time = mktime(&rtc);

	rtcc_set_sec(boot_time);
	
	res = f_mount(&fatfs, "", 0);
	if (res != FR_OK)
		printf("Failed to mount fatfs: %s\r\n", ff_strerror(res));

	status();
	print("\r\n");

	// These should be compiler errors:
	// Make sure that the structures are NOT larger then the padding
	if (sizeof(struct rotor) > sizeof(((struct rotor *)0)->pad))
		printf("Warning: struct rotor (%d) is bigger than its pad (%d), increase pad size for flash "
			"backwards compatibility\r\n",
			sizeof(struct rotor), sizeof(((struct rotor *)0)->pad));

	if (sizeof(struct motor) > sizeof(((struct motor *)0)->pad))
		printf("Warning: struct motor (%d) is bigger than its pad (%d), increase pad size for flash "
			"backwards compatibility\r\n",
			sizeof(struct motor), sizeof(((struct motor *)0)->pad));

	for (;;)
	{
		print("[Zeke&Daddy@console]# ");
		input(buf, sizeof(buf)-1, &history);
		print("\r\n");
		
		argc = parse_args(buf, args, MAX_ARGS);
		dispatch(argc, args, history);
	}
}
