#define PWM_FREQ          30000
#define DUTY_CYCLE_STEPS  0.3

void initTimer(TIMER_TypeDef *timer, int cc, int port, int pin);
void motor(int n, int speed);
