#include "em_cmu.h"
#include "em_emu.h"

#include "rotor.h"

volatile uint64_t systick_ticks = 0;
static volatile int lock = 0, locked_ticks = 0, ticks_per_sec = 1000;

void SysTick_Handler(void)
{
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

/*
	for (i = 0; i < NUM_ROTORS; i++)
	{
		if (rotor_pos(&rotors[i]) < rotors[i].target)
			motor_speed(&rotors[i].motor, rotors[i].motor.speed * 1.1);
		else if (rotor_pos(&rotors[i]) > rotors[i].target)
			motor_speed(&rotors[i].motor, rotors[i].motor.speed * 0.9);
	}
*/
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

void systick_delay(uint64_t delay)
{
	uint64_t cur = systick_get();

	while ((systick_get() - cur) < delay)
		EMU_EnterEM1();

}

