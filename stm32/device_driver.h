#include "stm32f4xx.h"
#include "option.h"
#include "macro.h"
#include "malloc.h"

// Uart.c

extern void Uart2_Init(int baud);
extern void Uart2_Send_Byte(char data);
extern char Uart2_Get_Pressed(void);
extern char Uart2_Get_Char(void);

extern void Uart1_Init(int baud);
extern void Uart1_Send_Byte(char data);
extern void Uart1_Send_String(char *pt);
extern void Uart1_Printf(char *fmt,...);
extern char Uart1_Get_Char(void);
extern char Uart1_Get_Pressed(void);

// SysTick.c

extern void SysTick_Run(unsigned int msec);
extern int SysTick_Check_Timeout(void);
extern unsigned int SysTick_Get_Time(void);
extern unsigned int SysTick_Get_Load_Time(void);
extern void SysTick_Stop(void);

// Led.c

extern void LED_Init(void);
extern void LED_On(void);
extern void LED_Off(void);
extern void LED_PB0_Init(void);
extern void LED_PB0_On(void);
extern void LED_PB0_Off(void);
extern void Board_RGB_LED_Init(void);
extern void Board_Status_LEds_Update(int flame_emergency, int gas_mode);

extern void GasFan_Init(void);
extern void GasFan_On(void);
extern void GasFan_Off(void);

// Clock.c

extern void Clock_Init(void);

// Key.c

extern void Key_Poll_Init(void);
extern int Key_Get_Pressed(void);
extern void Key_Wait_Key_Released(void);
extern void Key_Wait_Key_Pressed(void);
extern void Key_ISR_Enable(int en);

// adc.c

extern void ADC1_PA0_PA1_PA4_Init(void);
extern void ADC1_Read_PA0_PA1_PA4(int *a0, int *a1, int *a2);
extern void ADC1_PA1_PA4_PC1_Init(void);
extern void ADC1_Read_PA1_PA4_PC1(int *a1_ir, int *a2_gas, int *a3_flame);
extern void ADC1_PA1_PA2_PC1_Init(void);
extern void ADC1_Read_PA1_PA2_PC1(int *a1_ir, int *a2_gas, int *a3_flame);
extern int ADC1_Read_Channel(unsigned int ch);
extern void ADC1_PA1_PA4_PB0_Init(void);
extern void ADC1_Read_PA1_PA4_PB0(int *a1, int *a2, int *a3);
extern void ADC1_Start(void);
extern void ADC1_Stop(void);
extern int ADC1_Get_Status(void);
extern int ADC1_Get_Data(void);

// Timer.c

extern void TIM2_Delay(int time);
extern void TIM2_Stopwatch_Start(void);
extern unsigned int TIM2_Stopwatch_Stop(void);
extern void TIM4_Repeat(int time);
extern int TIM4_Check_Timeout(void);
extern void TIM4_Stop(void);
extern void TIM4_Change_Value(int time);
extern void TIM3_Out_Init(void);
extern void TIM3_Out_Freq_Generation(unsigned short freq);
extern void TIM3_Out_Stop(void);
extern void Servo_PWM_Init(void);
extern void Servo_PWM_SetPulseUs(unsigned int us);

#include "servo.h"
#include "stepmotor.h"
#include "firewall_stepper.h"

extern void Servo_Construct(Servo *s);
extern void Servo_HwInit(void);
extern void Servo_Init(Servo *s);
extern void Servo_SetAngle(Servo *s, int angle_deg);
extern void Servo_SetPulseUs(Servo *s, unsigned int pulse_us);