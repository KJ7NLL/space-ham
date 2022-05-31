#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "em_cmu.h"
#include "em_emu.h"

#include "rotor.h"

volatile uint64_t systick_ticks = 0;
static volatile int _systick_bypass = 0;
static volatile int lock = 0, locked_ticks = 0, ticks_per_sec = 1000;

void SysTick_Handler(void)
{
	struct motor *motor;
	struct rotor *rotor;
	int i;

	if (!lock)
	{
		systick_ticks++;
		if (locked_ticks != 0)
		{
			systick_ticks += locked_ticks;
			locked_ticks = 0;
		}
	}
	else
		locked_ticks++;

	// Bypass non-systick code.  Really this should be moved to an RTC IRQ
	// and let systick be turned off completely.
	if (_systick_bypass)
		return;

	for (i = 0; i < NUM_ROTORS; i++)
	{
		rotor = &rotors[i];
		motor = &rotor->motor;
		if (!rotor_online(rotor))
			continue;

		float newspeed = PIDController_Update(&rotor->pid, rotor->target, rotor_pos(rotor));
		motor_speed(motor, newspeed);

/*
		if (rotor_pos(rotor) < rotor->target)
			motor_speed(motor, motor->speed + 0.01);
		else if (rotor_pos(rotor) > rotor->target)
			motor_speed(motor, motor->speed - 0.01);
		else
			motor_speed(motor, 0);
*/
	}
}

int systick_update()
{
	return SysTick_Config(CMU_ClockFreqGet(cmuClock_CORE) / ticks_per_sec);
}

int systick_init(int tps)
{
	ticks_per_sec = tps;
	return systick_update();
}

void systick_bypass(int b)
{
	_systick_bypass = b;
}

void systick_set(uint64_t newticks)
{
	lock = 1;
	systick_ticks = newticks;
	locked_ticks = 0;
	lock = 0;
}

uint64_t systick_get()
{
	uint64_t t;
	lock = 1;
	t = systick_ticks;
	lock = 0;
	
	return t;
}

uint64_t systick_get_sec()
{
	return systick_get() / ticks_per_sec;
}

void systick_set_sec(time_t sec)
{
	systick_set(sec*(uint64_t)ticks_per_sec);
}

void systick_delay_ticks(uint64_t delay)
{
	uint64_t cur = systick_get();

	while ((systick_get() - cur) < delay)
		EMU_EnterEM1();

}

void systick_delay_sec(float sec)
{
	systick_delay_ticks(sec * (float)ticks_per_sec);
}

// return the number of seeconds elapsed given a starting tick count
float systick_elapsed_sec(uint64_t start)
{
	return (systick_get() - start) / (float)ticks_per_sec;
}

int _gettimeofday(struct timeval *tv, void *tz)
{
	uint64_t now = systick_get(); 

	if (tv == NULL)
		return -1;

	time_t sec = now / (uint64_t)ticks_per_sec;
	long usec = now - (sec * (uint64_t)ticks_per_sec);
	usec *= 1000000 / (uint64_t)ticks_per_sec;

	tv->tv_sec = sec;
	tv->tv_usec = usec;

	printf("s=%lu usec=%lu\n", (long)sec, (long)usec);

	return 0;
}

/*  Not sure if this works, but available if it is useful someday:
int _times(struct tms *t)
{
	clock_t ticks = systick_get() * 1000 / ticks_per_sec; 

	t->tms_utime = ticks;
	t->tms_stime = ticks;
	t->tms_cutime = ticks;
	t->tms_cstime = ticks;

	return ticks;
}
*/

time_t _time(time_t *t)
{
	if (t != NULL)
	{
		*t = systick_get_sec();
		return *t;
	}
	else
		return systick_get_sec();
}
