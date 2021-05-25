struct motor
{
	TIMER_TypeDef *timer;
	int port;
	int pin1;
	int pin2;

	char *name;

	// -1 to 1
	float speed;

	// speeds below min_duty_cycle stop the motor
	// speeds above max_duty_cycle run the motor at full speed
	float initial_duty_cycle, min_duty_cycle, max_duty_cycle, duty_cycle_limit;
};

struct rotor_cal
{
	float v, deg;
	int ready;
};

struct rotor
{
	struct motor motor;
	struct rotor_cal cal1, cal2;
	
	int iadc;

	float target;


};

extern struct rotor phi, theta;

void motor_init(struct motor *m);
void motor_speed(struct motor *m, float speed);
