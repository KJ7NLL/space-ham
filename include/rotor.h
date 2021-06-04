#define NUM_ROTORS 4

struct motor
{
	union {
		struct {
			char name[20];

			TIMER_TypeDef *timer;
			int port;
			int pin1;
			int pin2;

			int pwm_Hz;

			// The port, pins, and PWM have been assigned. If a motor
			// is used before it is ready, it will crash the MCU.
			int online;

			// Set this duty cycle upon initialization
			float duty_cycle_at_init;

			// speeds below min_duty_cycle stop the motor
			float duty_cycle_min;

			// speeds above max_duty_cycle run the motor at full speed
			float duty_cycle_max;

			// Temporarily limit the duty cycle below the variable speed. 
			// If this above duty_cycle_max, then no limit.
			// Warning: setting this below duty_cycle_min will stop the motor
			//          next time motor_speed is called. 
			float duty_cycle_limit;

			// -1 to 1
			float speed;
		};

		char pad[80];
	};
};

struct rotor_cal
{
	float v, deg;
	int ready;
};

struct rotor
{
	union {
		struct {
			struct motor motor;
			struct rotor_cal cal1, cal2;
			
			int iadc;

			float target;
		};

		char pad[80];
	};
};

extern struct rotor rotors[NUM_ROTORS];

// These motors will reference the motor in each rotor.
extern struct motor *motors[NUM_ROTORS];

void initRotors();

void motor_init(struct motor *m);
void motor_speed(struct motor *m, float speed);

int motor_valid(struct motor *m);
int motor_online(struct motor *m);
int rotor_valid(struct rotor *r);
int rotor_online(struct rotor *r);

struct motor *motor_get(char *name);
struct rotor *rotor_get(char *name);
