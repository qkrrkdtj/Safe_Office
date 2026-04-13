#ifndef STEPMOTOR_H
#define STEPMOTOR_H

#define STEPMOTOR_DOOR_STEPS	4500

void StepMotor_Init(void);
void StepMotor_SetWhileStepPoll(void (*fn)(void));
void StepMotor_SetStepDelayMs(unsigned int ms);
void StepMotor_SetStepDelayUs(unsigned int us);
void StepMotor_Step(int steps);
void StepMotor_Release(void);
void StepMotor_DoorOpen(void);
void StepMotor_DoorOpenForce(void);
void StepMotor_DoorClose(void);
void StepMotor_EmergencyOpen(void);
int StepMotor_DoorIsOpen(void);
void StepMotor_Test_Run(void);

#endif
