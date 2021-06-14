#include "em_cmu.h"
#include "em_emu.h"

volatile static int lock = 0, locked_ticks = 0, ticks_per_sec;
volatile static uint64_t ticks = 0;

void SysTick_Handler(void)
{
	if (!lock)
	{
		ticks++;
		if (locked_ticks != 0)
		{
			ticks += locked_ticks;
			locked_ticks = 0;
		}
	}
	else
		locked_ticks++;
}

int systick_update()
{
	return SysTick_Config(cmuClock_CORE / ticks_per_sec);
}

int systick_init(int tps)
{
	ticks_per_sec = tps;
	return systick_update();
}

void systick_set(uint64_t newticks)
{
	lock = 1;
	ticks = newticks;
	locked_ticks = 0;
	lock = 0;
}

uint64_t systick_get()
{
	uint64_t t;
	lock = 1;
	t = ticks;
	lock = 0;
	
	return t;
}

void systick_delay(uint64_t delay)
{
	uint64_t cur = systick_get();

	while ((systick_get() - cur) < delay)
		EMU_EnterEM1();

}

