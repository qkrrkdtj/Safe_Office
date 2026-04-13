#include "device_driver.h"
#include "firewall_stepper.h"

#define FW_PIN_MASK	0xFu
#define FW_TIM_TICK_HZ	1000000u

static const unsigned char s_full_phase[4] = {
	0x3u, 0x6u, 0xCu, 0x9u,
};

typedef struct {
	GPIO_TypeDef *gpio;
	unsigned char pin[4];
	TIM_TypeDef *tim;
	volatile int steps_left;
	volatile int dir;
	volatile int pos;
	volatile int abort;
	unsigned int phase_idx;
	int invert_phase;
} FwMotor;

static FwMotor s_fw[2] = {
	{
		GPIOB,
		{ 12u, 13u, 14u, 15u },
		TIM2,
		0, 0, 0, 0, 0u,
		0,
	},
	{
		GPIOC,
		{ 5u, 6u, 7u, 8u },
		TIM3,
		0, 0, 0, 0, 0u,
		0,
	},
};

static unsigned int s_step_delay_us = 3000u;
static void (*s_while_step_poll)(void);

static void apply_coils(FwMotor *m, unsigned int pattern)
{
	unsigned int p = pattern & FW_PIN_MASK;
	unsigned int k;

	for (k = 0u; k < 4u; k++) {
		unsigned int pin = m->pin[k];
		unsigned int bit = 1u << k;

		if (p & bit)
			m->gpio->BSRR = 1u << pin;
		else
			m->gpio->BSRR = 1u << (pin + 16u);
	}
}

static void fw_tim_load(TIM_TypeDef *tim)
{
	unsigned int us = s_step_delay_us;

	if (us < 100u)
		us = 100u;
	if (us > 60000u)
		us = 60000u;
	tim->PSC = (unsigned int)(TIMXCLK / (double)FW_TIM_TICK_HZ + 0.5) - 1u;
	tim->ARR = us - 1u;
	tim->EGR = TIM_EGR_UG;
	tim->SR = 0u;
}

static void fw_start_motion(FwMotor *m, int steps)
{
	int d = (steps >= 0) ? 1 : -1;
	int n = (steps >= 0) ? steps : -steps;

	if (n <= 0)
		return;
	m->dir = d;
	m->steps_left = n;
	fw_tim_load(m->tim);
	m->tim->CNT = 0u;
	m->tim->SR = 0u;
	m->tim->CR1 |= TIM_CR1_CEN;
}

static void wait_parallel_idle(void)
{
	while (s_fw[0].steps_left > 0 || s_fw[1].steps_left > 0) {
		if (s_while_step_poll)
			s_while_step_poll();
		__asm volatile ("" ::: "memory");
	}
}

static void fw_motor_isr(FwMotor *m)
{
	int phdir;

	if (!(m->tim->SR & TIM_SR_UIF))
		return;
	m->tim->SR = 0u;

	if (m->steps_left <= 0) {
		m->tim->CR1 &= ~TIM_CR1_CEN;
		return;
	}

	if (m->abort) {
		m->steps_left = 0;
		m->tim->CR1 &= ~TIM_CR1_CEN;
		return;
	}

	phdir = m->invert_phase ? -m->dir : m->dir;
	if (phdir > 0)
		m->phase_idx = (m->phase_idx + 1u) & 3u;
	else
		m->phase_idx = (m->phase_idx + 3u) & 3u;
	apply_coils(m, s_full_phase[m->phase_idx]);
	m->pos += m->dir;
	if (m->pos < 0)
		m->pos = 0;
	if (m->pos > (int)FIREWALL_TRAVEL_STEPS)
		m->pos = (int)FIREWALL_TRAVEL_STEPS;
	m->steps_left--;
	if (m->steps_left <= 0)
		m->tim->CR1 &= ~TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
	fw_motor_isr(&s_fw[0]);
}

void TIM3_IRQHandler(void)
{
	fw_motor_isr(&s_fw[1]);
}

void FirewallStepper_Init(void)
{
	unsigned int mi;
	unsigned int k;

	Macro_Set_Bit(RCC->AHB1ENR, 1);
	Macro_Set_Bit(RCC->AHB1ENR, 2);

	for (mi = 0u; mi < 2u; mi++) {
		FwMotor *m = &s_fw[mi];

		for (k = 0u; k < 4u; k++) {
			unsigned int pin = m->pin[k];

			Macro_Write_Block(m->gpio->MODER, 0x3u, 0x1u, pin * 2u);
			Macro_Clear_Bit(m->gpio->OTYPER, pin);
		}
		m->steps_left = 0;
		m->dir = 0;
		m->pos = (int)FIREWALL_TRAVEL_STEPS;
		m->abort = 0;
		m->phase_idx = 0u;
		apply_coils(m, 0u);
	}

	Macro_Set_Bit(RCC->APB1ENR, 0);
	Macro_Set_Bit(RCC->APB1ENR, 1);
	s_fw[0].tim->CR1 = 0;
	s_fw[0].tim->DIER = TIM_DIER_UIE;
	fw_tim_load(s_fw[0].tim);
	NVIC_SetPriority(TIM2_IRQn, 3);
	NVIC_EnableIRQ(TIM2_IRQn);

	s_fw[1].tim->CR1 = 0;
	s_fw[1].tim->DIER = TIM_DIER_UIE;
	fw_tim_load(s_fw[1].tim);
	NVIC_SetPriority(TIM3_IRQn, 3);
	NVIC_EnableIRQ(TIM3_IRQn);

	s_while_step_poll = 0;
}

void FirewallStepper_SetStepDelayUs(unsigned int us)
{
	if (us < 100u)
		us = 100u;
	s_step_delay_us = us;
	if (!(s_fw[0].tim->CR1 & TIM_CR1_CEN))
		fw_tim_load(s_fw[0].tim);
	if (!(s_fw[1].tim->CR1 & TIM_CR1_CEN))
		fw_tim_load(s_fw[1].tim);
}

void FirewallStepper_SetWhileStepPoll(void (*fn)(void))
{
	s_while_step_poll = fn;
}

void FirewallStepper_OpenBoth(void)
{
	int n0 = (int)FIREWALL_TRAVEL_STEPS - s_fw[0].pos;
	int n1 = (int)FIREWALL_TRAVEL_STEPS - s_fw[1].pos;

	if (n0 < 0)
		n0 = 0;
	if (n1 < 0)
		n1 = 0;
	fw_start_motion(&s_fw[0], n0);
	fw_start_motion(&s_fw[1], n1);
	wait_parallel_idle();
}

void FirewallStepper_CloseBoth(void)
{
	fw_start_motion(&s_fw[0], -s_fw[0].pos);
	fw_start_motion(&s_fw[1], -s_fw[1].pos);
	wait_parallel_idle();
}

void FirewallStepper_EmergencyCloseAll(void)
{
	s_fw[0].abort = 1;
	s_fw[1].abort = 1;
	wait_parallel_idle();
	s_fw[0].abort = 0;
	s_fw[1].abort = 0;
	FirewallStepper_CloseBoth();
}

void FirewallStepper_Release(void)
{
	s_fw[0].steps_left = 0;
	s_fw[1].steps_left = 0;
	s_fw[0].tim->CR1 &= ~TIM_CR1_CEN;
	s_fw[1].tim->CR1 &= ~TIM_CR1_CEN;
	apply_coils(&s_fw[0], 0u);
	apply_coils(&s_fw[1], 0u);
}

int FirewallStepper_Fw1IsOpen(void)
{
	return s_fw[0].pos >= (int)FIREWALL_TRAVEL_STEPS;
}

int FirewallStepper_Fw2IsOpen(void)
{
	return s_fw[1].pos >= (int)FIREWALL_TRAVEL_STEPS;
}
