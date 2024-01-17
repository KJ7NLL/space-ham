/*
 * MIT License
 *
 * Copyright (c) 2020 Philip Salmony
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <math.h>

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

	if (isnan(pid->proportional))
		pid->proportional = 0;

	if (isnan(pid->integrator))
		pid->integrator = 0;

	if (isnan(pid->differentiator))
		pid->differentiator = 0;

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
