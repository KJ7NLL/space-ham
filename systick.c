#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "em_cmu.h"
#include "em_emu.h"

#include "rotor.h"

static volatile int _systick_bypass = 0;
static volatile int ticks_per_sec = 1000;

void SysTick_Handler(void)
{
	struct motor *motor;
	struct rotor *rotor;
	int i;

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
