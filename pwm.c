#include "em_device.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_chip.h"
#include "em_gpio.h"
#include "em_timer.h"

#include "pwm.h"

static uint32_t top_value = 0;
static volatile float duty_cycle = 0;

void TIMER0_IRQHandler(void)
{
	// Acknowledge the interrupt
	uint32_t flags = TIMER_IntGet(TIMER0);

	TIMER_IntClear(TIMER0, flags);

	// Update CCVB to alter duty cycle starting next period
	TIMER_CompareBufSet(TIMER0, 0, (uint32_t) (top_value * duty_cycle));
}

void initTimer(void)
{
	uint32_t timerFreq = 0;

	// Initialize the timer
	TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;

	// Configure TIMER0 Compare/Capture for output compare
	TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;

	// Use PWM mode, which sets output on overflow and clears on compare
	// events
	timerInit.prescale = timerPrescale64;
	timerInit.enable = false;
	timerCCInit.mode = timerCCModePWM;

	// Configure but do not start the timer
	TIMER_Init(TIMER0, &timerInit);

	// Route Timer0 CC0 output to PA6
	GPIO->TIMERROUTE[0].ROUTEEN = GPIO_TIMER_ROUTEEN_CC0PEN;
	GPIO->TIMERROUTE[0].CC0ROUTE = (gpioPortA << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT) | 
	(6 << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);

	// Configure CC Channel 0
	TIMER_InitCC(TIMER0, 0, &timerCCInit);

	// Start with 10% duty cycle
	duty_cycle = DUTY_CYCLE_STEPS;

	// set PWM period
	timerFreq =
		CMU_ClockFreqGet(cmuClock_TIMER0) / (timerInit.prescale + 1);
	top_value = (timerFreq / PWM_FREQ);
	// Set top value to overflow at the desired PWM_FREQ frequency
	TIMER_TopSet(TIMER0, top_value);

	// Set compare value for initial duty cycle
	TIMER_CompareSet(TIMER0, 0, (uint32_t) (top_value * duty_cycle));

	// Start the timer
	TIMER_Enable(TIMER0, true);

	// Enable TIMER0 compare event interrupts to update the duty cycle
	TIMER_IntEnable(TIMER0, TIMER_IEN_CC0);
	NVIC_EnableIRQ(TIMER0_IRQn);
}
