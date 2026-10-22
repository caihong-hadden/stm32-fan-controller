#include "main.h"

volatile unsigned int TK_P11_Data = 0;
volatile unsigned int TK1_Baseline = 27000;
volatile unsigned int TK1_Threshold = 26000;
volatile unsigned char TK_Status = TK_Idle;
volatile bit Timeout_Flag = 0;

void TouchKey_Handler(void){
    switch(TK_Status){
        default: break;
        case TK_Idle:
            if(Timeout_Flag){
                static unsigned int timeout_delay_cnt = 0;
                if(++timeout_delay_cnt >= 200){ // 200 * 5ms = 1000ms
                    timeout_delay_cnt = 0;
                    Timeout_Flag = 0;
                }
            }
            if(TK_P11_Data < TK1_Threshold) TK_Status = TK_Pressed;
            break;
        case TK_Pressed:
            if(TK_P11_Data > TK1_Threshold) TK_Status = TK_Released;
            break;
        case TK_Released:
            if(TK_P11_Data > TK1_Threshold) TK_Status = TK_Idle;
            break;
    }
}

void TouchKey_Read(void){
    if(!(TSSTA2 & 0x80)){
        // TK sampling interval too short
        return;
    }
    TK_P11_Data = TSDATA_H << 8 | TSDATA_L;
}
