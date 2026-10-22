#ifndef __MAIN_H__
#define __MAIN_H__
#include "STC8H.H"

#define Main_FOSC 24000000UL//24MHZ
#define Baud_Rate 1000000UL
#define UART_Timer_Reload_Value (65536UL - (Main_FOSC / (4UL * Baud_Rate)))
#define SAVE_SET_ADDR 0x0000;//高级定时器触发值
#define PWM_Period 1000U//高级定时器周期值

void Delay_ms(unsigned int ms);
void VBAT_CAL(void);
void Key_Handler(void);
void Adjust_Duty(void);
void WS2812_State_Machine(void);
void TouchKey_Init(void);
void TouchKey_Handler(void);
void TouchKey_Read(void);
void Init_Systick(void);
void Init_UART1(void);
void Init_PWM(void);
void Init_EEPROM(void);

extern volatile unsigned int vbat;
extern volatile unsigned char Duty_Level;
extern volatile unsigned int Duty;
extern volatile unsigned int Target_Duty;

#endif
