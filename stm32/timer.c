#include "device_driver.h"

#define SERVO_TIM_HZ	1000000U
#define SERVO_PERIOD_US	20000U

/* Initialises TIM5 CH1 PWM output on PA0 (A0) at 50 Hz for servo control */
void Servo_PWM_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 0);
	Macro_Set_Bit(RCC->APB1ENR, 3);  /* TIM5 clock enable (bit 3) */

	Macro_Write_Block(GPIOA->MODER,   0x3, 0x2, 0);  /* PA0 → ALT */
	Macro_Write_Block(GPIOA->OSPEEDR, 0x3, 0x3, 0);  /* PA0 → high speed */
	Macro_Write_Block(GPIOA->AFR[0],  0xf, 0x2, 0);  /* PA0 → AF2 (TIM5 CH1) */

	TIM5->CR1 = 0;
	TIM5->PSC = (unsigned int)(TIMXCLK / (double)SERVO_TIM_HZ + 0.5) - 1;
	TIM5->ARR = SERVO_PERIOD_US - 1;
	TIM5->CCR1 = 1500;
	TIM5->CNT = 0;
	TIM5->CCMR1 = 0;
	TIM5->CCMR2 = 0;
	Macro_Write_Block(TIM5->CCMR1, 0x7, 0x6, 4);
	Macro_Set_Bit(TIM5->CCMR1, 3);
	TIM5->CCER = (1u << 0);
	Macro_Set_Bit(TIM5->EGR, 0);
	TIM5->CR1 = (1u << 0);
}

/* Sets the PWM pulse width in microseconds (1000–2000 µs → 0°–180°) */
void Servo_PWM_SetPulseUs(unsigned int us)
{
	if (us < 1000) us = 1000;
	if (us > 2000) us = 2000;
	TIM5->CCR1 = us;
}
