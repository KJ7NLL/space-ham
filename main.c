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

#include "i2c.h"
#include "i2c/rtc-ds3231.h"

#include "sat.h"

#define LED_PORT gpioPortB
#define LED0_PIN 0
#define LED1_PIN 1

#define MAX_ARGS 10

void dispatch(int argc, char **args, struct linklist *history);

FATFS fatfs;           /* Filesystem object */

struct flash_entry 
	flash_rotors = { .name = "rotors", .ptr = rotors, .len = sizeof(rotors) }, 
	flash_systick = { .name = "systick", .ptr = (void *)&systick_ticks, .len = sizeof(systick_ticks) }, 
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

	GPIO_PinModeSet(gpioPortD, 2, gpioModeInput, 0);
	GPIO_PinModeSet(gpioPortD, 3, gpioModeInput, 0);


	GPIO_PinModeSet(I2C_SDA_PORT, I2C_SDA, gpioModeWiredAndPullUpFilter, 1);
	GPIO_PinModeSet(I2C_SCL_PORT, I2C_SCL, gpioModeWiredAndPullUpFilter, 1);

	// turn on LED0 
	GPIO_PinModeSet(LED_PORT, LED0_PIN, gpioModePushPull, 0);
	GPIO_PinModeSet(LED_PORT, LED1_PIN, gpioModePushPull, 0);
}

void help()
{
	char *h =
		"status [watch]                                        # Display system status\r\n"
		"mv <motor_name> <([+-]deg|n|e|s|w)> [<speed=0-100>]   # Moves antenna\r\n"
		"(help|?|h)                                            # List of commands\r\n"
		"flash (save|load)                                     # Save to flash\r\n"
		"sat (load|track|list|search)                          # Satellite commands\r\n"
		"motor <motor_name> (speed|online|offline|on|off|name|port|pin1|pin2|hz|)\r\n"
		"rotor <rotor_name> (cal)                              # Rotor commands\r\n"
		"cam (on|off)                                          # Turns camera on or off\r\n"
		"led (1|0)                                             # Turns LED1/0 on or off\r\n"
		"hist|history                                          # History of commands\r\n"
		"date                                                  # Set or get the date\r\n"
		"debug-keys                                            # Print chars and hex\r\n";
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

	printf("Ticks: %ld\r\n", (long)systick_get());

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

		printf("%s pos=%.2f deg, target=%.2f deg [%s, port=%c%d/%c%d]: speed=%.1f%%\r\n",
			rotors[i].motor.name,
			rotor_pos(&rotors[i]),
			rotors[i].target,
			!rotor_valid(&rotors[i]) ? "invalid" : 
				(rotor_online(&rotors[i]) ? "online" : "offline"), 
			port,
			rotors[i].motor.pin1, 
			port,
			rotors[i].motor.pin2,
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
	else if (match(args[2], "detail"))
	{
		motor_detail(m);
	}
	else if (match(args[2], "speed") && argc == 4 && !isalpha(args[3][0]))
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
	else if (match(args[2], "speed") && argc >= 5)
	{
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
	else if (match(args[2], "name") && argc >= 4)
	{
		strncpy(m->name, args[3], sizeof(m->name)-1);
	}
	else if (match(args[2], "hz") && argc >= 4)
	{
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
	else if (match(args[2], "port") && argc >= 4)
	{
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
	else if (match(args[2], "pin1") && argc >= 4)
	{
		int pin = atoi(args[3]);
		if (pin < 0 || pin > 7)
		{
			printf("invalid pin: %s\r\n", args[3]);

			return;
		}

		m->pin1 = pin;
		if (motor_valid(m))
			motor_init(m);
	}
	else if (match(args[2], "pin2") && argc >= 4)
	{
		int pin = atoi(args[3]);
		if (pin < 0 || pin > 7)
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

	if (argc < 3)
	{
		print("Usage: pid var <value>\r\n"
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

	if (deg < 0)
	{
		print("Degree angles must start at 0\r\n");

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

void rotor(int argc, char **args)
{
	struct rotor *r;

	if (argc < 3)
	{
		print("Usage: rotor <rotor_name> (cal|detail)\r\n"
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
}

void mv(int argc, char **args)
{
	struct rotor *r;

	float deg, speed;

	if (argc < 3)
	{
		print("mv <motor_name> <([+-]deg|n|e|s|w)> [<speed=0-100>]\r\n"
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
	
	if (argc >= 4)
	{
		speed = atof(args[3]) / 100;
		if (speed < r->motor.duty_cycle_min)
		{
			printf("Rotor speed %.2f is too low the minimum duty cycle of %.2f for this rotor\r\n", 
				speed * 100, r->motor.duty_cycle_min);

			return;
		}
		else if (speed > r->motor.duty_cycle_max)
		{
			printf("Rotor speed %.2f is too low the maximum duty cycle of %.2f for this rotor\r\n", 
				speed * 100, r->motor.duty_cycle_max);

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

void flash(int argc, char **args)
{
	int offset;
	if (argc < 2)
	{
		print("Usage: flash (save|load)\r\n");

		return;
	}

	offset = FLASH_DATA_BASE;

	if (match(args[1], "write") || match(args[1], "save"))
	{
		systick_bypass(1);
		flash_write(flash_table, offset);
		systick_bypass(0);
	}
	else if (match(args[1], "load"))
	{
		systick_bypass(1);
		flash_read(flash_table, offset);
		systick_bypass(0);
	}
	else
	{
		printf("Unkown argument: %s\r\n", args[1]);

		return;
	}
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
		systick_delay_sec(1);
		if (!serial_read_done())
		{
			print("\x0c");
		}
	}
	while (!serial_read_done());
}

void sat_pos(tle_t *tle)
{
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
  select_ephemeris(&tle);
}

void sat(int argc, char **args)
{
	tle_t tle;

	char buf[128], tle_set[139];

	int i, sat_idx = 0;

	if (argc < 2)
	{
		 print("usage: sat (load|detail|search|list|track)\r\n");

		 return;
	}

	if (match(args[1], "load"))
	{
		i = 0;

		print("Paste TLE data and press CTRL+D when done\r\n");
		memset(&tle, 0, sizeof(tle));
		while (serial_read_line(buf, 80))
		{
			if (i == 0)
			{
				strncpy(tle.sat_name, buf, sizeof(tle.sat_name));
				tle.sat_name[sizeof(tle.sat_name)-1] = 0;
			}
			else if (i == 1)
			{
				strncpy(tle_set, buf, 69);
				tle_set[69] = 0;
			}
			else if (i == 2)
			{
				strncpy(tle_set+69, buf, 69);
				tle_set[138] = 0;
				if (Good_Elements(tle_set))
				{
					Convert_Satellite_Data(tle_set, &tle);
					printf("parsed tle: %s\r\n", tle.sat_name);
					sat_detail(&tle);
				}
				else
					printf("invalid tle: %s\r\n", tle.sat_name);
			}

			i++;
			
			if (i == 3)
			{
				i = 0;
			}
		}

		if (i != 0)
		{
			print("ignoring partial satellite data\r\n");
		}
		printf("Loaded %d satellite TLEs\r\n", sat_idx);

	}
}

void fat(int argc, char **args)
{
	FIL fil;            /* File object */
	FRESULT res;        /* API result code */
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
	
	else if (match(args[0], "led") && argc >= 2)
	{
		if (args[1][0] == '0')
			GPIO_PinOutToggle(gpioPortB, 0);

		else if (args[1][0] == '1')
			GPIO_PinOutToggle(gpioPortB, 1);
		else
			print("Invalid pin\r\n");
	}

	else if (match(args[0], "flash"))
	{
		flash(argc, args);
	}

	else if (match(args[0], "fat"))
		fat(argc, args);

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
		char *buf = malloc(n_bytes);

		if (!buf)
		{
			print("buf is null, out of memory?\r\n");
			return;
		}

		buf[0] = 0;
		i2c_master_read(0x68 << 1, 0, buf, n_bytes);
		for (i = 0; i < n_bytes; i++)
			printf("%d. %02X\r\n", i, buf[i]);

		free(buf);
	}
	else if (match(args[0], "date"))
	{
		char buf[12];
		struct tm rtc;

		if (argc >= 2 && match(args[1], "read"))
		{
			ds3231_read_time(&rtc);
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
				time_t t = atoi(args[2]);

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

			ds3231_write_time(&rtc);
		}
		else
		{
			printf("usage: date (read|set)\r\n"
				"date read - show current time\r\n"
				"date set  - set current time\r\n"
				"date set (unixtime|YYYY MM DD hh mm ss)\r\n");

			return;
		}

		printf("UTC: %02d/%02d/%04d  %02d:%02d:%02d  unix time: %ld\r\n",
			rtc.tm_mon+1, rtc.tm_mday, rtc.tm_year + 1900,
			rtc.tm_hour, rtc.tm_min, rtc.tm_sec,
			(unsigned long)mktime(&rtc));

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

	res = f_mount(&fatfs, "", 0);
	if (res != FR_OK)
		printf("Failed to mount fatfs: %s\r\n", ff_strerror(res));

	status();
	print("\r\n");

	// These should be compiler errors:
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
