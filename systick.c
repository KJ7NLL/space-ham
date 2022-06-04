#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

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

		// Try to reach the closest degree angle.  For example, if at 360 
		// and the rotor target is set to 630=360+270, then move to 270 instead.
		float rpos = rotor_pos(rotor);
		float rtarget = rotor->target;
		if (rtarget+360 <= rotor->cal2.deg &&
			fabs(rpos - rtarget) > fabs(rpos - (rtarget+360)))
		{
			rotor->target = rtarget + 360;
		}
		else if (rtarget-360 >= rotor->cal1.deg &&
			fabs(rpos - rtarget) > fabs(rpos - (rtarget-360)))
		{
			rotor->target = rtarget - 360;
		}

		float range = rotor->cal2.deg - rotor->cal1.deg;
		float target = (rotor->target - rotor->cal1.deg) / range;
		float pos = (rpos - rotor->cal1.deg) / range;
		float newspeed = PIDController_Update(&rotor->pid, target, pos);
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
