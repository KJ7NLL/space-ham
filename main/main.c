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

#include "platform.h"

#include "ff.h"
#include "fatfs-util.h"

#include "astronomy.h"

#include "linklist.h"
#include "serial.h"
#include "strutil.h"
#include "iadc.h"
#include "rotor.h"
#include "pwm.h"
#include "flash.h"
#include "systick.h"
#include "rtcc.h"
#include "gnss.h"
#include "wifi.h"
#include "time_sync.h"
#include "http_get.h"

#include "i2c.h"
#include "i2c/rtc-ds3231.h"
#include "i2c/ads111x.h"
#include "i2c/qmc5883l.h"
#include "i2c/mmc5603nj.h"
#include "i2c/mxc4005xc.h"
#include "lcd.h"

#include "sat.h"
#include "stars.h"

#include "config.h"

#define LED_PORT gpioPortB
#define LED0_PIN 0
#define LED1_PIN 1

#define MAX_ARGS 67

time_t boot_time;

char *astro_tracked_name = NULL;
astro_body_t astro_tracked_body = BODY_INVALID;

void dispatch(int argc, char **args, struct linklist *history);
int idle_counts = 0;
void main_idle();
void meminfo();

#ifdef __EFR32__
FATFS _fatfs, *fatfs = &_fatfs;           /* Filesystem object */
#endif

#ifdef __ESP32__
FATFS *fatfs = NULL;           /* Filesystem object */
#endif

struct flash_entry 
	flash_rotors = { .name = "rotors", .ptr = rotors, .len = sizeof(rotors) }, 
	*flash_table[] = {
		&flash_rotors,
		NULL
	};

config_t config = {
	// Observer's geodetic co-ordinates.
	// Lat North, Lon East in rads, Alt in km 
	.observer = {45.0*3.141592654/180, -122.0*3.141592654/180, 0.0762, 0.0}, 
	.username = "user",
	.i2c_freq = 10000,
	.lcd_freq = 400000,
	.gnss_debug = false,
	.gnss_passthrough = false,
	.wifi_ssid = "",
	.wifi_pass = "",
};

void initGpio(void)
{
#ifdef __EFR32__
	CMU_ClockEnable(cmuClock_GPIO, true);

	// Configure PA5 as an output (TX)
	GPIO_PinModeSet(gpioPortA, 5, gpioModePushPull, 0);

	// Configure PA6 as an input (RX)
	GPIO_PinModeSet(gpioPortA, 6, gpioModeInput, 0);

	GPIO_PinModeSet(gpioPortA, 3, gpioModePushPull, 0);
	
	// PA04 is used for VCOMCTS and does not work for general use
	//GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 0);

	// Configure PD04 as output and set high (VCOM_ENABLE)
	//
	// Comment out next line to disable VCOM.  You can force VCOM enabled
	// by pulling up PD04 (P31 on the WSTK) to 3.3v through a 1k resistor
	// if you need it.
	//
	// Keeping this disabled makes PA04 (and PC01?) available for GPIO use,
	// but you have to directly access the UART via PA05(tx) and PA06(rx)
	// when PD04 is disabled.
	//
	//GPIO_PinModeSet(gpioPortD, 4, gpioModePushPull, 1);
	
	// PWM ports:
	GPIO_PinModeSet(gpioPortC, 0, gpioModePushPull, 0);
	GPIO_PinModeSet(gpioPortC, 1, gpioModePushPull, 0);
	GPIO_PinModeSet(gpioPortC, 2, gpioModePushPull, 0);
	GPIO_PinModeSet(gpioPortC, 3, gpioModePushPull, 0);

	// PD00: Theta IADC
	GPIO_PinModeSet(gpioPortD, 2, gpioModeInput, 0);

	// PD01: Phi IADC
	GPIO_PinModeSet(gpioPortD, 3, gpioModeInput, 0);

	// PA00: Telescope IADC
	GPIO_PinModeSet(gpioPortA, 0, gpioModeInput, 0);

	// PA04: UNUSED IADC
	GPIO_PinModeSet(gpioPortA, 4, gpioModeInput, 0);

	// PC00: I2C SCL
	// PC01: I2C SDA
	GPIO_PinModeSet(I2C_SDA_PORT, I2C_SDA, gpioModeWiredAndPullUpFilter, 1);
	GPIO_PinModeSet(I2C_SCL_PORT, I2C_SCL, gpioModeWiredAndPullUpFilter, 1);

	// turn on LED0 
	GPIO_PinModeSet(LED_PORT, LED0_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(LED_PORT, LED1_PIN, gpioModePushPull, 0);
#endif

#ifdef __ESP32__
	// set button GPIOs to pulldown
	gpio_config_t gpio = {0};

	gpio.pin_bit_mask = (1 << 20) | (1 << 21) | (1 << 22);
	gpio.mode = GPIO_MODE_INPUT;
	gpio.pull_up_en = GPIO_PULLUP_DISABLE;
	gpio.pull_down_en = GPIO_PULLDOWN_ENABLE;
	gpio.intr_type = GPIO_INTR_DISABLE;
	gpio_config(&gpio);
#endif
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
		"status|stat                                                     # Display system status\r\n"
		"stop                                                            # Offline all motors\r\n"
		"date (read|set|scale)                                           # Set or get the date\r\n"
		"config (latitude|longitude|altitude|username) <value>           # User configuration\r\n"
		"motor <motor_name> (speed|online|offline|...)                   # Motor commands\r\n"
		"rotor <rotor_name> (cal|detail|pid|target|ramptime|adc)         # Rotor commands\r\n"
		"pc <command_prefix>                                             # prefix a command\r\n"
		"flash (save|load)                                               # Save to flash\r\n"
		"mv <motor_name> <([+-]deg|n|e|s|w)>                             # Moves antenna\r\n"
		"sat (load|rx|demo|track|list|search)                            # Track satellites\r\n"
		"astro (list|search <body>|track <body>)                         # Track celestial bodies\r\n"
		"fat (mkfs|mount|rx <file>|cat <file>|load <file>|find|umount)   # FAT filesystem\r\n"
		"hist|history                                                    # History of commands\r\n"
		"reset|reboot                                                    # Reset the CPU (reboot)\r\n"
		"watch <command>                                                 # Repeat commands 1/sec\r\n"
		"debug-keys                                                      # Print chars and hex\r\n"
		"i2c <hex_addr> <num_bytes>                                      # Print i2c register\r\n"
		"free                                                            # Print memory info\r\n"
		"\r\n"
		"Run commands by themselves for additional help.\r\n";

	print(h);
}

void dump_history(struct linklist *history)
{
	struct llnode *temp = history->tail;
	while (temp != NULL)
	{
		print(temp->s);
		print("\r\n");
		temp = temp->prev;
	}
}

void status()
{
	int i;
	
	struct tm rtc;
	
	time_t now = time(0);
	gmtime_r(&now, &rtc);
	print_tm(&rtc);
	printf("Uptime: %0.2f hours - ", (float)(now-boot_time)/3600.0);

	printf("Your lat/long: %f N %f E; altitude: %f - ",
		Degrees(config.observer.lat), Degrees(config.observer.lon),
		config.observer.alt);

	printf("%d calcs/sec   %d i2c/sec\r\n", idle_counts, i2c_get_count());
	idle_counts = 0;

	for (i = 0; i < IADC_NUM_INPUTS; i++)
	{
		printf("iadc[%d]: %f volts\r\n", i, (float)iadc_get_result(i));
	}
	print("\r\n");
	
	for (i = 0; i < NUM_ROTORS; i++)
	{
		if (rotor_valid(&rotors[i]) && rotor_cal_valid(&rotors[i]))
		{
			printf("%-8s cal: [%7.2f, %7.2f] deg: [%4.2f, %4.2f] volts, %8.4f mV/deg\r\n",
				rotors[i].motor.name,
				rotor_cal_min(&rotors[i])->deg,
				rotor_cal_max(&rotors[i])->deg,
				rotor_cal_min(&rotors[i])->v, rotor_cal_max(&rotors[i])->v,
				(rotor_cal_max(&rotors[i])->v - rotor_cal_min(&rotors[i])->v) /
					(rotor_cal_max(&rotors[i])->deg - rotor_cal_min(&rotors[i])->deg) * 1000
				);
		}
		else
			printf("%-8s not calibrated\r\n", rotors[i].motor.name);

	}

	print("\r\n");
	for (i = 0; i < NUM_ROTORS; i++)
	{
		char port = '?';

#ifdef __EFR32__
		switch (rotors[i].motor.port)
		{
			case gpioPortA: port = 'A'; break;
			case gpioPortB: port = 'B'; break;
			case gpioPortC: port = 'C'; break;
			case gpioPortD: port = 'D'; break;
		}
#endif

		printf("%-8s pos:%7.2f tgt:%7.2f err: %6.2f deg [%s, port:%c%d/%c%d]: P:%6.1f%% I:%6.1f%% D:%6.1f%% speed:%6.1f%%\r\n",
			rotors[i].motor.name,
			rotor_pos(&rotors[i]),
			rotors[i].target,
			rotor_pos(&rotors[i]) - rotors[i].target,
			!rotor_valid(&rotors[i]) ? "invalid" : 
				(rotor_online(&rotors[i]) ? "online" :
					(!motor_online(&rotors[i].motor) ? "motor offline" : "rotor offline")), 
			port,
			rotors[i].motor.pin1, 
			port,
			rotors[i].motor.pin2,
			rotors[i].pid.P * 100,
			rotors[i].pid.I * 100,
			rotors[i].pid.D * 100,
			rotors[i].motor.speed * 100
			);

	//	motor_detail(&rotors[i].motor);
	//	rotor_detail(&rotors[i]);
	}

	// print satellite status
	sat_status();
	if (astro_tracked_name != NULL)
		printf("Tracking celestial body: %s\r\n", astro_tracked_name);
	else
		printf("No celestial body is being tracked\r\n");
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
	else if (argc == 4 && match(args[2], "speed") && !isalpha((int)args[3][0]))
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

#ifdef __EFR32__
			case 'a': m->port = gpioPortA; break;
			case 'b': m->port = gpioPortB; break;
			case 'c': m->port = gpioPortC; break;
			case 'd': m->port = gpioPortD; break;
#endif

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
		rotor_pid_reset(r);
		rotor_detail(r);
		return;
	}

	if (argc < 3)
	{
		print("Usage: rotor R pid (reset|var <value>)\r\n"
			"Set var to value. See `rotor <name> detail` for available fields\r\n"
			"See https://doi.org/10.1016/j.precisioneng.2022.01.006 for field details:\r\n"
			"reset                 # Reset PID dynamic state\r\n"
			"kp   <value>          # Proportional gain\r\n"
			"ki   <value>          # Integral gain\r\n"
			"kvfb <value>          # Differential gain\r\n"
			"kvff <value>          # Velocity feedforward gain\r\n"
			"kaff <value>          # Acceleration feedforward gain\r\n"
			"k1   <value>          # SMC stiffness\r\n"
			"k2   <value>          # SMC damping\r\n"
			"k3   <value>          # SMC phase tuning\r\n"
			"k4   <value>          # SMC phase tuning\r\n"
			"\r\n"
			"You can tune these values using `optimize-pid.pl`\r\n"
			"\r\n"
			);
		return;
	}

	f = atof(args[2]);

	if (match(args[1], "kp")) r->pid.kp = f;
	else if (match(args[1], "ki")) r->pid.ki = f;
	else if (match(args[1], "kvfb")) r->pid.kvfb = f;
	else if (match(args[1], "kvff")) r->pid.kvff = f;
	else if (match(args[1], "kaff")) r->pid.kaff = f;
	else if (match(args[1], "k1")) r->pid.k1 = f;
	else if (match(args[1], "k2")) r->pid.k2 = f;
	else if (match(args[1], "k3")) r->pid.k3 = f;
	else if (match(args[1], "k4")) r->pid.k4 = f;

	else
	{
		print("Not configurable\r\n");
		
		return;
	}

	rotor_pid_reset(r);
	rotor_detail(r);
}

void rotor_cal(struct rotor *r, int argc, char **args)
{
	int i;

	if (argc < 4)
	{
		printf( "Usage: rotor %s cal (reset|list|add ...|remove <n>|trim <deg>|offset <deg>)\r\n"
			"reset                 # Reset all calibrations\r\n"
			"list                  # List all calibrations\r\n"
			"add <deg>             # Add a new calibration as <deg> degrees\r\n"
			"add offset            # Add a calibration based on offset; see `offset` below\r\n"
			"remove <n>            # Remove an existing calibration by index from `list`\r\n"
			"trim <deg>            # Re-calculate calibration voltages to adjust by <deg>\r\n"
			"offset [=+-]<deg>     # Adjust rotor position by <deg> without voltage trim\r\n"
			"\r\n"
			"You must run `flash write` to save changes.\r\n"
			"\r\n"
			"`add offset` is useful when tracking an object and you find that your position is\r\n"
			"slightly off. You can use `rotor R cal offset N` to align your position, and\r\n"
			"then use `rotor R cal add offset` to create a calibration with voltages\r\n"
			"adjusted for the offset.\r\n"
			"\r\n"
			"Calibration supports up to %d calibration points and assumes a linear voltage\r\n"
			"slope between the degrees of any two points.  You must make at least two\r\n"
			"calibrations; the min and max calibrations are used as the maximum extents for\r\n"
			"the rotor. Use the `motor` command to move the rotor between calibrations when\r\n"
			"`rotor %s target off` is set.\r\n",
				args[1],
				ROTOR_CAL_NUM,
				args[1]
				);
		return;
	}

	if (match(args[3], "reset"))
	{
		memset(&r->cal, '\0', sizeof(r->cal));

		r->cal_count = 0;

		printf("Calibration reset: %s\r\n", r->motor.name);
	}

	else if (match(args[3], "list"))
	{
		printf(" n.      DEG    VOLTS\r\n");
		for (i = 0; i < ROTOR_CAL_NUM; i++)
			if (r->cal[i].ready)
				printf("%2d. %8.3f %8.6f\r\n", i, r->cal[i].deg, r->cal[i].v);
	}

	else if (argc >= 5 && match(args[3], "trim"))
	{
		if (!rotor_cal_trim(r, atof(args[4])))
			printf("Trim failed: Make sure you have at least 2 calibrations with unique degree angles.\r\n");
	}

	else if (argc >= 5 && (match(args[3], "remove") || match(args[3], "delete")))
	{
		int idx = atoi(args[4]);

		rotor_cal_remove(r, idx);

		if (r->cal_count < 2)
			printf("Please add additional calibrations for interpolation\r\n");
	}

	else if (argc >= 5 && match(args[3], "add"))
	{
		if (match(args[4], "offset"))
		{
			rotor_cal_add(r, r->target);

			r->offset = 0;
		}
		else
			rotor_cal_add(r, atof(args[4]));

		if (r->cal_count == 1)
			printf("Not done yet, please add a second caliberation for interpolation\r\n");
		else if (r->cal_count == 2)
			printf("Now you can add up to %d calibrations for fine-tuning\r\n", ROTOR_CAL_NUM);
	}

	else if (argc >= 5 && match(args[3], "offset"))
	{
		switch (args[4][0])
		{
			case '+':
			case '-':
				r->offset += atof(args[4]);
				break;

			case '=':
				r->offset = atof(args[4]+1);
				break;

			default:
				r->offset = atof(args[4]);
		}

	}

	else
	{
		printf("invalid or incomplete sub-command: rotor %s cal %s\r\n", args[1], args[3]);
	}
}

void rotor(int argc, char **args)
{
	struct rotor *r;

	if (argc < 3)
	{
		print("Usage: rotor <rotor_name> (cal ...|detail|pid ...|target (on|off)|ramptime <sec>|stat [n])\r\n"
			"cal                     # Calibrate degree to ADC mappings\r\n"
			"detail                  # Show detailed rotor info\r\n"
			"adc (type|addr|channel) # ADC settings\r\n"
			"pid                     # PID Controller settings\r\n"
			"target (on|off)         # Turn on/off target tracking\r\n"
			"ramptime <sec>          # Set min time to full speed\r\n"
			"static   <deg>          # static dwell: stop rotor within <deg> degrees of target\r\n"
			"dynamic  <deg>          # dynamic dwell: do not backtrack within <deg> while tracking\r\n"
			"stat [n]                # Show rotor status, optionally n times\r\n"
			"\r\n"
			"Run a subcommand without arguments for more detail\r\n"
			);
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
		rotor_cal(r, argc, args);
	}
	else if (match(args[2], "detail"))
	{
		rotor_detail(r);
	}
	else if (match(args[2], "stat"))
	{
		float min_err = 1e9, max_err = -1e9;
		float prev_range_err = 0;
		float avg_range_err = 0;
		int n = 0;
		do
		{
			float err = rotor_pos(r) - r->target;
			if (err > 180)
				err -= 360;
			if (err > max_err)
				max_err = err;
			if (err < min_err)
				min_err = err;

			if (n)
				avg_range_err += fabs(prev_range_err - err);

			n++;

			printf("%-8s pos:%7.2f tgt:%7.2f err: %6.2f deg: P:%6.2f%% I:%6.2f%% D:%6.2f%% FF:%6.2f%% S:%6.2f%% out=%6.2f%% speed:%6.4f%% tgt err: max=%5.3f prev=%5.3f avg=%5.3f\r\n",
				r->motor.name,
				rotor_pos(r),
				r->target,
				err,
				r->pid.P * 100,
				r->pid.I * 100,
				r->pid.D * 100,
				r->pid.FF * 100,
				r->pid.S * 100,
				r->pid.out * 100,
				r->motor.speed * 100,
				fabs(max_err - min_err),
				fabs(prev_range_err - err),
				avg_range_err / n
				);

				if (argc > 3 && n >= atoi(args[3]))
					break;

				rtcc_delay_sec(0.5, main_idle);
				prev_range_err = err;
		}
		while (serial_read_char() == -1);
	}
	else if (match(args[2], "pid"))
	{
		rotor_pid(r, argc-2, &args[2]);
	}
	else if (match(args[2], "target"))
	{
		if (argc < 4)
		{
			print("usage: rotor <rotor_name> target (on|off)\r\n"
				"Turning the target off will disable tracking for that rotor\r\n");

			return;
		}
		if (match(args[3], "on"))
			r->target_enabled = 1;
		else if (match(args[3], "off"))
		{
			r->target_enabled = 0;

			rotor_pid_reset(r);

			// Stop the motor or it will drift at any existing speed setting:
			motor_speed(&r->motor, 0);
		}
		else
			print("expected: on/off\r\n");
	}
	else if (match(args[2], "adc"))
	{
		if (argc < 5)
		{
			print("usage: rotor <rotor_name> adc type (0|1)\r\n"
				"rotor <rotor_name> adc addr <hex_addr>\r\n"
				"rotor <rotor_name> adc vref <pga_value>\r\n"
				"rotor <rotor_name> adc channel <channel>\r\n"
				"\r\n"
				"pga_value is the programable gain value\r\n"
				"defined in the ads111x datasheet. These are the\r\n"
				"values:\r\n"
				"0. ADS111X_PGA_6144MV\r\n"
				"1. ADS111X_PGA_4096MV\r\n"
				"2. ADS111X_PGA_2048MV\r\n"
				"3. ADS111X_PGA_1024MV\r\n"
				"4. ADS111X_PGA_0512MV\r\n"
				"5. ADS111X_PGA_0256MV\r\n"
				"\r\n"
				"Use the number next to the desired voltage\r\n"
				);

			return;
		}

		if (match(args[3], "type"))
		{
			r->adc_type = atoi(args[4]);
		}
		else if (match(args[3], "addr"))
		{
			int addr = strtol(args[4], NULL, 16);

			r->adc_addr = addr;
		}
		else if (match(args[3], "channel"))
		{
			r->adc_channel = atoi(args[4]);
		}
		else if (match(args[3], "vref"))
		{
			r->adc_vref = atoi(args[4]);
		}
		else
		{
			printf("Unkown sub-command: %s\r\n", args[3]);

			return;
		}

		rotor_adc_init(r);
	}
	else if (match(args[2], "exp"))
	{
		if (argc < 4)
		{
			print("usage: rotor <rotor_name> exp <exponent>\r\n"

				"Internally, speeds are values from 0-1. Thus, if we raise the speed to a power,\r\n"
				"then smaller values are biased toward even smaller values. The motor speed\r\n"
				"returned by the pid controller is raised to this exponent before setting the\r\n"
				"motor speed. This allows the motor to slow down as it approaches it's target,\r\n"
				"and minimizes near-target jitter. The default value of 1.0 is linear, thus\r\n"
				"using the pid controller without change. We find a value between 1.1 and 2.0 to\r\n"
				"be effective.\r\n");

			return;
		}

		r->speed_exp = atof(args[3]);
		if (r->speed_exp < 1)
		{
			print("Exponent reset to minimum value of 1.\r\n");

			r->speed_exp = 1;
		}
	}
	else if (match(args[2], "ramptime"))
	{
		if (argc < 4)
		{
			print("usage: rotor <rotor_name> ramptime <seconds>\r\n"
				"Set the time to maximum speed in seconds, or 0 to disable\r\n");

			return;
		}

		r->ramp_time = atof(args[3]);
		if (r->ramp_time < 0)
		{
			print("What are you thinking?!, this is not a time\r\n"
				"machine. Ramp time has been disabled\r\n");

			r->ramp_time = 0;
		}
	}
	else if (match(args[2], "static"))
	{
		if (argc < 4)
		{
			printf("usage: rotor <rotor_name> static <deg>\r\n"
				"Set the err in degrees before the rotor will move to correct it's position\r\n");

			return;
		}

		r->pid.stationary = atof(args[3]);
	}
	else if (match(args[2], "dynamic"))
	{
		if (argc < 4)
		{
			printf("usage: rotor <rotor_name> dynamic <deg>\r\n"
				"Set to allow the rotor to move only in\r\n"
				"the same direction as the satellite/celestial\r\n"
				"object while err is within <deg>\r\n"
				"\r\n"
				"Set to 0 disable\r\n");

			return;
		}

		r->pid.one_dir_motion = atof(args[3]);
	}

	else
		printf("unexpected argument: %s\r\n", args[2]);
}

void mv(int argc, char **args)
{
	struct rotor *r;

	float deg;

	if (argc != 3)
	{
		print("mv <motor_name> <([=+-]deg|n|e|s|w)>\r\n"
			"Move rotor to a degree angle.\r\n"
			"<deg>                 # Move to nearest <deg> mod 360\r\n"
			"=<deg>                # Move to absolute position <deg>\r\n"
			"+<deg>                # Add <deg> degrees to current position\r\n"
			"-<deg>                # Subtract <deg> degrees from current position\r\n"
			"north|n               # Point north (deg=0)\r\n"
			"east|e                # Point east  (deg=90)\r\n"
			"south|s               # Point south (deg=180)\r\n"
			"west|w                # Point west  (deg=270)\r\n"
			);
		return;
	}

	r = rotor_get(args[1]);

	if (r == NULL)
	{
		printf("Unkown Rotor: %s\r\n", args[1]);
		return;
	}

	if (!rotor_valid(r))
	{
		printf("Rotor %s is not calibrated, please calibrate and try again.\r\n", r->motor.name);
		return;
	}

	if (!r->target_enabled)
		printf("Warning: Applied, but rotor target is disabled. Use `rotor %s target on`\r\n", r->motor.name);

	switch (tolower(args[2][0]))
	{
		case '+':
		case '-':
			deg = atof(args[2]);
			deg = r->target + deg;
			r->target_absolute = 1;
			break;

		case '=':
			deg = atof(args[2]+1);
			r->target_absolute = 1;
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

		default:
			deg = atof(args[2]);
			break;
	}

	if (rotor_cal_valid(r) && (deg < rotor_cal_min(r)->deg || deg > rotor_cal_max(r)->deg))
	{
		printf("Cannot move rotor outside of calibrated range: %.2f !< %.2f !< %.2f\r\n",
			rotor_cal_min(r)->deg, deg, rotor_cal_max(r)->deg);

		return;
	}

	// Set the target degree angle
	r->target = deg;
}

void flash(int argc, char **args)
{

	if (argc < 2)
	{
		print("Usage: flash (save|load)\r\n");

		return;
	}

	systick_bypass(1);

	if (match(args[1], "write") || match(args[1], "save"))
	{
		rotor_cal_save();
	}
	else if (match(args[1], "load"))
	{
		rotor_cal_load();
	}
	else
	{
		printf("Unkown argument: %s\r\n", args[1]);
	}

	systick_bypass(0);
}


void watch(int argc, char **args, struct linklist *history)
{
	int c;

	if (argc < 2)
	{
		print("usage: watch <command>\r\n"
			"Run <command> once per second until a key is pressed\r\n"
			);
			
			return;
	}

	print("\x1b[3J\x1b[H\x1b[2J");
	do
	{
		dispatch(argc-1, &args[1], history);
		rtcc_delay_sec(1, main_idle);

		c = serial_read_char();
		if (c == -1)
		{
			print("\x1b[H");
		}
	}
	while (c == -1);
}

void cmd_prefix(int argc, char **args, struct linklist *history)
{
	int i;
	int argc2;

	char buf[128];
	char *inbuf;

	if (argc == 1)
	{
		printf("Usage: pc <command_prefix>\r\n"
		       "\r\n"
		       "Prepend every command you type with <command_prefix>.\r\n"
		       "For example: if you run `pc rotor theta`, then all you\r\n"
		       "have to run is `cal...` or whatever theta command you\r\n"
		       "wish to run because `rotor theta` is already typed for\r\n"
		       "you.\r\n"
		       "\r\n"
		       "Press enter to return to normal input\r\n"
		       );
		return;
	}

	buf[0] = 0;
	for (i = 1; i < argc; i++)
	{
		strncat(buf, args[i], sizeof(buf)-1);

		if (i < argc-1)
			strncat(buf, " ", sizeof(buf)-1);
	}

	inbuf = buf+strlen(buf);

	printf("Press <enter> to quit\r\n");
	do
	{
		inbuf[0] = 0;
		printf("[%s@prefix]# %s: ", config.username, buf);

		fflush(stdout);

		input(inbuf, sizeof(buf)-strlen(buf), history, main_idle);
		print("\r\n");

		argc2 = parse_args(inbuf, args+argc, MAX_ARGS - argc);
		if (argc2 > 0)
			dispatch(argc+argc2-1, args+1, history);
	} while (argc2 > 0);
}

void sat(int argc, char **args)
{
	FRESULT res = FR_OK;  /* API result code */
	FIL in;              /* File object */
	UINT br;          /* Bytes written */

	static tle_t tle;

	char buf[128], tle_set[139];

	int i;

	if (argc < 2)
	{
		 print("usage: sat (load|rx|search|list|track|demo)\r\n"
			"load                  # Paste a single TLE for for tracking\r\n"
			"rx                    # Recieve TLEs via xmodem\r\n"
			"list                  # Show all loaded satellites\r\n"
			"search <text>         # Find satellite by name\r\n"
			"track <satname|N>     # Track a satellite by name or number\r\n"
			"demo [<seconds>]      # Track each satellite for N seconds\r\n"
		 );

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
			{
				sat_init(&tle);
				status();
				break;
			}
		}
	}
	else if (match(args[1], "rx"))
	{
		print ("Begin sending your TLE text file via xmodem\r\n");
		int br = xmodem_rx("tle.txt");

		printf("Receved %d bytes\r\n", br);
		sat_tle_to_bin();
	}
	else if (match(args[1], "tle_to_bin"))
	{
		sat_tle_to_bin();
	}
	else if (match(args[1], "reset"))
	{
		sat_reset();
		rotor_suspend_all();
	}
	else if (match(args[1], "list") ||
		match(args[1], "search") ||
		match(args[1], "track"))
	{
		res = f_open(&in, "tle.bin", FA_READ);
		if (res != FR_OK)
		{
			printf("tle.bin: error %d: %s\r\n", res, ff_strerror(res));
			f_close(&in);
			return;
		}
		
		const sat_t *sat;
		tle_t tle_tmp;

		i = 1;

		int n = 0, found = 0;

		float degrees = -INFINITY;

		if (argc == 3)
			n = atoi(args[2]);
		else if (argc >= 4 && match(args[2], "above"))
				degrees = atof(args[3]);

		printf("  n. [CAT #] SATELLITE                   AZI    ELE\r\n");
		printf("===================================================\r\n");
		do
		{
			res = f_read(&in, &tle_tmp, sizeof(tle_tmp), &br);
			if (br < sizeof(tle_tmp))
				break;

			if (n == i ||
				n == tle.catnr ||
				(argc == 3 && n == 0 &&
					strcasestr(tle_tmp.sat_name, args[2])) ||
				argc >= 4 || argc < 3)
			{
				tle = tle_tmp;
				sat = sat_init(&tle);
				if (sat->sat_el > degrees)
				{
					printf("%3d. [%5d] %-24s %6.2f %6.2f\r\n",
						i, tle.catnr, tle.sat_name,
						sat->sat_az, sat->sat_el);
				}
				found++;
			}
			i++;
		} while (res == FR_OK);

		sat_reset();

		if (match(args[1], "track"))
		{
			if (found == 1)
			{
				sat_init(&tle);
				status();
			}
			else
				printf("\r\nYou found %d satellites, restrict your"
					" search and try again\r\n", found);
		}
	}
	else if (match(args[1], "demo"))
	{
		res = f_open(&in, "tle.bin", FA_READ);
		if (res != FR_OK)
		{
			printf("tle.bin: error %d: %s\r\n", res, ff_strerror(res));
			f_close(&in);
			return;
		}

		tle_t tle_tmp;
		i = 1;
		int n = 3, c = -1;
		if (argc >= 3)
			n = atoi(args[2]);

		do
		{
			res = f_read(&in, &tle_tmp, sizeof(tle_tmp), &br);
			if (br < sizeof(tle_tmp))
				break;

			memcpy(&tle, &tle_tmp, sizeof(tle_t));
			sat_init(&tle);
			for (int t = 0; t < n && c == -1; t++)
			{
				c = serial_read_char();

				print("\x0c");
				status();
				rtcc_delay_sec(1, main_idle);
			}
			printf("%d. %s (%d)\r\n", i, tle.sat_name, tle.catnr);
			i++;
		} while (res == FR_OK && c == -1);
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
		res = f_mount(fatfs, "", 0);
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
	else if (argc >= 3 && match(args[1], "load"))
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
	else if (argc >= 3 && match(args[1], "cat"))
	{
		res = f_open(&fil, args[2], FA_READ);
		printf("res: %d\r\n", res);
		
		do
		{
			res = f_read(&fil, buf, sizeof(buf)-1, &br);
			serial_write(buf, br);
		}
		while (br > 0);

		res = f_close(&fil);
	}
	else if (argc >= 3 && match(args[1], "rx"))
	{
		br = xmodem_rx(args[2]);

		printf("Receved %d bytes\r\n", br);
	}
	else if (argc >= 3 && match(args[1], "tx"))
	{
		bw = xmodem_tx(args[2]);

		printf("sent %d bytes\r\n", bw);
	}
	else if (argc >= 4 && match(args[1], "http_get"))
	{
		http_get(args[2], args[3]);
	}
	else
	{
		printf("Usage: fat (mkfs|mount|rx <file>|cat <file>|load <file>|find|umount|http_get <file> <url>\r\n");

		return;
	}

	if (res != FR_OK)
		printf("error %d: %s\r\n", res, ff_strerror(res));
}

void astro(int argc, char **args)
{
	static const astro_body_t body[] = {
		BODY_SUN, BODY_MOON, BODY_MERCURY, BODY_VENUS, BODY_EARTH, BODY_MARS,
		BODY_JUPITER, BODY_SATURN, BODY_URANUS, BODY_NEPTUNE,
#ifdef __ESP32__
		// Requires a big stack
		BODY_PLUTO,
#endif
	};

	astro_observer_t observer;
	astro_time_t time;
	astro_equatorial_t equ_ofdate, equ_2000;
	astro_horizon_t hor;
	astro_illum_t imag;
	int found_planet_idx = -1, found_star_idx = -1;
	int i;
	int num_bodies = sizeof(body) / sizeof(body[0]);

	time = Astronomy_CurrentTime();

	observer.latitude = Degrees(config.observer.lat);
	observer.longitude = Degrees(config.observer.lon);
	observer.height = config.observer.alt * 1000;	// km to m

	if (argc < 2)
		printf("usage: astro (list|search <body>|track <body>)\r\n"
			"list [above <deg>]    # Show all celestial bodies\r\n"
			"search <text>         # Find celestial body by name\r\n"
			"track <body|N>        # Track a body by name or number\r\n"
		);

	else if (match(args[1], "reset"))
	{
		astro_tracked_name = NULL;
		astro_tracked_body = BODY_INVALID;
	}

	else if (match(args[1], "list") ||
		match(args[1], "search") ||
		match(args[1], "track"))
	{
		int n = 0, found = 0, idx = 0;

		float degrees = -INFINITY;

		int c = -1;

		if (argc == 3)
			n = atoi(args[2]);
		else if (argc >= 4 && match(args[2], "above"))
				degrees = atof(args[3]);

		printf("  n. BODY                                        RA      DEC       AZ      ALT      MAG\r\n");
		for (i = 0; i < num_bodies && c == -1; i++)
		{
			c = serial_read_char();
			idx++;

			if (n != idx
				&& argc == 3
				&& !strcasestr(Astronomy_BodyName(body[i]), args[2]))
				continue;

			found++;
			found_planet_idx = i;

			equ_2000 = Astronomy_Equator(body[i], &time, observer, EQUATOR_J2000, ABERRATION);
			if (equ_2000.status != ASTRO_SUCCESS)
			{
				printf("%s: Astronomy_Equator returned status %d trying to get J2000 coordinates.\r\n",
					Astronomy_BodyName(body[i]),
					equ_2000.status);
			}

			equ_ofdate = Astronomy_Equator(body[i], &time, observer, EQUATOR_OF_DATE, ABERRATION);
			if (equ_ofdate.status != ASTRO_SUCCESS)
			{
				printf("%s: Astronomy_Equator returned status %d trying to get coordinates of date.\r\n",
					Astronomy_BodyName(body[i]),
					equ_ofdate.status);
			}

			imag = Astronomy_Illumination(body[i], time);

			hor = Astronomy_Horizon(&time, observer, equ_ofdate.ra,
						equ_ofdate.dec, REFRACTION_NORMAL);

			if (hor.altitude > degrees)
				printf("%3d. %-37s %8.2lf %8.2lf %8.2lf %8.2lf %8.2lf\r\n",
				       idx, Astronomy_BodyName(body[i]),
				       equ_2000.ra, equ_2000.dec,
				       hor.azimuth, hor.altitude, imag.mag);
		}

		for (i = 0; i < NUM_STARS && c == -1; i++)
		{
			c = serial_read_char();
			idx++;

			if (n != idx
				&& argc == 3
				&& !strcasestr(stars[i].name, args[2]))
				continue;

			found++;
			found_star_idx = i;

			Astronomy_DefineStar(BODY_STAR8,
				stars[i].ra, stars[i].dec, stars[i].dist_ly);

			equ_ofdate = Astronomy_Equator(BODY_STAR8, &time, observer, EQUATOR_OF_DATE, ABERRATION);
			if (equ_ofdate.status != ASTRO_SUCCESS)
			{
				printf("%s: Astronomy_Equator returned status %d trying to get coordinates of date.\r\n",
					stars[i].name,
					equ_ofdate.status);
			}

			// Secretly use the ra/dec from Astronomy_Equator(EQUATOR_OF_DATE)
			// for horizontal position to correct for light travel,
			// parallax, and aberration---but show the star's
			// actual J2000 ra/dec for informational display.
			hor = Astronomy_Horizon(&time, observer, equ_ofdate.ra, equ_ofdate.dec, REFRACTION_NORMAL);

			if (hor.altitude > degrees)
				printf("%3d. %-37s %8.2lf %8.2lf %8.2lf %8.2lf %8.2lf\r\n",
				       idx, stars[i].name,
				       stars[i].ra, //equ_ofdate.ra,
				       stars[i].dec,// equ_ofdate.dec,
				       hor.azimuth, hor.altitude,
				       stars[i].mag_vis);
		}

		if (match(args[1], "track"))
		{
			if (found == 1)
			{
				if (found_planet_idx >= 0)
				{
					astro_tracked_name = (char*)Astronomy_BodyName(body[found_planet_idx]);

					astro_tracked_body = body[found_planet_idx];
				}

				if (found_star_idx >= 0)
				{
					Astronomy_DefineStar(BODY_STAR1,
						stars[found_star_idx].ra, stars[found_star_idx].dec, stars[found_star_idx].dist_ly);
					astro_tracked_name = stars[found_star_idx].name;
					astro_tracked_body = BODY_STAR1;
				}

				status();
			}
			else
				printf("\r\nYou found %d celestial bodies, restrict your"
					" search and try again\r\n", found);
		}
	}
	else
		printf("unkown sub-command: %s\r\n", args[1]);
}

void meminfo()
{
#ifdef __EFR32__
	// 0x20000000 is the memory base on the EFR32:
	char *p = malloc(1);
	printf("heap : %p\r\n", p);
	printf("stack: %p\r\n", &p);
	printf("heap used : %5d bytes\r\n", (int)p - 0x20000000);
	printf("stack used: %5d bytes\r\n", (0x20000000+96*1024)- (int)(&p));
	free(p);
#endif

#ifdef __ESP32__
	printf("=== MALLOC_CAP_INTERNAL\r\n");
	heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);

	printf("\r\n=== MALLOC_CAP_8BIT\r\n");
	heap_caps_print_heap_info(MALLOC_CAP_8BIT);

	char *stats = malloc(2048);
	if (stats == NULL)
	{
		printf("failed to allocate memory\r\n");
		return;
	}

	printf("\r\n=== TASK STATS\r\n");
	printf("name\t\trun ctr\t\tavg %%cpu\r\n");
	vTaskGetRunTimeStats(stats);
	printf(stats);
	printf("text length=%d\r\n", strlen(stats));

	printf("\r\n=== TASK LIST\r\n");
	printf("Name\t\tstate\tprio\tcore?\tmin free stack\ttask number\r\n");
	vTaskList(stats);
	printf(stats);
	printf("text length=%d\r\n", strlen(stats));
	printf("Tasks are reported as blocked (B), ready (R), deleted (D) or suspended (S).\r\n");

	printf("before free\r\n");
	free(stats);
#endif


	printf("sizeof(rotors): %d bytes\r\n", (int)sizeof(rotors));
}

void dispatch(int argc, char **args, struct linklist *history)
{
	int i, c;

	if (argc <= 0)
		return;

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

	else if (match(args[0], "pc"))
	{
		cmd_prefix(argc, args, history);
	}

	else if (match(args[0], "free"))
	{
		meminfo();
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

	else if (match(args[0], "astro"))
	{
		astro(argc, args);
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
#ifdef __EFR32__
		NVIC_SystemReset();
#else
		system("stty cooked echo");
		exit(0);
#endif
	}
	else if (match(args[0], "config"))
	{
		if (argc < 3)
		{
			print("usage: config (latitude|longitude|altitude|username) <value>\r\n"
				"latitude|lat   <deg>  # North latitude in degrees (-deg for south)\r\n"
				"longitude|long <deg>  # East longitude in degrees (-deg for west)\r\n"
				"altitude|alt   <km>   # Altitude in kilometers\r\n"
				"username|user  <user> # Your callsign/name\r\n"
				"uplink         <mhz>  # Uplink frequency in MHz for doppler\r\n"
				"downlink       <mhz>  # Downlink frequency in MHz for doppler\r\n"
				"wifissid       <ssid> # Your wifi ssid\r\n"
				"wifipass       <ssid> # Your wifi password\r\n"
			);
			return;
		}

		if (match(args[1], "lat") || match(args[1], "latitude"))
			config.observer.lat = Radians(atof(args[2]));
		else if (match(args[1], "long") || match(args[1], "longitude"))
			config.observer.lon = Radians(atof(args[2]));
		else if (match(args[1], "alt") || match(args[1], "altitude"))
			config.observer.alt = atof(args[2]);
		else if (match(args[1], "user") || match(args[1], "username"))
			strncpy(config.username, args[2], sizeof(config.username)-1);
		else if (match(args[1], "uplink"))
			config.uplink_mhz = atof(args[2]);
		else if (match(args[1], "downlink"))
			config.downlink_mhz = atof(args[2]);
		else if (match(args[1], "i2cfreq"))
			config.i2c_freq = atoi(args[2]);
		else if (match(args[1], "lcdfreq"))
			config.lcd_freq = atoi(args[2]);
		else if (match(args[1], "wifipass"))
			strncpy(config.wifi_pass, args[2], sizeof(config.wifi_pass)-1);
		else if (match(args[1], "wifissid"))
			strncpy(config.wifi_ssid, args[2], sizeof(config.wifi_ssid)-1);
		else
		{
			printf("invalid setting: %s\r\n", args[1]);
			return;
		}

		f_write_file("config.bin", &config, sizeof(config));
	}

	else if (match(args[0], "wifi"))
	{
		if (argc < 2)
		{
			printf("Usage: wifi (scan|connect)\r\n");
			return;
		}

		if (match(args[1], "connect"))
			wifi_connect(config.wifi_ssid, config.wifi_pass);
	}

	else if  (match(args[0], "i2c"))
	{
		if (argc >= 5 && match(args[1], "read"))
		{
			int addr = strtol(args[2], NULL, 16);
			int target = strtol(args[3], NULL, 16);
			int n_bytes = atoi(args[4]);
			unsigned char *buf = malloc(n_bytes);
			memset(buf, 0, n_bytes);

			if (!buf)
			{
				print("buf is null, out of memory?\r\n");
				return;
			}

			buf[0] = 0;
			i2c_master_read(addr << 1, target, buf, n_bytes);
			for (i = 0; i < n_bytes; i++)
				printf("%d. %02X\r\n", i, buf[i]);

			free(buf);
		}
		else if (argc >= 5 && match(args[1], "write"))
		{
			int addr = strtol(args[2], NULL, 16);
			int target = strtol(args[3], NULL, 16);
			int n_bytes = (unsigned)argc - 4;
			unsigned char *buf = malloc(n_bytes);
			memset(buf, 0, n_bytes);

			printf("Sending %d bytes to 0x%02X\r\n", n_bytes, addr);

			if (!buf)
			{
				print("buf is null, out of memory?\r\n");
				return;
			}

			for (i = 0; i < n_bytes; i++)
			{
				buf[i] = strtol(args[i+4], NULL, 16);
				printf("%d. %02X\r\n", i, buf[i]);
			}

			i2c_master_write(addr << 1, target, buf, n_bytes);

			free(buf);
		}
		else if (argc >= 3 && match(args[1], "adc"))
		{
			float value = 0;
			int addr = strtol(args[2], NULL, 16);

			value = ads111x_measure_req((ads111x_t *)i2c_req_get_cont(addr));

			printf("voltage: %.12f\r\n", value);
		}
		else if (argc >= 2 && match(args[1], "list"))
		{
			const volatile struct linklist *reqs = i2c_req_cont_list();

			struct llnode *node = reqs->head;
			printf("i2c cont completions: %5d\r\n", i2c_get_count());
			while (node != NULL)
			{
				i2c_req_t *req = node->data;
				printf("%02x: %s: target=%02x valid=%d age=%d samples=%d errors=%d\r\n",
					req->addr >> 1,
					req->name,
					req->target,
					req->valid,
					(int)(time(0) - req->complete_time),
					req->sample_count,
					req->err_count);

				for (i = 0; i < req->n_bytes; i++)
					printf("  %d. %02X\r\n", i, req->result[i]);

				if (match(req->name, "ads111x"))
					printf("  %.9f volts\r\n",
						ads111x_measure_req((ads111x_t*)req));
				else if (match(req->name, "qmc5883l"))
				{
					printf("  x=%6d y=%6d z=%6d (raw)\r\n",
						qmc5883l_measure_req_raw((qmc5883l_t*)req, QMC5883L_DATA_X),
						qmc5883l_measure_req_raw((qmc5883l_t*)req, QMC5883L_DATA_Y),
						qmc5883l_measure_req_raw((qmc5883l_t*)req, QMC5883L_DATA_Z)
						);
					printf("  x=%6.3f y=%6.3f z=%6.3f (calibrated -1 to 1)\r\n",
						qmc5883l_measure_req((qmc5883l_t*)req, QMC5883L_DATA_X),
						qmc5883l_measure_req((qmc5883l_t*)req, QMC5883L_DATA_Y),
						qmc5883l_measure_req((qmc5883l_t*)req, QMC5883L_DATA_Z)
						);
					printf("  xy=%8.3f xz=%8.3f yz=%8.3f (deg)\r\n",
						qmc5883l_measure_req_plane((qmc5883l_t*)req, QMC5883L_PLANE_XY),
						qmc5883l_measure_req_plane((qmc5883l_t*)req, QMC5883L_PLANE_XZ),
						qmc5883l_measure_req_plane((qmc5883l_t*)req, QMC5883L_PLANE_YZ)
						);
					qmc5883l_t *mag = (qmc5883l_t*)req;
					printf("  x cal range: %6d - %6d\r\n"
					       "  y cal range: %6d - %6d\r\n"
					       "  z cal range: %6d - %6d\r\n",
					       mag->min_x, mag->max_x,
					       mag->min_y, mag->max_y,
					       mag->min_z, mag->max_z);
				}
				else if (match(req->name, "mmc5603nj"))
				{
					printf("  x=%6d y=%6d z=%6d (raw)\r\n",
						mmc5603nj_measure_req_raw((mmc5603nj_t*)req, MMC5603NJ_DATA_X),
						mmc5603nj_measure_req_raw((mmc5603nj_t*)req, MMC5603NJ_DATA_Y),
						mmc5603nj_measure_req_raw((mmc5603nj_t*)req, MMC5603NJ_DATA_Z)
						);
					printf("  x=%6.3f y=%6.3f z=%6.3f (calibrated -1 to 1)\r\n",
						mmc5603nj_measure_req((mmc5603nj_t*)req, MMC5603NJ_DATA_X),
						mmc5603nj_measure_req((mmc5603nj_t*)req, MMC5603NJ_DATA_Y),
						mmc5603nj_measure_req((mmc5603nj_t*)req, MMC5603NJ_DATA_Z)
						);
					printf("  xy=%8.3f xz=%8.3f yz=%8.3f (deg)\r\n",
						mmc5603nj_measure_req_plane((mmc5603nj_t*)req, MMC5603NJ_PLANE_XY),
						mmc5603nj_measure_req_plane((mmc5603nj_t*)req, MMC5603NJ_PLANE_XZ),
						mmc5603nj_measure_req_plane((mmc5603nj_t*)req, MMC5603NJ_PLANE_YZ)
						);

					mmc5603nj_t *mag = (mmc5603nj_t*)req;
					printf("  x cal range: %6d - %6d\r\n"
					       "  y cal range: %6d - %6d\r\n"
					       "  z cal range: %6d - %6d\r\n",
					       (int)mag->min_x, (int)mag->max_x,
					       (int)mag->min_y, (int)mag->max_y,
					       (int)mag->min_z, (int)mag->max_z);
				}
				else if (match(req->name, "mxc4005xc"))
				{
					printf("  x=%6d y=%6d z=%6d (raw)\r\n",
						mxc4005xc_measure_req_raw((mxc4005xc_t*)req, MXC4005XC_DATA_X),
						mxc4005xc_measure_req_raw((mxc4005xc_t*)req, MXC4005XC_DATA_Y),
						mxc4005xc_measure_req_raw((mxc4005xc_t*)req, MXC4005XC_DATA_Z)
						);
					printf("  x=%6.3f y=%6.3f z=%6.3f (calibrated -1 to 1)\r\n",
						mxc4005xc_measure_req((mxc4005xc_t*)req, MXC4005XC_DATA_X),
						mxc4005xc_measure_req((mxc4005xc_t*)req, MXC4005XC_DATA_Y),
						mxc4005xc_measure_req((mxc4005xc_t*)req, MXC4005XC_DATA_Z)
						);
					printf("  xy=%8.3f xz=%8.3f yz=%8.3f (deg)\r\n",
						mxc4005xc_measure_req_plane((mxc4005xc_t*)req, MXC4005XC_PLANE_XY),
						mxc4005xc_measure_req_plane((mxc4005xc_t*)req, MXC4005XC_PLANE_XZ),
						mxc4005xc_measure_req_plane((mxc4005xc_t*)req, MXC4005XC_PLANE_YZ)
						);
				}

				node = node->next;
			}
		}
		else
			printf("usage: i2c read <hex_addr> <target_addr> <num_bytes>\r\n"
				"usage: i2c write <hex_addr> <target_addr> [<byte> <byte> <byte>...]\r\n"
				"usage: i2c adc <hex_addr>\r\n"
				"usage: i2c list - list continuously sampled measurements\r\n"
				"\r\n"
				"Read              # Reads from the i2c device\r\n"
				"  <num_bytes>     # Selects number of bytes to read using a decimal\r\n"
				"Write             # Writes to the i2c device\r\n"
				"  <byte>          # The value in hex to write to the i2c device\r\n"
				"  <byte>          # You may write as many bytes as your device requires\r\n"
				"\r\n"
				"hex_address selects the address of the device to write/read from.\r\n"
				"target_addr selects the register to write/read from.\r\n");
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
				(int)(time(0) - now));
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
			char *temp = NULL;
			if (argc >= 8)
			{
				rtc.tm_year = atoi(args[2]) - 1900;
				rtc.tm_mon = atoi(args[3]) - 1;
				rtc.tm_mday = atoi(args[4]);
				rtc.tm_hour = atoi(args[5]);
				rtc.tm_min = atoi(args[6]);
				rtc.tm_sec = atoi(args[7]);

				if (argc >= 9)
					temp = args[8];
			}
			else if (argc == 3 || argc == 4)
			{
				time_t t = atoll(args[2]);

				// RTC doesn't support earlier than y2k.
				// You can call this a y2k bug ;)
				if (t < 946684800)
					t = 946684800;
				gmtime_r(&t, &rtc);

				if (argc >= 4)
					temp = args[3];
			}
			else
			{
				print("usage: date set (unixtime|YYYY MM DD hh mm ss) [temp]\r\n"
					"  If a single integer is given, then it is seconds since Jan 1, 1970 UTC\r\n"
					"  If 6 integers are passed, then parse as YYYY MM DD hh mm ss\r\n"
					"  If you specify temp as the last argument when setting UNIX time then it\r\n"
					"  will not write to the RTC.\r\n"
					"  Note: clock only supports dates after the year 2000.\r\n"
					);
				return;
			}

			rtcc_set_sec(mktime(&rtc));

			if (temp && match(temp, "temp"))
				printf("Temporary date: not writing to RTC.\r\n");
			else
				ds3231_write_time(&rtc);
		}
		else if (argc >= 2 && match(args[1], "sync"))
		{
			fetch_and_store_time_in_nvs(NULL);
		}
		else
		{
			printf("usage: date (read|set|scale)\r\n"
				"date read             # Read current time from RTC and set.\r\n"
				"date set <time>       # Set current time. Date formats:\r\n"
				"                      #  - unixtime\r\n"
				"                      #  - YYYY MM DD hh mm ss)\r\n"
				"date scale <scale>    # Artifically accelerate time by step_size times normal\r\n"
				);

			return;
		}

		mktime(&rtc);
		print_tm(&rtc);
	}

	else if (match(args[0], "iodump"))
		gpio_dump_io_configuration(stdout, (1 << GPIO_PIN_COUNT)-1);

	else if (match(args[0], "ioread"))
		printf("gpio setting: %d\r\n"
			"gpio setting: %d\r\n"
			"gpio setting: %d\r\n"
			"button number: %d\r\n",
			gpio_get_level(20),
			gpio_get_level(21),
			gpio_get_level(22),
			(gpio_get_level(20) << 0) | (gpio_get_level(21) << 1) | (gpio_get_level(22) << 2)
			);

	// This must be the last else if:
	else if (!match(args[0], ""))
	{
		print("Unkown command: ");
		print(args[0]);
		print("\r\n");
	}
}

int tracking_update()
{
	static int az_rotor_idx = 0;
	static int el_rotor_idx = 1;
	const sat_t *sat = NULL;

	idle_counts++;
	sat = sat_update();

	// Set the rotor target for az/el if a satellite is being tracked.
	if (sat != NULL)
	{
		rotors[az_rotor_idx].target = sat->sat_az;
		rotors[el_rotor_idx].target = sat->sat_el;

		return 1;
	}
	else if (astro_tracked_body != BODY_INVALID)
	{
		astro_observer_t observer;
		astro_time_t time;
		astro_equatorial_t equ_ofdate;
		astro_horizon_t hor;

		time = Astronomy_CurrentTime();

		observer.latitude = Degrees(config.observer.lat);
		observer.longitude = Degrees(config.observer.lon);
		observer.height = config.observer.alt * 1000;	// km to m

		equ_ofdate = Astronomy_Equator(astro_tracked_body, &time, observer, EQUATOR_OF_DATE, ABERRATION);
		if (equ_ofdate.status != ASTRO_SUCCESS)
		{
			printf("%s: Astronomy_Equator returned status %d trying to get coordinates of date.\r\n",
				astro_tracked_name,
				equ_ofdate.status);

			astro_tracked_body = BODY_INVALID;
			astro_tracked_name = NULL;

			return 0;
		}

		hor = Astronomy_Horizon(&time, observer, equ_ofdate.ra,
					equ_ofdate.dec, REFRACTION_NORMAL);

		rotors[az_rotor_idx].target = hor.azimuth;
		rotors[el_rotor_idx].target = hor.altitude;

		return 1;
	}
	else
		return 0;
}

#ifdef __ESP32__
void tracking_update_thread()
{
	TickType_t interval = 10/portTICK_PERIOD_MS;
	TickType_t now = xTaskGetTickCount();
	while (1)
	{
		tracking_update();
		vTaskDelayUntil(&now, interval);
	}
}
#endif

void main_idle()
{
#ifdef __EFR32__
	// EFR32 dosen't support threads, so this is called while waiting at a
	// terminal prompt
	if (tracking_update() != 1)
		platform_sleep();
#else
	platform_sleep();
#endif
}

#ifdef __ESP32__
int app_main()
#else
int main()
#endif
{
	struct rotor *theta = &rotors[0], *phi = &rotors[1], *focus = &rotors[3]; // not a typo
	struct linklist history = {.head = NULL, .tail = NULL};
	char buf[128], *args[MAX_ARGS];
	
	int i, argc;

	// Chip errata
#ifdef __EFR32__
	CHIP_Init();
#endif
#ifdef __ESP32__
	vTaskPrioritySet(NULL, 5);
#endif

	// Initialize efr32 features
	initGpio();
	initUsart0();
	//print("\x0c\r\n");

	// These should be compiler errors:
	// Make sure that the structures are NOT larger then the padding
	if (sizeof(struct motor) > sizeof(((struct motor *)0)->pad))
		printf("Warning: struct motor (%d) is bigger than its pad (%d), increase pad size for flash "
			"backwards compatibility\r\n",
			(int)sizeof(struct motor), (int)sizeof(((struct motor *)0)->pad));

	meminfo();

	// Mount fatfs
#ifdef __ESP32__
	//esp_vfs_fat_register("/", "", 8, &fatfs);
	static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

	const esp_vfs_fat_mount_config_t mount_config = {
		.max_files = 4,
		.format_if_mount_failed = true,
		.allocation_unit_size = CONFIG_WL_SECTOR_SIZE
	};

	esp_err_t err;

	err = esp_vfs_fat_spiflash_mount_rw_wl("/spiflash", "fat", &mount_config, &s_wl_handle);

	if (err != ESP_OK) {
		printf("Failed to mount FATFS (%s)", esp_err_to_name(err));
	}
#else
	FRESULT res;

	res = f_mount(fatfs, "", 0);

	if (res != FR_OK)
		printf("Failed to mount fatfs: %s\r\n", ff_strerror(res));
#endif

	// Load user config
	f_read_file("config.bin", &config, sizeof(config));

#ifdef HAVE_IADC
	initIADC();
#endif

#ifdef HAVE_I2C
	initI2C();

/*	qmc5883l_t *compass = qmc5883l_measure_req_alloc(0x0d);

	// Reset the compass device by putting it in standby first, otherwise
	// it will not accept the new config.
	compass->control_reg_1_mode = QMC5883L_MODE_STANDBY;
	qmc5883l_config_write(compass);

	compass->control_reg_1_mode = QMC5883L_MODE_CONTINUOUS;
	compass->control_reg_1_odr = QMC5883L_DATA_RATE_200HZ;
	compass->control_reg_1_osr = QMC5883L_OVER_SAMPLE_512;

	qmc5883l_config_write(compass);
	//i2c_req_add_cont((i2c_req_t *)compass);
*/
	// Initialize compass
	mmc5603nj_t *mag = mmc5603nj_measure_req_alloc(0x30);
	if (mag == NULL)
	{
		printf("Error: failed to allocate mag\r\n");
	}
	mag->control_reg_odr = 250;
	mag->control_reg_0_auto_sr_en = 1;
	mag->control_reg_0_cmm_freq_en = 1;
	mag->control_reg_1_bw = MMC5603NJ_BW_150HZ;
	mag->control_reg_2_prd_set = MMC5603NJ_PRD_SET_500;
	mag->control_reg_2_en_prd_set = 1;
	mag->control_reg_2_cmm_en = 1;
	mag->invert_z = true;
	mag->invert_y = true;
	mmc5603nj_config_write(mag);
	i2c_req_add_cont((i2c_req_t *)mag);

	// Initialize accelerometer
	mxc4005xc_t *acc = mxc4005xc_measure_req_alloc(0x15);
	acc->invert_z = true;
	i2c_req_add_cont((i2c_req_t *)acc);

	// Initialize realtime clock
	rtcc_init(128);
	struct tm rtc;

	ds3231_read_time(&rtc);
	boot_time = mktime(&rtc);
#endif

	rtcc_set_sec(boot_time);

	// Initialize rotors
	initRotors();

#ifdef __EFR32__
	theta->motor.port = gpioPortB;
	theta->motor.pin1 = 0;
	theta->motor.pin2 = 1;

	phi->motor.port = gpioPortC;
	phi->motor.pin1 = 0;
	phi->motor.pin2 = 1;

	focus->motor.port = gpioPortC;
	focus->motor.pin1 = 2;
	focus->motor.pin2 = 3;
#else
	theta->motor.port = -1;
	phi->motor.port = -1;
	focus->motor.port = -1;

	theta->motor.motor_addr = 0x61;
	theta->motor.motor_type = MOTOR_TYPE_DRV8830;
	phi->motor.motor_addr = 0x60;
	phi->motor.motor_type = MOTOR_TYPE_DRV8830;
#endif

	// Load calibrations from FAT
	rotor_cal_load();

	// Initalize motors that were loaded from flash if they were valid
	for (i = 0; i < NUM_ROTORS; i++)
	{
		if (motor_valid(&rotors[i].motor))
		{
			motor_init(&rotors[i].motor);
		}
	}

	// Initalize systick after reading flash so that it does not change.
	// This must happen after rotors are initalized because systick moves
	// the rotor target.
	if (systick_init(100) != 0)
		print("Failed to set systick to 100 Hz\r\n");

	help();
	print("\r\n");

	status();
	print("\r\n");

#ifdef __ESP32__
	xTaskCreate(tracking_update_thread,
		"tracking_thread",
		32768,
		NULL,
		0,
		NULL);

	gnss_init();
	wifi_init();
	wifi_connect(config.wifi_ssid, config.wifi_pass);
	sntp_init_timer();
#endif
	for (;;)
	{
		char *name = "console";

		if (sat_get())
		{
			name = (char*)sat_get()->tle.sat_name;
		}
		else if (astro_tracked_name != NULL)
			name = astro_tracked_name;

		printf("[%s@%s]# ", config.username, name);

		fflush(stdout);

		input(buf, sizeof(buf)-1, &history, main_idle);
		print("\r\n");
		
		memset(args, 0, sizeof(args));
		argc = parse_args(buf, args, MAX_ARGS);
		dispatch(argc, args, &history);
	}
}
