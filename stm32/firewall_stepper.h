#ifndef FIREWALL_STEPPER_H
#define FIREWALL_STEPPER_H

#define FIREWALL_TRAVEL_STEPS	4500

void FirewallStepper_Init(void);
void FirewallStepper_SetStepDelayUs(unsigned int us);
void FirewallStepper_SetWhileStepPoll(void (*fn)(void));
void FirewallStepper_OpenBoth(void);
void FirewallStepper_CloseBoth(void);
void FirewallStepper_EmergencyCloseAll(void);
void FirewallStepper_Release(void);
int FirewallStepper_Fw1IsOpen(void);
int FirewallStepper_Fw2IsOpen(void);

#endif
