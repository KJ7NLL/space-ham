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
//
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

		// Safety: keep the target in range of cal1.deg and cal2.deg:
		if (rotor->target < rotor_cal_min(rotor)->deg)
			rotor->target = rotor_cal_min(rotor)->deg;
		else if (rotor->target > rotor_cal_max(rotor)->deg)
			rotor->target = rotor_cal_max(rotor)->deg;

		float rpos = rotor_pos(rotor);
		float rtarget = rotor->target;

		// Return to relative positioning when we are within 90 degrees
		// so it will not backtrack
		if (fabs(rtarget - rpos) < 90)
			rotor->target_absolute = 0;

		if (!rotor->target_absolute)
		{
			// Try to reach the closest degree angle.  For example, if at 360
			// and the rotor target is set to 630=360+270, then move to 270 instead.

			if (rtarget+360 <= rotor_cal_max(rotor)->deg &&
				fabs(rpos - rtarget) > fabs(rpos - (rtarget+360)))
			{
				rotor->target = rtarget + 360;
			}
			else if (rtarget-360 >= rotor_cal_min(rotor)->deg &&
				 fabs(rpos - rtarget) > fabs(rpos - (rtarget-360)))
			{
				rotor->target = rtarget - 360;
			}
		}

		float newspeed = rotor_pid_update(rotor, rotor->target, rpos);

		// The sign of `newspeed` must be the same, so save the sign:
		float sign;
		if (newspeed < 0)
			sign = -1;
		else
			sign = 1;

		// If we are within 0.1 degrees of the target, stop moving:
		// This could be an option, but disabled for now.
		//if (fabs(rtarget - rpos) < 0.1)
		//	newspeed = 0;

		// use fabs here for any math to make sure we don't create a bug.
		// fabs() will always return positive, and *sign will set it +/-.
		newspeed = pow(fabs(newspeed), rotor->speed_exp) * sign;

		// Limit the speed increse to stay under a maximum ramping time.
		// Only ramp in the increasing direction, not while slowing down.
		if (rotor->ramp_time > 0 && fabs(newspeed) > fabs(motor->speed))
		{
			float diff_speed = newspeed - motor->speed;

			float max_delta_speed = 1.0 / (rotor->ramp_time * (float)ticks_per_sec);
			if (fabs(diff_speed) > max_delta_speed)
			{
				if (diff_speed > 0)
					newspeed = motor->speed + max_delta_speed;
				else
					newspeed = motor->speed - max_delta_speed;
			}
		}

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
