#include "device_driver.h"
#include <stdint.h>
#include <stdio.h>
volatile int Key_Pressed = 0;

#define MOVEMENT_ADC_THRESHOLD	2000
#define DOOR_HOLD_MS		3000u
#define STARTUP_SETTLE_MS	500u
#define MOVEMENT_BASELINE_STREAK	20u
#define MOVEMENT_CONFIRM_READS	5u
#define EXIT_MODE_TICK_MS	10u
#define EXIT_ADC_BURST		5u
#define EXIT_MOVEMENT_TICKS	2u
#define ESP32_UART_BAUD		115200
#define ESP32_LINE_TIMEOUT_MS	5000u
#define FLAME_EMERGENCY_TRIGGER_ADC	500
#define FLAME_EMERGENCY_REARM_BELOW	10
#define FLAME_EMERGENCY_SUSTAIN_MS	500u
#define FLAME_COUNTDOWN_PRINT_MS	500u
#define GAS_MODE_TRIGGER_ADC		1200
#define GAS_MODE_REARM_BELOW		800
#define GAS_WINDOW_OPEN_DEG		0
#define GAS_WINDOW_CLOSED_DEG		180
#define EXIT_BUTTON_PIN			6u
#define EMERGENCY_CLEAR_BUTTON_PIN	7u

static unsigned int s_person_count;
static int s_emergency_mode;
static int s_emergency_block_retrigger;
static int s_gas_mode;
static int s_gas_block_retrigger;
static Servo s_window_servo;
static volatile int s_exit_button_pending;
static int s_exit_button_arm;
static int s_exit_btn_saw_released;
static int s_emerg_clear_arm;
static int s_emerg_clear_saw_released;
static int s_exit_inputs_ready;
static int s_flame_baseline_ok;

static void Auto_Door_On_Movement(int a1);
static void Door_Run_Open_Hold_Close(const char *tag);
static void Exit_Run_Door_Open_First(void);

static void Sys_Init(void)
{
	SCB->CPACR |= (0x3 << 10*2)|(0x3 << 11*2);
	Clock_Init();
	Uart2_Init(115200);
	Uart1_Init(ESP32_UART_BAUD);
	setvbuf(stdout, NULL, _IONBF, 0);
}

static void Print_Door_Status(void)
{
	printf("door: %s\n", StepMotor_DoorIsOpen() ? "OPEN" : "CLOSED");
}

static void Exit_Button_Init(void)
{
	unsigned int pos = EXIT_BUTTON_PIN * 2u;

	Macro_Set_Bit(RCC->AHB1ENR, 0);
	Macro_Write_Block(GPIOA->MODER, 0x3u, 0x0u, pos);
	Macro_Write_Block(GPIOA->PUPDR, 0x3u, 0x1u, pos);
}

static int Exit_Button_IsPressed(void)
{
	return Macro_Check_Bit_Clear(GPIOA->IDR, EXIT_BUTTON_PIN);
}

static void Emergency_Clear_Button_Init(void)
{
	unsigned int pos = EMERGENCY_CLEAR_BUTTON_PIN * 2u;

	Macro_Set_Bit(RCC->AHB1ENR, 0);
	Macro_Write_Block(GPIOA->MODER, 0x3u, 0x0u, pos);
	Macro_Write_Block(GPIOA->PUPDR, 0x3u, 0x1u, pos);
}

static int Emergency_Clear_Button_IsPressed(void)
{
	return Macro_Check_Bit_Clear(GPIOA->IDR, EMERGENCY_CLEAR_BUTTON_PIN);
}

static void Refresh_Status_LEDs(void)
{
	Board_Status_LEds_Update(s_emergency_mode, s_gas_mode);
}

static void Apply_Alarm_Clear(void)
{
	int had_flame = s_emergency_mode;
	int had_gas = s_gas_mode;

	s_emergency_mode = 0;
	s_emergency_block_retrigger = 1;
	s_gas_mode = 0;
	s_gas_block_retrigger = 1;

	printf("ALARM CLEAR: PA7 — re-arm flame after ADC < %d; gas after PA4 < %d\n",
		FLAME_EMERGENCY_REARM_BELOW, GAS_MODE_REARM_BELOW);
	if (had_flame) {
		StepMotor_DoorClose();
		Print_Door_Status();
		FirewallStepper_OpenBoth();
		printf("firewalls: OPEN (normal)\n");
	}
	if (had_gas) {
		GasFan_Off();
		Servo_SetAngle(&s_window_servo, GAS_WINDOW_CLOSED_DEG);
		printf("gas: fan off, window servo to %d deg\n", GAS_WINDOW_CLOSED_DEG);
	}
	Refresh_Status_LEDs();
}

static void Poll_Emergency_Clear_Button(void)
{
	if (!s_exit_inputs_ready || (!s_emergency_mode && !s_gas_mode))
		return;

	if (!Emergency_Clear_Button_IsPressed()) {
		s_emerg_clear_saw_released = 1;
		s_emerg_clear_arm = 1;
		return;
	}

	if (!s_emerg_clear_saw_released)
		return;

	if (s_emerg_clear_arm) {
		s_emerg_clear_arm = 0;
		Apply_Alarm_Clear();
	}
}

static void Flame_Try_Enable_Baseline(int adc_pc1)
{
	if (!s_flame_baseline_ok && adc_pc1 < FLAME_EMERGENCY_TRIGGER_ADC) {
		s_flame_baseline_ok = 1;
		printf("AUTO: flame PC1=%d below trigger — fire channel ready.\n", adc_pc1);
	}
}

static void Dwt_Cycle_Counter_Enable(void)
{
	static int s_done;

	if (s_done)
		return;
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	s_done = 1;
}

static void Poll_Flame_Emergency(void)
{
	int flame = ADC1_Read_Channel(11u);
	static uint64_t s_flame_high_cycles;
	static uint64_t s_flame_print_cycle_acc;
	static uint32_t s_prev_cyccnt;
	static int s_have_prev_cyccnt;
	uint32_t dcycles = 0;

	if (flame < FLAME_EMERGENCY_REARM_BELOW)
		s_emergency_block_retrigger = 0;

	Dwt_Cycle_Counter_Enable();

	if (s_have_prev_cyccnt) {
		uint32_t c = DWT->CYCCNT;

		dcycles = c - s_prev_cyccnt;
		s_prev_cyccnt = c;
	} else {
		s_prev_cyccnt = DWT->CYCCNT;
		s_have_prev_cyccnt = 1;
	}

	if (!s_flame_baseline_ok) {
		s_flame_high_cycles = 0;
		return;
	}

	{
		static int s_flame_counting_prev;
		const uint64_t print_cycle_thresh = (uint64_t)HCLK
			* (uint64_t)FLAME_COUNTDOWN_PRINT_MS / 1000ULL;
		int can_trigger = !s_emergency_mode && !s_emergency_block_retrigger
			&& !s_gas_mode;
		int counting = flame >= FLAME_EMERGENCY_TRIGGER_ADC && can_trigger;

		if (counting) {
			int edge = !s_flame_counting_prev;
			unsigned int elapsed_ms;
			unsigned int remain;

			s_flame_counting_prev = 1;
			s_flame_high_cycles += (uint64_t)dcycles;
			elapsed_ms = (unsigned int)(s_flame_high_cycles * 1000ULL
				/ (uint64_t)HCLK);
			if (elapsed_ms >= FLAME_EMERGENCY_SUSTAIN_MS) {
				s_emergency_mode = 1;
				s_flame_high_cycles = 0;
				s_flame_counting_prev = 0;
				s_flame_print_cycle_acc = 0;
				printf("EMERGENCY: flame ADC(PC1)=%d >= %d for >= %u ms — opening door; closing firewalls (latched)\n",
					flame, FLAME_EMERGENCY_TRIGGER_ADC,
					FLAME_EMERGENCY_SUSTAIN_MS);
				StepMotor_EmergencyOpen();
				Print_Door_Status();
				FirewallStepper_EmergencyCloseAll();
				printf("firewalls: CLOSED (emergency)\n");
			} else {
				remain = FLAME_EMERGENCY_SUSTAIN_MS - elapsed_ms;

				if (edge) {
					printf("FLAME: PC1=%d high — ~%u ms until emergency if held\n",
						flame, remain);
					s_flame_print_cycle_acc = 0;
				} else {
					s_flame_print_cycle_acc += (uint64_t)dcycles;
					if (s_flame_print_cycle_acc >= print_cycle_thresh) {
						s_flame_print_cycle_acc = 0;
						printf("FLAME: ~%u ms until emergency (PC1=%d)\n",
							remain, flame);
					}
				}
			}
		} else {
			s_flame_high_cycles = 0;
			s_flame_counting_prev = 0;
			s_flame_print_cycle_acc = 0;
		}
	}
}

static void Poll_Gas_Mode(void)
{
	int gas = ADC1_Read_Channel(4u);

	if (gas < GAS_MODE_REARM_BELOW)
		s_gas_block_retrigger = 0;

	if (gas >= GAS_MODE_TRIGGER_ADC && !s_gas_mode && !s_gas_block_retrigger
	    && !s_emergency_mode) {
		s_gas_mode = 1;
		printf("GAS MODE: PA4 ADC=%d >= %d — yellow LED, fan PB1/PB2, window servo PA0=%d deg\n",
			gas, GAS_MODE_TRIGGER_ADC, GAS_WINDOW_OPEN_DEG);
		GasFan_On();
		Servo_SetAngle(&s_window_servo, GAS_WINDOW_OPEN_DEG);
	}
}

static void Poll_Exit_Button(void)
{
	if (!s_exit_inputs_ready)
		return;

	if (!Exit_Button_IsPressed()) {
		s_exit_btn_saw_released = 1;
		s_exit_button_arm = 1;
		return;
	}

	if (!s_exit_btn_saw_released)
		return;

	if (s_exit_button_arm) {
		s_exit_button_arm = 0;
		s_exit_button_pending = 1;
	}
}

static void Poll_App(void)
{
	Poll_Emergency_Clear_Button();
	Poll_Flame_Emergency();
	Poll_Gas_Mode();
	Poll_Exit_Button();
	Refresh_Status_LEDs();
}

static void Wait_Ms_Chunked(unsigned int ms)
{
	unsigned int tpm = (unsigned int)(HCLK / (8. * 1000.) + 0.5);
	unsigned int chunk = (tpm > 0u) ? (16777215u / tpm) : 1000u;

	if (chunk == 0u || chunk > 1000u)
		chunk = 1000u;

	while (ms > 0u) {
		unsigned int c = ms > chunk ? chunk : ms;

		SysTick_Run(c);
		while (!SysTick_Check_Timeout()) {
			Poll_App();
			__asm volatile ("" ::: "memory");
		}
		ms -= c;
	}
}

static int Str_To_Int(const char *s)
{
	int value = 0;
	int sign = 1;

	if (*s == '-') {
		sign = -1;
		s++;
	}

	while (*s) {
		if (*s < '0' || *s > '9')
			break;
		value = value * 10 + (*s - '0');
		s++;
	}

	return value * sign;
}

static int Esp32_ReadLine(char *buf, int max_len, unsigned int timeout_ms)
{
	unsigned int ms_left = timeout_ms;
	int i = 0;
	char ch;

	if (max_len < 2)
		return 0;

	while (i < max_len - 1 && ms_left > 0u) {
		ch = Uart1_Get_Pressed();
		if (ch != 0) {
			if (ch == '\n' || ch == '\r')
				break;
			buf[i++] = ch;
		}
		Wait_Ms_Chunked(1);
		ms_left--;
	}
	buf[i] = '\0';
	return i;
}

static void Run_Movement_Esp32_Sequence(int *sensor_armed)
{
	int a1, a2, a3;
	int n;
	char rx_buf[24];

	printf("AUTO: movement confirmed — ESP32 query (UART1)...\n");
	Wait_Ms_Chunked(50);
	ADC1_Read_PA1_PA4_PC1(&a1, &a2, &a3);
	if (a1 < MOVEMENT_ADC_THRESHOLD) {
		printf("AUTO: A1 cleared before ESP32 — re-armed.\n");
		*sensor_armed = 1;
		return;
	}

	printf("ESP32: SEND HELLO\n");
	Uart1_Send_String("HELLO\n");

	Esp32_ReadLine(rx_buf, sizeof(rx_buf), ESP32_LINE_TIMEOUT_MS);
	n = Str_To_Int(rx_buf);
	printf("ESP32: RECV \"%s\" -> %d\n", rx_buf, n);

	if (n >= 1)
		Auto_Door_On_Movement(a1);
	else
		printf("AUTO: ESP32 count < 1 — door stays closed.\n");

	while (1) {
		ADC1_Read_PA1_PA4_PC1(&a1, &a2, &a3);
		if (a1 < MOVEMENT_ADC_THRESHOLD)
			break;
		Wait_Ms_Chunked(50);
	}
	Wait_Ms_Chunked(50);
	*sensor_armed = 1;
}

static void Exit_Run_Door_Open_First(void)
{
	printf("EXIT: opening door first (stepper uses current position; skips if already fully open)...\n");
	StepMotor_DoorOpen();
	Print_Door_Status();
}

static void Door_Run_Open_Hold_Close(const char *tag)
{
	printf("%s: opening door (stepper uses current position; no extra steps if already open)...\n",
		tag);
	StepMotor_DoorOpen();
	printf("%s: open motion finished.\n", tag);
	Print_Door_Status();

	printf("%s: holding open %u ms...\n", tag, DOOR_HOLD_MS);
	Wait_Ms_Chunked(DOOR_HOLD_MS);
	printf("%s: closing door (steps only from current position toward fully closed)...\n", tag);

	StepMotor_DoorClose();
	printf("%s: close motion finished.\n", tag);
	Print_Door_Status();

	SysTick_Run(1000);
}

static void Auto_Door_On_Movement(int a1)
{
	printf("AUTO: A1=%d >= %d (movement) — entry sequence...\n", a1,
		MOVEMENT_ADC_THRESHOLD);
	Door_Run_Open_Hold_Close("AUTO");
	s_person_count++;
	printf("AUTO: person count (after open/close sequence) = %u\n", s_person_count);
}

static void Startup_Stabilize_Sensors(int *sensor_armed)
{
	int a1, a2, a3;
	unsigned int mov_low_streak = 0;
	unsigned int flame_low_streak = 0;
	unsigned int iter = 0;

	printf("AUTO: startup settle %u ms (ADC / sensor after reset)...\n",
		STARTUP_SETTLE_MS);
	Wait_Ms_Chunked(STARTUP_SETTLE_MS);

	printf("AUTO: need A1 < %d and flame PC1 < %d for %u samples each (~10 ms apart)...\n",
		MOVEMENT_ADC_THRESHOLD, FLAME_EMERGENCY_TRIGGER_ADC,
		MOVEMENT_BASELINE_STREAK);

	while ((mov_low_streak < MOVEMENT_BASELINE_STREAK
		|| flame_low_streak < MOVEMENT_BASELINE_STREAK) && iter < 500u) {
		ADC1_Read_PA1_PA4_PC1(&a1, &a2, &a3);
		if (a1 < MOVEMENT_ADC_THRESHOLD)
			mov_low_streak++;
		else
			mov_low_streak = 0;
		if (a3 < FLAME_EMERGENCY_TRIGGER_ADC)
			flame_low_streak++;
		else
			flame_low_streak = 0;
		iter++;
		Wait_Ms_Chunked(10);
	}

	if (mov_low_streak >= MOVEMENT_BASELINE_STREAK) {
		*sensor_armed = 1;
		printf("AUTO: movement sensor armed (last A1=%d).\n", a1);
	} else {
		*sensor_armed = 0;
		printf("AUTO: A1 stayed >= %d; will arm when it drops below.\n",
			MOVEMENT_ADC_THRESHOLD);
	}

	if (flame_low_streak >= MOVEMENT_BASELINE_STREAK) {
		s_flame_baseline_ok = 1;
		printf("AUTO: flame channel ready (last PC1=%d).\n", a3);
	} else {
		s_flame_baseline_ok = 0;
		printf("AUTO: flame PC1 stayed >= %d; will enable after it drops below.\n",
			FLAME_EMERGENCY_TRIGGER_ADC);
	}
}

void Main(void)
{
	unsigned int step_delay_us = 3000u;
	unsigned int move_high_streak = 0;
	unsigned int exit_move_high_streak = 0;
	int sensor_armed = 0;
	int exit_mode = 0;
	unsigned int exit_ms_left = 0;
	int exit_decrement_done = 0;
	char c;

	Sys_Init();
	StepMotor_Init();
	StepMotor_SetStepDelayUs(step_delay_us);
	ADC1_PA1_PA4_PC1_Init();
	Board_RGB_LED_Init();
	Board_Status_LEds_Update(0, 0);
	GasFan_Init();
	Servo_Init(&s_window_servo);
	Servo_SetAngle(&s_window_servo, GAS_WINDOW_CLOSED_DEG);
	Exit_Button_Init();
	Emergency_Clear_Button_Init();
	FirewallStepper_Init();
	FirewallStepper_SetStepDelayUs(step_delay_us);
	FirewallStepper_SetWhileStepPoll(Poll_App);
	StepMotor_SetWhileStepPoll(Poll_App);
	SysTick_Run(1000);

	printf("Door stepper IN1 PB3 IN2 PB5 IN3 PB4 IN4 PB10 — %d steps/travel, %u us/step\n",
		STEPMOTOR_DOOR_STEPS, step_delay_us);
	printf("UART2 ST-Link VCP: PA2 TX / PA3 RX; UART1 ESP32 PA9 TX / PA10 RX %u baud; gas ADC PA4\n",
		(unsigned)ESP32_UART_BAUD);
	printf("UART1: HELLO -> ESP32; open if count >= 1; +1 person per auto open\n");
	printf("AUTO: A1(IR) >= %d (%u hits) -> ESP32 then open, hold %u ms, close\n",
		MOVEMENT_ADC_THRESHOLD, MOVEMENT_CONFIRM_READS, DOOR_HOLD_MS);
	printf("Person count: %u (AUTO: ESP32+door; EXIT: open, IR %u s, movement -> count-1 no ESP32, then close)\n",
		s_person_count, DOOR_HOLD_MS / 1000u);
	printf("EXIT: PA6 or 'e' (off if flame emergency) — open; PA1 movement -> -1; close after %u s from open\n",
		DOOR_HOLD_MS / 1000u);
	printf("ALARM CLEAR: PA7 — clears flame emergency + gas mode (door/FW/fan/servo/LEDs)\n");
	printf("  = / -   : manual open / close\n");
	printf("  space/r : release coils (state unchanged)\n");
	printf("  [ / ]   : step delay us, now %u\n", step_delay_us);
	printf("Flame PC1: EMERGENCY if ADC >= %d for >= %u ms; clear PA7; re-arm after ADC < %d\n",
		FLAME_EMERGENCY_TRIGGER_ADC, FLAME_EMERGENCY_SUSTAIN_MS,
		FLAME_EMERGENCY_REARM_BELOW);
	printf("Gas PA4: GAS MODE if ADC >= %d; yellow PC0; re-arm after < %d; clear PA7\n",
		GAS_MODE_TRIGGER_ADC, GAS_MODE_REARM_BELOW);
	printf("Fan L293D: PB1->1A(IN1) PB2->2A(IN2); motor between 1Y & 2Y; tie 1,2EN to 3.3V\n");
	printf("Window servo: signal PA0 (TIM5 PWM, 50 Hz); power servo from 5V + GND\n");
	printf("Firewalls: FW1 PB12-15 / FW2 PC5-8 (TIM2/TIM3), opposite spin; 'f' open 'g' close 'F' release\n");
	printf("Status LEDs: PC0 yellow (gas) | PC2 red (flame) | PC4 green (normal)\n");
	printf("Every 1 s: IR PA1 | gas PA4(A2) | flame PC1 (ADC)\n");
	Print_Door_Status();

	Startup_Stabilize_Sensors(&sensor_armed);

	while (Uart2_Get_Pressed())
		;
	s_exit_inputs_ready = 1;

	for (;;)
	{
		int a1, a2, a3;

		Poll_App();

		if (s_emergency_mode && exit_mode) {
			exit_mode = 0;
			exit_move_high_streak = 0;
			exit_ms_left = 0;
			exit_decrement_done = 0;
		}

		if (s_exit_button_pending) {
			s_exit_button_pending = 0;
			if (s_emergency_mode)
				printf("EXIT: button ignored (flame emergency active)\n");
			else {
				Exit_Run_Door_Open_First();
				exit_mode = 1;
				exit_ms_left = DOOR_HOLD_MS;
				exit_move_high_streak = 0;
				exit_decrement_done = 0;
				printf("EXIT: armed (button PA6) — %u ms until close; IR PA1 for count-1 (no ESP32)...\n",
					DOOR_HOLD_MS);
			}
		}

		if (exit_mode) {
			unsigned int k;
			unsigned int hits = 0;

			Wait_Ms_Chunked(EXIT_MODE_TICK_MS);
			if (exit_ms_left >= EXIT_MODE_TICK_MS)
				exit_ms_left -= EXIT_MODE_TICK_MS;
			else
				exit_ms_left = 0;

			for (k = 0; k < EXIT_ADC_BURST; k++) {
				ADC1_Read_PA1_PA4_PC1(&a1, &a2, &a3);
				if (a1 >= MOVEMENT_ADC_THRESHOLD)
					hits++;
			}

			if (hits > 0u) {
				exit_move_high_streak++;
				if (exit_move_high_streak >= EXIT_MOVEMENT_TICKS
				    && !exit_decrement_done) {
					exit_decrement_done = 1;
					printf("EXIT: movement (A1=%d, %u/%u burst hits) — count-1 (no ESP32)\n",
						a1, hits, EXIT_ADC_BURST);
					if (s_person_count > 0u) {
						s_person_count--;
						printf("EXIT: person count=%u\n", s_person_count);
					} else
						printf("EXIT: person count stays 0\n");
				}
			} else
				exit_move_high_streak = 0;

			if (exit_mode && exit_ms_left == 0u) {
				exit_mode = 0;
				exit_move_high_streak = 0;
				exit_decrement_done = 0;
				printf("EXIT: %u ms after open — closing door (count=%u).\n",
					DOOR_HOLD_MS, s_person_count);
				StepMotor_DoorClose();
				Print_Door_Status();
				SysTick_Run(1000);
			}
		} else {

		if (SysTick_Check_Timeout()) {
			SysTick_Run(1000);
			ADC1_Read_PA1_PA4_PC1(&a1, &a2, &a3);
			printf("ADC IR(PA1)=%4d gas(PA4/A2)=%4d flame(PC1)=%4d\n",
				a1, a2, a3);
			Flame_Try_Enable_Baseline(a3);
		}

		if (!StepMotor_DoorIsOpen()) {
			ADC1_Read_PA1_PA4_PC1(&a1, &a2, &a3);
			Flame_Try_Enable_Baseline(a3);
			if (sensor_armed && a1 >= MOVEMENT_ADC_THRESHOLD) {
				move_high_streak++;
				if (move_high_streak >= MOVEMENT_CONFIRM_READS) {
					move_high_streak = 0;
					sensor_armed = 0;
					Run_Movement_Esp32_Sequence(&sensor_armed);
				}
			} else {
				move_high_streak = 0;
				if (!sensor_armed && a1 < MOVEMENT_ADC_THRESHOLD) {
					printf("AUTO: A1=%d below threshold — sensor re-armed.\n", a1);
					sensor_armed = 1;
				}
			}
		} else {
			move_high_streak = 0;
			if (!s_flame_baseline_ok) {
				ADC1_Read_PA1_PA4_PC1(&a1, &a2, &a3);
				Flame_Try_Enable_Baseline(a3);
			}
		}

		}

		c = Uart2_Get_Pressed();
		if ((c == 'e' || c == 'E') && s_exit_inputs_ready) {
			if (s_emergency_mode)
				printf("EXIT: UART ignored (flame emergency active)\n");
			else {
				Exit_Run_Door_Open_First();
				exit_mode = 1;
				exit_ms_left = DOOR_HOLD_MS;
				exit_move_high_streak = 0;
				exit_decrement_done = 0;
				printf("EXIT: armed (UART) — %u ms until close; IR PA1 for count-1 (no ESP32)...\n",
					DOOR_HOLD_MS);
			}
		} else if (c == '=') {
			StepMotor_DoorOpen();
			Print_Door_Status();
		} else if (c == '-') {
			StepMotor_DoorClose();
			Print_Door_Status();
		} else if (c == ' ' || c == 'r') {
			StepMotor_Release();
			printf("stepper: release\n");
			Print_Door_Status();
		} else if (c == '[') {
			if (step_delay_us < 19800u)
				step_delay_us += 200u;
			else
				step_delay_us = 20000u;
			StepMotor_SetStepDelayUs(step_delay_us);
			FirewallStepper_SetStepDelayUs(step_delay_us);
			printf("stepper: delay %u us\n", step_delay_us);
		} else if (c == ']') {
			if (step_delay_us > 300u)
				step_delay_us -= 200u;
			else
				step_delay_us = 100u;
			StepMotor_SetStepDelayUs(step_delay_us);
			FirewallStepper_SetStepDelayUs(step_delay_us);
			printf("stepper: delay %u us\n", step_delay_us);
		} else if (c == 'f') {
			printf("firewalls: opening both...\n");
			FirewallStepper_OpenBoth();
			printf("firewalls: FW1 %s FW2 %s\n",
				FirewallStepper_Fw1IsOpen() ? "OPEN" : "—",
				FirewallStepper_Fw2IsOpen() ? "OPEN" : "—");
		} else if (c == 'g') {
			printf("firewalls: closing both...\n");
			FirewallStepper_CloseBoth();
			printf("firewalls: closed\n");
		} else if (c == 'F') {
			FirewallStepper_Release();
			printf("firewalls: coils released\n");
		}
	}
}
