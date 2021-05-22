#define PWM_FREQ         1047 // C5 note
#define DUTY_CYCLE_STEPS  0.3

int timer_idx(TIMER_TypeDef *timer);

void timer_init_pwm(TIMER_TypeDef *timer, int cc, int port, int pin, float duty_cycle);

void timer_enable(TIMER_TypeDef *timer);
void timer_disable(TIMER_TypeDef *timer);

void timer_irq_enable(TIMER_TypeDef *timer);
void timer_irq_disable(TIMER_TypeDef *timer);

int timer_cmu_clock(TIMER_TypeDef *timer);

void timer_cc_route(TIMER_TypeDef *timer, int cc, int port, int pin);
void timer_cc_route_clear(TIMER_TypeDef *timer, int cc);
void timer_cc_duty_cycle(TIMER_TypeDef *timer, int cc, float duty_cycle);
