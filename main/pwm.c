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
#include "platform.h"

#include "pwm.h"
#include "iadc.h"

#ifdef __EFR32__
TIMER_TypeDef *TIMERS[4] = {TIMER0, TIMER1, TIMER2, TIMER3};
#else
TIMER_TypeDef *TIMERS[4];
#endif

#ifdef __EFR32__
static uint32_t top_values[4] = {0, 0, 0, 0};
static volatile float duty_cycles[4] = {-1, -1, -1, -1};

void TIMER0_IRQHandler(void)
{
	// Acknowledge the interrupt
	uint32_t flags = TIMER_IntGet(TIMER0);

	TIMER_IntClear(TIMER0, flags);
}

void TIMER1_IRQHandler(void)
{
	// Acknowledge the interrupt
	uint32_t flags = TIMER_IntGet(TIMER1);

	TIMER_IntClear(TIMER1, flags);
}

void TIMER2_IRQHandler(void)
{
	// Acknowledge the interrupt
	uint32_t flags = TIMER_IntGet(TIMER2);

	TIMER_IntClear(TIMER2, flags);
}

void TIMER3_IRQHandler(void)
{
	// Acknowledge the interrupt
	uint32_t flags = TIMER_IntGet(TIMER3);

	TIMER_IntClear(TIMER3, flags);
}

void timer_cc_duty_cycle(TIMER_TypeDef *timer, int cc, float duty_cycle)
{
	int idx = timer_idx(timer);

	if (duty_cycles[idx] != duty_cycle)
	{
		TIMER_CompareBufSet(timer, cc, (uint32_t) (top_values[idx] * duty_cycle));
		duty_cycles[idx] = duty_cycle;
	}
}

void timer_irq_enable(TIMER_TypeDef *timer)
{
	if (timer == TIMER0)
		NVIC_EnableIRQ(TIMER0_IRQn);
	if (timer == TIMER1)
		NVIC_EnableIRQ(TIMER1_IRQn);
	if (timer == TIMER2)
		NVIC_EnableIRQ(TIMER2_IRQn);
	if (timer == TIMER3)
		NVIC_EnableIRQ(TIMER3_IRQn);
}

void timer_irq_disable(TIMER_TypeDef *timer)
{
	if (timer == TIMER0)
		NVIC_DisableIRQ(TIMER0_IRQn);
	if (timer == TIMER1)
		NVIC_DisableIRQ(TIMER1_IRQn);
	if (timer == TIMER2)
		NVIC_DisableIRQ(TIMER2_IRQn);
	if (timer == TIMER3)
		NVIC_DisableIRQ(TIMER3_IRQn);
}

int timer_cmu_clock(TIMER_TypeDef *timer)
{
	if (timer == TIMER0)
		return cmuClock_TIMER0;
	if (timer == TIMER1)
		return cmuClock_TIMER1;
	if (timer == TIMER2)
		return cmuClock_TIMER2;
	if (timer == TIMER3)
		return cmuClock_TIMER3;

	return cmuClock_TIMER0;
}

int timer_idx(TIMER_TypeDef *timer)
{
	if (timer == TIMER0)
		return 0;
	if (timer == TIMER1)
		return 1;
	if (timer == TIMER2)
		return 2;
	if (timer == TIMER3)
		return 3;

	return 0;
}

void timer_cc_route_clear(TIMER_TypeDef *timer, int cc)
{
	switch (cc)
	{
		case 0:
			GPIO->TIMERROUTE[timer_idx(timer)].ROUTEEN = 0;
			GPIO->TIMERROUTE[timer_idx(timer)].CC0ROUTE = 0;
			TIMER_IntDisable(timer, TIMER_IEN_CC0);
			break;
		case 1:
			GPIO->TIMERROUTE[timer_idx(timer)].ROUTEEN = 0;
			GPIO->TIMERROUTE[timer_idx(timer)].CC1ROUTE = 0;
			TIMER_IntDisable(timer, TIMER_IEN_CC1);
			break;
		case 2:
			GPIO->TIMERROUTE[timer_idx(timer)].ROUTEEN = 0;
			GPIO->TIMERROUTE[timer_idx(timer)].CC2ROUTE = 0;
			TIMER_IntDisable(timer, TIMER_IEN_CC2);
			break;
	}
}

void timer_cc_route(TIMER_TypeDef *timer, int cc, int port, int pin)
{
	switch (cc)
	{
		case 0:
			GPIO->TIMERROUTE[timer_idx(timer)].ROUTEEN = GPIO_TIMER_ROUTEEN_CC0PEN;
			GPIO->TIMERROUTE[timer_idx(timer)].CC0ROUTE = (port << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT) | 
				(pin << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);
			TIMER_IntEnable(timer, TIMER_IEN_CC0);
			break;
		case 1:
			GPIO->TIMERROUTE[timer_idx(timer)].ROUTEEN = GPIO_TIMER_ROUTEEN_CC1PEN;
			GPIO->TIMERROUTE[timer_idx(timer)].CC1ROUTE = (port << _GPIO_TIMER_CC1ROUTE_PORT_SHIFT) | 
				(pin << _GPIO_TIMER_CC1ROUTE_PIN_SHIFT);
			TIMER_IntEnable(timer, TIMER_IEN_CC1);
			break;
		case 2:
			GPIO->TIMERROUTE[timer_idx(timer)].ROUTEEN = GPIO_TIMER_ROUTEEN_CC2PEN;
			GPIO->TIMERROUTE[timer_idx(timer)].CC2ROUTE = (port << _GPIO_TIMER_CC2ROUTE_PORT_SHIFT) | 
				(pin << _GPIO_TIMER_CC2ROUTE_PIN_SHIFT);
			TIMER_IntEnable(timer, TIMER_IEN_CC2);
			break;
	}
}

void timer_enable(TIMER_TypeDef *timer)
{
	TIMER_Enable(timer, true);
}

void timer_disable(TIMER_TypeDef *timer)
{
	TIMER_Enable(timer, false);
}
#endif

void timer_init_pwm(TIMER_TypeDef *timer, int cc, int port, int pin, int pwm_Hz, float duty_cycle)
{
#ifdef __EFR32__
	uint32_t timerFreq = 0;

	int idx;

	// Initialize the timer
	TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;

	// Configure timer Compare/Capture for output compare
	TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;

	CMU_ClockEnable(timer_cmu_clock(timer), true);
	
	idx = timer_idx(timer);

	// Use PWM mode, which sets output on overflow and clears on compare
	// events
	timerInit.prescale = timerPrescale64;
	timerInit.enable = false;
	timerCCInit.mode = timerCCModePWM;

	// Configure but do not start the timer
	TIMER_Init(timer, &timerInit);

	// Route Timer0 CC0 output to PA6
	timer_cc_route(timer, cc, port, pin);

	// Configure CC Channel 0
	TIMER_InitCC(timer, cc, &timerCCInit);
	
	// Get PWM period
	timerFreq = CMU_ClockFreqGet(timer_cmu_clock(timer)) / (timerInit.prescale + 1);
	
	// Set top value to overflow at the desired PWM_FREQ frequency
	top_values[idx] = (timerFreq / pwm_Hz);
	TIMER_TopSet(timer, top_values[idx]);

	// Set compare value for initial duty cycle
	// top_values[idx] must be set before calling this function
	timer_cc_duty_cycle(timer, cc, duty_cycle);

	// Start the timer
	TIMER_Enable(timer, true);
#endif
}
