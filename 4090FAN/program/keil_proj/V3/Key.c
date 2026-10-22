#include "main.h"

#define Duty_Reg PWMA_CCR4

volatile unsigned int Duty = 100;
volatile unsigned int Actual_Duty = 100; 
volatile unsigned char Duty_Level = 1;
volatile unsigned char Is_Max_Speed = 0;
volatile unsigned char Saved_Duty_Level = 1;
volatile bit Fine_Tune_Inc = 0;
volatile bit Fine_Tune_Dec = 0;
volatile unsigned char Key_Event = NOKEY;
volatile bit Button = 1;

unsigned int Duty_Table[11] = {0, 85, 120, 180, 240, 300, 360, 420, 500, 600, 685};

void Adjust_Duty(void){
    if (Button) { 
        Fine_Tune_Inc = 0;
        Fine_Tune_Dec = 0;
    }
    if (Fine_Tune_Inc) {
        Duty = (Duty < 1000U) ? Duty + 5 : 1000U;
    } else if (Fine_Tune_Dec) {
        Duty = (Duty > 0) ? Duty - 5 : 0;
    }

    // PWM 软启动 (Ramping)
    if (Actual_Duty < Duty) {
        Actual_Duty = (Actual_Duty + 10 > Duty) ? Duty : Actual_Duty + 10;
    } else if (Actual_Duty > Duty) {
        Actual_Duty = (Actual_Duty < 10) ? Duty : Actual_Duty - 10;
    }

    Duty_Reg = (1000U - Actual_Duty); 
}

void Key_Handler(void){
    // 按键事件处理逻辑 (省略具体实现以聚焦于类型修复，保留原逻辑)
    if(Key_Event == SINGLEKEY){
        Duty_Level = (Duty_Level % 10) + 1;
        Target_Duty = Duty_Table[Duty_Level];
        Key_Event = NOKEY;
    } else if(Key_Event == DOUBLEKEY){
        if(Duty_Level == 1) Duty_Level = 10; else Duty_Level--;
        Target_Duty = Duty_Table[Duty_Level];
        Key_Event = NOKEY;
    } else if(Key_Event == TRIPLEKEY){
        if(!Is_Max_Speed){
            Saved_Duty_Level = Duty_Level;
            Duty_Level = 10;
            Target_Duty = Duty_Table[10];
            Is_Max_Speed = 1;
        }else{
            Duty_Level = Saved_Duty_Level;
            Target_Duty = Duty_Table[Duty_Level];
            Is_Max_Speed = 0;
        }
        Key_Event = NOKEY;
    } else if(Key_Event == LONGKEY){
        // Power toggle logic
        Key_Event = NOKEY;
    }
}
