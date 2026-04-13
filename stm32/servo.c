#include "device_driver.h"
#include "servo.h"

void Servo_Construct(Servo *s)
{
	s->min_pulse_us = 1000;
	s->max_pulse_us = 2000;
	s->min_angle_deg = 0;
	s->max_angle_deg = 180;
}

void Servo_HwInit(void)
{
	Servo_PWM_Init();
}

void Servo_Init(Servo *s)
{
	Servo_Construct(s);
	Servo_HwInit();
}

void Servo_SetPulseUs(Servo *s, unsigned int us)
{
	if (us < s->min_pulse_us)
		us = s->min_pulse_us;
	if (us > s->max_pulse_us)
		us = s->max_pulse_us;
	Servo_PWM_SetPulseUs(us);
}

void Servo_SetAngle(Servo *s, int angle_deg)
{
	int lo = s->min_angle_deg;
	int hi = s->max_angle_deg;
	unsigned int pmin = s->min_pulse_us;
	unsigned int pmax = s->max_pulse_us;
	unsigned int us;
	int span;

	if (angle_deg < lo)
		angle_deg = lo;
	if (angle_deg > hi)
		angle_deg = hi;

	span = hi - lo;
	if (span <= 0) {
		Servo_SetPulseUs(s, pmin);
		return;
	}

	us = pmin + (unsigned int)(((unsigned int)(angle_deg - lo) * (pmax - pmin)
		+ (unsigned int)span / 2u) / (unsigned int)span);
	Servo_SetPulseUs(s, us);
}
