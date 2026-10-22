#include "main.h"
#include "WS2812.h"

void Delay_ms(unsigned int ms){
    unsigned int i, j;
    for(i=0; i<ms; i++){
        for(j=0; j<1000; j++); // 粗略延时
    }
}

void main(void){
    Init_Systick();
    Init_UART1();
    Init_PWM();
    Init_EEPROM();
    TouchKey_Init();

    while(1){
        // 主循环保持空，逻辑在定时器中断中执行
    }
}

void Timer0_Routine(void) interrupt 1{
    static unsigned char count = 0;
    count++;
    if(count >= 33){ // 约1秒 (30ms * 33)
        count = 0;
        // 状态上报
        // 这里建议使用自定义的 UART 打印函数
    }
    VBAT_CAL();
    Key_Handler();
    Adjust_Duty();
}
