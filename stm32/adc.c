#include "device_driver.h"

void ADC1_PA0_PA1_PA4_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 0);
	Macro_Write_Block(GPIOA->MODER, 0x3, 0x3, 0);
	Macro_Write_Block(GPIOA->MODER, 0x3, 0x3, 2);
	Macro_Write_Block(GPIOA->MODER, 0x3, 0x3, 8);

	Macro_Set_Bit(RCC->APB2ENR, 8);
	Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 0);
	Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 3);
	Macro_Write_Block(ADC1->SMPR2, 0x7, 0x7, 12);
	Macro_Write_Block(ADC1->SQR1, 0xF, 0x0, 20);

	Macro_Write_Block(ADC->CCR, 0x3, 0x2, 16);
	Macro_Set_Bit(ADC1->CR2, 0);
}

void ADC1_Start(void)
{
	Macro_Set_Bit(ADC1->CR2, 30);
}

void ADC1_Stop(void)
{
	Macro_Clear_Bit(ADC1->CR2, 30);
	Macro_Clear_Bit(ADC1->CR2, 0);
}

int ADC1_Get_Status(void)
{
	int r = Macro_Check_Bit_Set(ADC1->SR, 1);

	if (r)
	{
		Macro_Clear_Bit(ADC1->SR, 1);
		Macro_Clear_Bit(ADC1->SR, 4);
	}

	return r;
}

int ADC1_Get_Data(void)
{
	return Macro_Extract_Area(ADC1->DR, 0xFFF, 0);
}

void ADC1_Read_PA0_PA1_PA4(int *a0, int *a1, int *a2)
{
	Macro_Write_Block(ADC1->SQR3, 0x1F, 0x0, 0);
	ADC1_Start();
	while (!ADC1_Get_Status())
		;
	*a0 = ADC1_Get_Data();

	Macro_Write_Block(ADC1->SQR3, 0x1F, 0x1, 0);
	ADC1_Start();
	while (!ADC1_Get_Status())
		;
	*a1 = ADC1_Get_Data();

	Macro_Write_Block(ADC1->SQR3, 0x1F, 0x4, 0);
	ADC1_Start();
	while (!ADC1_Get_Status())
		;
	*a2 = ADC1_Get_Data();
}

static void ADC1_Convert_Channel(unsigned int ch, int *out)
{
	Macro_Write_Block(ADC1->SQR3, 0x1Fu, ch, 0);
	ADC1_Start();
	while (!ADC1_Get_Status())
		;
	*out = ADC1_Get_Data();
}

void ADC1_PA1_PA4_PC1_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 0);
	Macro_Set_Bit(RCC->AHB1ENR, 2);
	Macro_Write_Block(GPIOA->MODER, 0x3u, 0x3u, 2u);
	Macro_Write_Block(GPIOA->MODER, 0x3u, 0x3u, 8u);
	Macro_Write_Block(GPIOC->MODER, 0x3u, 0x3u, 2u);

	Macro_Set_Bit(RCC->APB2ENR, 8);
	Macro_Write_Block(ADC1->SMPR2, 0x7u, 0x7u, 3u);
	Macro_Write_Block(ADC1->SMPR2, 0x7u, 0x7u, 12u);
	Macro_Write_Block(ADC1->SMPR1, 0x7u, 0x7u, 3u);
	Macro_Write_Block(ADC1->SQR1, 0xFu, 0x0u, 20u);

	Macro_Write_Block(ADC->CCR, 0x3u, 0x2u, 16u);
	Macro_Set_Bit(ADC1->CR2, 0);
}

void ADC1_Read_PA1_PA4_PC1(int *a1_ir, int *a2_gas, int *a3_flame)
{
	ADC1_Convert_Channel(1u, a1_ir);
	ADC1_Convert_Channel(4u, a2_gas);
	ADC1_Convert_Channel(11u, a3_flame);
}

void ADC1_PA1_PA2_PC1_Init(void)
{
	ADC1_PA1_PA4_PC1_Init();
}

void ADC1_Read_PA1_PA2_PC1(int *a1_ir, int *a2_gas, int *a3_flame)
{
	ADC1_Read_PA1_PA4_PC1(a1_ir, a2_gas, a3_flame);
}

int ADC1_Read_Channel(unsigned int ch)
{
	int v;

	ADC1_Convert_Channel(ch, &v);
	return v;
}

void ADC1_PA1_PA4_PB0_Init(void)
{
	ADC1_PA1_PA4_PC1_Init();
}

void ADC1_Read_PA1_PA4_PB0(int *a1, int *a2, int *a3)
{
	ADC1_Read_PA1_PA4_PC1(a1, a2, a3);
}
