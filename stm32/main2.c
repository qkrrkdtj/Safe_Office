#include "device_driver.h"
#include <stdio.h>

#define MOVEMENT_ADC_THRESHOLD      2000
#define DOOR_HOLD_MS                3000u
#define STARTUP_IGNORE_MS           5000u
#define MOVEMENT_CONFIRM_READS      2u

static void Delay(volatile int count)
{
    while (count--);
}

static void Sys_Init(int baud)
{
    SCB->CPACR |= (0x3 << 10*2) | (0x3 << 11*2);
    Clock_Init();
    Uart2_Init(baud);   // PC 출력용
    Uart1_Init(baud);   // ESP32-CAM 통신용
    setvbuf(stdout, NULL, _IONBF, 0);
    LED_Init();
}

static void Print_Door_Status(void)
{
    printf("door: %s\r\n", StepMotor_DoorIsOpen() ? "OPEN" : "CLOSED");
}

/* =========================
   SysTick 기반 ms 대기
   ========================= */
static void Wait_Ms_Chunked(unsigned int ms)
{
    unsigned int tpm = (unsigned int)(HCLK / (8. * 1000.) + 0.5);
    unsigned int chunk = (tpm > 0u) ? (16777215u / tpm) : 1000u;

    if (chunk == 0u || chunk > 1000u)
        chunk = 1000u;

    while (ms > 0u) {
        unsigned int c = ms > chunk ? chunk : ms;

        SysTick_Run(c);
        while (!SysTick_Check_Timeout())
            __asm volatile ("" ::: "memory");
        ms -= c;
    }
}

static void IR_ADC_Init(void)
{
    ADC1_PA1_PA4_PB0_Init();
}

static int IR_ADC_Read(void)
{
    int a1, a2, a3;
    ADC1_Read_PA1_PA4_PB0(&a1, &a2, &a3);
    return a1;
}

static int IR_Is_Detected(void)
{
    int value = IR_ADC_Read();

    if (value >= MOVEMENT_ADC_THRESHOLD)
        return 1;
    else
        return 0;
}

/* =========================
   문 열기 -> 대기 -> 문 닫기
   ========================= */
static void Auto_Door_Open_Close(void)
{
    printf("AUTO: opening door...\r\n");
    StepMotor_DoorOpen();
    printf("AUTO: open motion finished.\r\n");
    Print_Door_Status();

    printf("AUTO: holding open for %u ms...\r\n", DOOR_HOLD_MS);
    Wait_Ms_Chunked(DOOR_HOLD_MS);

    printf("AUTO: closing door...\r\n");
    StepMotor_DoorClose();
    printf("AUTO: close motion finished.\r\n");
    Print_Door_Status();

    SysTick_Run(1000);
}

static void Uart1_ReadLine(char *buf, int max_len)
{
    int i = 0;
    char ch;

    while (i < max_len - 1)
    {
        ch = Uart1_Get_Char();

        if (ch == '\n' || ch == '\r')
            break;

        buf[i++] = ch;
    }

    buf[i] = '\0';
}

static int Str_To_Int(const char *s)
{
    int value = 0;
    int sign = 1;

    if (*s == '-')
    {
        sign = -1;
        s++;
    }

    while (*s)
    {
        if (*s < '0' || *s > '9')
            break;

        value = value * 10 + (*s - '0');
        s++;
    }

    return value * sign;
}

void Main(void)
{
    char c;
    int prev_ir = 0;
    int move_high_streak = 0;
    int sensor_armed = 1;
    unsigned int step_delay_us = 2100u;

    int people_count = 0;
    char rx_buf[16];

    Sys_Init(115200);

    StepMotor_Init();
    StepMotor_SetStepDelayUs(step_delay_us);

    IR_ADC_Init();

    printf("STM32 UART1 <-> ESP32-CAM + AUTO DOOR START\r\n");
    printf("IR threshold: %d\r\n", MOVEMENT_ADC_THRESHOLD);
    printf("Ignore IR input for first %u ms...\r\n", STARTUP_IGNORE_MS);
    printf("Door hold: %u ms\r\n", DOOR_HOLD_MS);
    Print_Door_Status();

    Wait_Ms_Chunked(STARTUP_IGNORE_MS);
    printf("IR sensor active now.\r\n");

    for (;;)
    {
        int cur_ir = IR_Is_Detected();

        if (!StepMotor_DoorIsOpen()) {
            if (sensor_armed && cur_ir) {
                move_high_streak++;
            } else {
                move_high_streak = 0;
            }
        } else {
            move_high_streak = 0;
        }

        /* 연속 감지 확인 후 ESP32에 판별 요청 */
        if (!StepMotor_DoorIsOpen() &&
            sensor_armed &&
            move_high_streak >= MOVEMENT_CONFIRM_READS)
        {
            Delay(200000);

            if (IR_Is_Detected())
            {
                printf("IR DETECTED\r\n");
                printf("A1 = %d\r\n", IR_ADC_Read());
                printf("SEND: HELLO\r\n");

                Uart1_Send_String("HELLO\n");

                Uart1_ReadLine(rx_buf, sizeof(rx_buf));
                people_count += Str_To_Int(rx_buf);

                printf("RECV COUNT: %d\r\n", people_count);

                if (people_count > 0)
                {
                    sensor_armed = 0;
                    move_high_streak = 0;

                    Auto_Door_Open_Close();
                }
                else if (people_count == 0)
                {
                    move_high_streak = 0;
                }
                else
                {
                    printf("RECV FAIL\r\n");
                    move_high_streak = 0;
                }

                /* 감지 유지 중에는 재트리거 방지 */
                while (IR_Is_Detected())
                {
                    Wait_Ms_Chunked(100);
                }

                Delay(200000);
                prev_ir = 0;
                sensor_armed = 1;
                continue;

                printf("Count: %d\r\n", people_count);
            }
        }

        if (!sensor_armed && !cur_ir)
            sensor_armed = 1;

        prev_ir = cur_ir;

        /* 수동 제어 유지 */
        c = Uart2_Get_Pressed();
        if (c == '=') {
            StepMotor_DoorOpen();
            Print_Door_Status();
        } else if (c == '-') {
            StepMotor_DoorClose();
            Print_Door_Status();
        } else if (c == ' ' || c == 'r') {
            StepMotor_Release();
            printf("stepper: release\r\n");
            Print_Door_Status();
        } else if (c == '[') {
            if (step_delay_us < 19800u)
                step_delay_us += 200u;
            else
                step_delay_us = 20000u;
            StepMotor_SetStepDelayUs(step_delay_us);
            printf("stepper: delay %u us\r\n", step_delay_us);
        } else if (c == ']') {
            if (step_delay_us > 300u)
                step_delay_us -= 200u;
            else
                step_delay_us = 100u;
            StepMotor_SetStepDelayUs(step_delay_us);
            printf("stepper: delay %u us\r\n", step_delay_us);
        }
        printf("IR A1 = %d\r\n", IR_ADC_Read());
        Wait_Ms_Chunked(1000);
    }
}