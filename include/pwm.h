#define PWM_FREQ         1047 // C5 note
#define DUTY_CYCLE_STEPS  0.3

void initTimer(TIMER_TypeDef *timer, int cc, int port, int pin);
void motor(int n, int speed);
void timer_enable(TIMER_TypeDef *timer);
void timer_disable(TIMER_TypeDef *timer);
int timer_cmu_clock(TIMER_TypeDef *timer);
int timer_route_idx(TIMER_TypeDef *timer);
void timer_cc_route(TIMER_TypeDef *timer, int cc, int port, int pin);
