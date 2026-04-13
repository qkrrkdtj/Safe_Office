#include "device_driver.h"
#include "stepmotor.h"
#include <stdio.h>

#define STEPMOTOR_GPIO		GPIOB
#define STEPMOTOR_RCC_AHB1ENR_BIT	1

#define STEPMOTOR_PIN_MASK	0xFu
#define STEPTIM_TICK_HZ		1000000u
#define STEPMOTOR_GEAR_PRINT_STEP	450

static const unsigned char s_coil_pin[4] = { 3u, 5u, 4u, 10u };

static const unsigned char s_full_phase[4] = {
	0x3u, 0x6u, 0xCu, 0x9u,
};

static unsigned int s_phase_idx;
static unsigned int s_step_delay_us = 3000u;

volatile static int s_tim_steps_left;
volatile static int s_tim_dir;
volatile static int s_abort_motion;
volatile static int s_door_pos;

static void (*s_while_step_poll)(void);

static void delay_ms_busy(unsigned int ms)
{
	volatile unsigned int n;

	n = (SYSCLK / 4000U) * ms;
	while (n--)
		__asm volatile ("" ::: "memory");
}

static void step_tim_load_period(void)
{
	unsigned int us = s_step_delay_us;

	if (us < 100u)
		us = 100u;
	if (us > 60000u)
		us = 60000u;
	TIM4->PSC = (unsigned int)(TIMXCLK / (double)STEPTIM_TICK_HZ + 0.5) - 1u;
	TIM4->ARR = us - 1u;
	TIM4->EGR = TIM_EGR_UG;
	TIM4->SR = 0u;
}

static void apply_coils_fast(unsigned int pattern)
{
	unsigned int p = pattern & STEPMOTOR_PIN_MASK;
	unsigned int pin;
	unsigned int bit;

	pin = s_coil_pin[0];
	bit = 1u << 0;
	if (p & bit)
		STEPMOTOR_GPIO->BSRR = 1u << pin;
	else
		STEPMOTOR_GPIO->BSRR = 1u << (pin + 16u);
	pin = s_coil_pin[1];
	bit = 1u << 1;
	if (p & bit)
		STEPMOTOR_GPIO->BSRR = 1u << pin;
	else
		STEPMOTOR_GPIO->BSRR = 1u << (pin + 16u);
	pin = s_coil_pin[2];
	bit = 1u << 2;
	if (p & bit)
		STEPMOTOR_GPIO->BSRR = 1u << pin;
	else
		STEPMOTOR_GPIO->BSRR = 1u << (pin + 16u);
	pin = s_coil_pin[3];
	bit = 1u << 3;
	if (p & bit)
		STEPMOTOR_GPIO->BSRR = 1u << pin;
	else
		STEPMOTOR_GPIO->BSRR = 1u << (pin + 16u);
}

static void apply_coils(unsigned int pattern)
{
	apply_coils_fast(pattern);
}

void StepMotor_Init(void)
{
	unsigned int m;

	Macro_Set_Bit(RCC->AHB1ENR, STEPMOTOR_RCC_AHB1ENR_BIT);
	for (m = 0; m < 4u; m++) {
		unsigned int pin = s_coil_pin[m];

		Macro_Write_Block(STEPMOTOR_GPIO->MODER, 0x3u, 0x1u, pin * 2u);
		Macro_Clear_Bit(STEPMOTOR_GPIO->OTYPER, pin);
	}
	s_phase_idx = 0u;
	s_door_pos = 0;
	s_abort_motion = 0;
	apply_coils(0u);

	Macro_Set_Bit(RCC->APB1ENR, 2);
	TIM4->CR1 = 0;
	TIM4->DIER = TIM_DIER_UIE;
	step_tim_load_period();
	NVIC_SetPriority(TIM4_IRQn, 2);
	NVIC_EnableIRQ(TIM4_IRQn);
	s_while_step_poll = 0;
}

void StepMotor_SetWhileStepPoll(void (*fn)(void))
{
	s_while_step_poll = fn;
}

void StepMotor_SetStepDelayMs(unsigned int ms)
{
	if (ms < 1u)
		ms = 1u;
	s_step_delay_us = ms * 1000u;
}

void StepMotor_SetStepDelayUs(unsigned int us)
{
	if (us < 100u)
		us = 100u;
	s_step_delay_us = us;
	if (!(TIM4->CR1 & TIM_CR1_CEN))
		step_tim_load_period();
}

void TIM4_IRQHandler(void)
{
	if (!(TIM4->SR & TIM_SR_UIF))
		return;
	TIM4->SR = 0u;

	if (s_tim_steps_left <= 0) {
		TIM4->CR1 &= ~TIM_CR1_CEN;
		return;
	}

	if (s_abort_motion) {
		s_tim_steps_left = 0;
		TIM4->CR1 &= ~TIM_CR1_CEN;
		return;
	}

	if (s_tim_dir > 0)
		s_phase_idx = (s_phase_idx + 1u) & 3u;
	else
		s_phase_idx = (s_phase_idx + 3u) & 3u;
	apply_coils_fast(s_full_phase[s_phase_idx]);
	s_door_pos += s_tim_dir;
	if (s_door_pos < 0)
		s_door_pos = 0;
	if (s_door_pos > (int)STEPMOTOR_DOOR_STEPS)
		s_door_pos = (int)STEPMOTOR_DOOR_STEPS;
	s_tim_steps_left--;
	if (s_tim_steps_left <= 0)
		TIM4->CR1 &= ~TIM_CR1_CEN;
}

void StepMotor_Step(int steps)
{
	int dir = (steps >= 0) ? 1 : -1;
	int n = (steps >= 0) ? steps : -steps;

	if (n <= 0)
		return;

	s_tim_dir = dir;
	s_tim_steps_left = n;
	step_tim_load_period();
	TIM4->CNT = 0u;
	TIM4->SR = 0u;
	TIM4->CR1 |= TIM_CR1_CEN;

	{
		int last_mark = s_door_pos / STEPMOTOR_GEAR_PRINT_STEP;

		printf("door gear: start %s, pos=%d/%d, this move=%d steps\n",
			dir > 0 ? "OPEN" : "CLOSE", s_door_pos,
			(int)STEPMOTOR_DOOR_STEPS, n);

		while (s_tim_steps_left > 0) {
			int p = s_door_pos;
			int m = p / STEPMOTOR_GEAR_PRINT_STEP;

			if (m != last_mark) {
				last_mark = m;
				printf("door gear: pos=%d/%d, remaining this move=%d\n",
					p, (int)STEPMOTOR_DOOR_STEPS,
					s_tim_steps_left);
			}
			if (s_while_step_poll)
				s_while_step_poll();
			__asm volatile ("" ::: "memory");
		}

		printf("door gear: done, pos=%d/%d\n", s_door_pos,
			(int)STEPMOTOR_DOOR_STEPS);
	}
}

static void StepMotor_AbortMotion(void)
{
	s_abort_motion = 1;
	while (s_tim_steps_left > 0)
		__asm volatile ("" ::: "memory");
	s_abort_motion = 0;
}

static void StepMotor_OpenRemainderAfterAbort(void)
{
	int n;

	StepMotor_AbortMotion();
	n = (int)STEPMOTOR_DOOR_STEPS - s_door_pos;
	if (n > 0)
		StepMotor_Step(n);
}

void StepMotor_Release(void)
{
	apply_coils(0u);
}

void StepMotor_DoorOpen(void)
{
	int n;

	if (s_door_pos >= (int)STEPMOTOR_DOOR_STEPS)
		return;
	n = (int)STEPMOTOR_DOOR_STEPS - s_door_pos;
	StepMotor_Step(n);
}

void StepMotor_DoorOpenForce(void)
{
	StepMotor_OpenRemainderAfterAbort();
}

void StepMotor_EmergencyOpen(void)
{
	StepMotor_OpenRemainderAfterAbort();
}

void StepMotor_DoorClose(void)
{
	if (s_door_pos <= 0)
		return;
	StepMotor_Step(-s_door_pos);
}

int StepMotor_DoorIsOpen(void)
{
	return s_door_pos >= (int)STEPMOTOR_DOOR_STEPS;
}

void StepMotor_Test_Run(void)
{
	const int test_steps = 512;

	printf("Stepper: IN1=D3(PB3) IN2=D4(PB5) IN3=D5(PB4) IN4=D6(PB10) ULN2003\n");
	StepMotor_SetStepDelayUs(2000u);
	printf("Test: %d steps CW, pause, %d CCW, release\n", test_steps, test_steps);
	StepMotor_Step(test_steps);
	delay_ms_busy(200);
	StepMotor_Step(-test_steps);
	StepMotor_Release();
	printf("Stepper test done.\n");
}
