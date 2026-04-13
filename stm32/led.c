#include "device_driver.h"

void LED_Init(void)
{
	/* 아래 코드 수정 금지 : Port-A Clock Enable */
	Macro_Set_Bit(RCC->AHB1ENR, 0); 

	// LED를 출력으로 설정하고 초기 OFF
	Macro_Write_Block(GPIOA->MODER, 0x3, 0x1, 10);
	Macro_Clear_Bit(GPIOA->OTYPER, 5);
	Macro_Clear_Bit(GPIOA->ODR, 5); 
}

void LED_On(void)
{
	// LED On
	Macro_Set_Bit(GPIOA->ODR, 5); 
}

void LED_Off(void)
{
	// LED Off
	Macro_Clear_Bit(GPIOA->ODR, 5); 
}

void LED_PB0_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 1);
	Macro_Write_Block(GPIOB->MODER, 0x3, 0x1, 0);
	Macro_Clear_Bit(GPIOB->OTYPER, 0);
	Macro_Clear_Bit(GPIOB->ODR, 0);
}

void LED_PB0_On(void)
{
	Macro_Set_Bit(GPIOB->ODR, 0);
}

void LED_PB0_Off(void)
{
	Macro_Clear_Bit(GPIOB->ODR, 0);
}

#define STATUS_LED_YELLOW_PIN	0u
#define STATUS_LED_RED_PIN	2u
#define STATUS_LED_GREEN_PIN	4u

static void board_status_led_hw_init_pin(unsigned int pin)
{
	Macro_Write_Block(GPIOC->MODER, 0x3u, 0x1u, pin * 2u);
	Macro_Clear_Bit(GPIOC->OTYPER, pin);
#if BOARD_STATUS_LEDS_ACTIVE_LOW
	Macro_Set_Bit(GPIOC->ODR, pin);
#else
	Macro_Clear_Bit(GPIOC->ODR, pin);
#endif
}

static void status_leds_drive(int red_on, int green_on, int yellow_on)
{
	unsigned int yellow_bit = 1u << STATUS_LED_YELLOW_PIN;
	unsigned int red_bit = 1u << STATUS_LED_RED_PIN;
	unsigned int green_bit = 1u << STATUS_LED_GREEN_PIN;
	unsigned int c_set = 0u;
	unsigned int c_clr = 0u;

#if BOARD_STATUS_LEDS_ACTIVE_LOW
	if (!red_on)
		c_set |= red_bit;
	else
		c_clr |= red_bit;
	if (!green_on)
		c_set |= green_bit;
	else
		c_clr |= green_bit;
	if (!yellow_on)
		c_set |= yellow_bit;
	else
		c_clr |= yellow_bit;
#else
	if (red_on)
		c_set |= red_bit;
	else
		c_clr |= red_bit;
	if (green_on)
		c_set |= green_bit;
	else
		c_clr |= green_bit;
	if (yellow_on)
		c_set |= yellow_bit;
	else
		c_clr |= yellow_bit;
#endif
	GPIOC->BSRR = c_set | (c_clr << 16u);
}

void Board_RGB_LED_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 2);
	board_status_led_hw_init_pin(STATUS_LED_YELLOW_PIN);
	board_status_led_hw_init_pin(STATUS_LED_RED_PIN);
	board_status_led_hw_init_pin(STATUS_LED_GREEN_PIN);
}

void Board_Status_LEds_Update(int flame_emergency, int gas_mode)
{
	int red_on = flame_emergency;
	int green_on = !flame_emergency && !gas_mode;
	int yellow_on = gas_mode;

	status_leds_drive(red_on, green_on, yellow_on);
}

