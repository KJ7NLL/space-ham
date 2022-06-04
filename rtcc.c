#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "em_cmu.h"
#include "em_emu.h"
#include "em_rtcc.h"

volatile uint64_t rtcc_ticks = 0;
static volatile int _rtcc_bypass = 0;
static volatile int lock = 0, locked_ticks = 0, ticks_per_sec = 1024;

// Increasing the clock scale will artifically speed up time, including
// all timer and elapsed time calculations:
static int rtcc_clock_scale = 1;

void RTCC_IRQHandler(void)
{
	if (!lock)
	{
		rtcc_ticks += rtcc_clock_scale;
		if (locked_ticks != 0)
		{
			rtcc_ticks += locked_ticks;
			locked_ticks = 0;
		}
	}
	else
		locked_ticks++;

	RTCC_IntClear(RTCC_IF_CC1);
}

void rtcc_init(int tps)
{
	ticks_per_sec = tps;

	RTCC_Init_TypeDef rtccInit = RTCC_INIT_DEFAULT;
	RTCC_CCChConf_TypeDef rtccInitCompareChannel = RTCC_CH_INIT_COMPARE_DEFAULT;

	// Initialize LFXO with specific parameters
	CMU_LFXOInit_TypeDef lfxoInit = CMU_LFXOINIT_DEFAULT;

	CMU_LFXOInit(&lfxoInit);

	// Setting RTCC clock source
	CMU_ClockSelectSet(cmuClock_RTCCCLK, cmuSelect_LFXO);

	// Initialize CC1 to toggle PRS output on compare match
	rtccInitCompareChannel.compMatchOutAction = rtccCompMatchOutActionToggle;
	RTCC_ChannelInit(1, &rtccInitCompareChannel);

	// Setting the CC1 compare value of the RTCC
	RTCC_ChannelCCVSet(1, 32768/ticks_per_sec - 1);

	// Initialize the RTCC
	rtccInit.cntWrapOnCCV1 = true;	// Clear counter on CC1 compare match
	rtccInit.presc = rtccCntPresc_1;	// Prescaler 1
	RTCC_Init(&rtccInit);

	// Enabling Interrupt from RTCC CC1
	RTCC_IntEnable(RTCC_IEN_CC1);
	NVIC_ClearPendingIRQ(RTCC_IRQn);
	NVIC_EnableIRQ(RTCC_IRQn);
}

void rtcc_bypass(int b)
{
	_rtcc_bypass = b;
}

void rtcc_set(uint64_t newticks)
{
	lock = 1;
	rtcc_ticks = newticks;
	locked_ticks = 0;
	lock = 0;
}

uint64_t rtcc_get()
{
	uint64_t t;
	lock = 1;
	t = rtcc_ticks;
	lock = 0;
	
	return t;
}

uint64_t rtcc_get_sec()
{
	return rtcc_get() / ticks_per_sec;
}

void rtcc_set_sec(time_t sec)
{
	rtcc_set(sec*(uint64_t)ticks_per_sec);
}

void rtcc_delay_ticks(uint64_t delay)
{
	uint64_t cur = rtcc_get();

	while ((rtcc_get() - cur) < delay)
		EMU_EnterEM1();

}

void rtcc_delay_sec(float sec)
{
	rtcc_delay_ticks(sec * (float)ticks_per_sec);
}

// return the number of seeconds elapsed given a starting tick count
float rtcc_elapsed_sec(uint64_t start)
{
	return (rtcc_get() - start) / (float)ticks_per_sec;
}

int _gettimeofday(struct timeval *tv, void *tz)
{
	uint64_t now = rtcc_get(); 

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
	clock_t ticks = rtcc_get() * 1000 / ticks_per_sec; 

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
		*t = rtcc_get_sec();
		return *t;
	}
	else
		return rtcc_get_sec();
}

void rtcc_set_clock_scale(int scale)
{
	rtcc_clock_scale = scale;
}
