#include "em_device.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_chip.h"
#include "em_gpio.h"
#include "em_timer.h"

#include "pwm.h"
#include "iadc.h"

static uint32_t top_value = 0;
static volatile float duty_cycle = 0.3;

void TIMER0_IRQHandler(void)
{
	// Acknowledge the interrupt
	uint32_t flags = TIMER_IntGet(TIMER0);

	TIMER_IntClear(TIMER0, flags);

	// Update CCVB to alter duty cycle starting next period
	duty_cycle = iadc_get_result(0) / 3.3;
	if (duty_cycle > 0.96)
		duty_cycle = 1.0;

	if (duty_cycle < 0.04)
		duty_cycle = 0;


	TIMER_CompareBufSet(TIMER0, 0, (uint32_t) (top_value * duty_cycle));
}

void timer_enable(TIMER_TypeDef *timer)
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

void timer_disable(TIMER_TypeDef *timer)
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

int timer_route_idx(TIMER_TypeDef *timer)
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

void timer_cc_route(TIMER_TypeDef *timer, int cc, int port, int pin)
{
	switch (cc)
	{
		case 0:
			GPIO->TIMERROUTE[timer_route_idx(timer)].ROUTEEN = GPIO_TIMER_ROUTEEN_CC0PEN;
			GPIO->TIMERROUTE[timer_route_idx(timer)].CC0ROUTE = (port << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT) | 
				(pin << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);
			TIMER_IntEnable(timer, TIMER_IEN_CC0);
			break;
		case 1:
			GPIO->TIMERROUTE[timer_route_idx(timer)].ROUTEEN = GPIO_TIMER_ROUTEEN_CC1PEN;
			GPIO->TIMERROUTE[timer_route_idx(timer)].CC1ROUTE = (port << _GPIO_TIMER_CC1ROUTE_PORT_SHIFT) | 
				(pin << _GPIO_TIMER_CC1ROUTE_PIN_SHIFT);
			TIMER_IntEnable(timer, TIMER_IEN_CC1);
			break;
		case 2:
			GPIO->TIMERROUTE[timer_route_idx(timer)].ROUTEEN = GPIO_TIMER_ROUTEEN_CC2PEN;
			GPIO->TIMERROUTE[timer_route_idx(timer)].CC2ROUTE = (port << _GPIO_TIMER_CC2ROUTE_PORT_SHIFT) | 
				(pin << _GPIO_TIMER_CC2ROUTE_PIN_SHIFT);
			TIMER_IntEnable(timer, TIMER_IEN_CC2);
			break;
	}
}
	
void initTimer(TIMER_TypeDef *timer, int cc, int port, int pin)
{
	uint32_t timerFreq = 0;

	// Initialize the timer
	TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;

	// Configure timer Compare/Capture for output compare
	TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;

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

	// Start with 10% duty cycle
	duty_cycle = DUTY_CYCLE_STEPS;

	// set PWM period
	timerFreq = CMU_ClockFreqGet(timer_cmu_clock(timer)) / (timerInit.prescale + 1);
	top_value = (timerFreq / PWM_FREQ);
	// Set top value to overflow at the desired PWM_FREQ frequency
	TIMER_TopSet(timer, top_value);

	// Set compare value for initial duty cycle
	TIMER_CompareSet(timer, cc, (uint32_t) (top_value * duty_cycle));

	// Start the timer
	TIMER_Enable(timer, true);

	// Enable timer compare event interrupts to update the duty cycle
			TIMER_IntEnable(TIMER0, TIMER_IEN_CC0);
	timer_enable(timer);
}
