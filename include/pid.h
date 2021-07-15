#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

typedef struct
{
	// Controller gains 
	float Kp;
	float Ki;
	float Kd;

	
	// Derivative low-pass filter time constant 
	float tau;

	// Output limits 
	float out_min;
	float out_max;

	// Integrator limits
	float int_min;
	float int_max;

	// Sample time (in seconds) 
	float T;

	// Controller "memory" 
	float proportional;
	float integrator;
	float prevError;	/* Required for integrator */

	float differentiator;
	float prevMeasurement;	/* Required for differentiator */

	// Controller output 
	float out;

} PIDController;

void PIDController_Init(PIDController *pid);
float PIDController_Update(PIDController *pid, float setpoint, float measurement);

#endif
