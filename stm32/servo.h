#ifndef SERVO_H
#define SERVO_H

typedef struct Servo {
	unsigned int min_pulse_us;
	unsigned int max_pulse_us;
	int min_angle_deg;
	int max_angle_deg;
} Servo;

void Servo_Construct(Servo *s);
void Servo_HwInit(void);
void Servo_Init(Servo *s);
void Servo_SetAngle(Servo *s, int angle_deg);
void Servo_SetPulseUs(Servo *s, unsigned int pulse_us);

#endif
