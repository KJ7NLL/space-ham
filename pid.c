#include "pid.h"

void PIDController_Init(PIDController *pid)
{
	pid->proportional = 0.0f;
	pid->integrator = 0.0f;
	pid->differentiator = 0.0f;

	pid->prevError = 0.0f;
	pid->prevMeasurement = 0.0f;

	pid->out = 0.0f;
}

float PIDController_Update(PIDController *pid, float setpoint, float measurement)
{

	float error = setpoint - measurement;
	pid->proportional = pid->Kp * error;

	pid->integrator =
		pid->integrator + 0.5f * pid->Ki * pid->T * (error +
							     pid->prevError);

	if (pid->integrator > pid->int_max)
		pid->integrator = pid->int_max;
	else if (pid->integrator < pid->int_min)
		pid->integrator = pid->int_min;

	// Derivative (band-limited differentiator)
	// Note: derivative on measurement, therefore minus sign in front of equation!
	pid->differentiator = -(2.0f * pid->Kd * (measurement - pid->prevMeasurement) 
					+ (2.0f * pid->tau - pid->T) * pid->differentiator)
				/ (2.0f * pid->tau + pid->T);

	// Compute output and apply limits
	pid->out = pid->proportional + pid->integrator + pid->differentiator;


	if (pid->out > pid->out_max)
		pid->out = pid->out_max;
	else if (pid->out < pid->out_min)
		pid->out = pid->out_min;


	// Store error and measurement for later use 
	pid->prevError = error;
	pid->prevMeasurement = measurement;

	// Return controller output 
	return pid->out;
}
