#include "STC8H.H"
#include "main.h"

volatile bit poweron = 1;
volatile bit System_OK = 0;
volatile bit Timer0_Sync = 1;
volatile bit Timer1_Sync = 1;

void Delay_ms(uint16_t ms){//24MHZ
    //看门狗定时器无法软件关闭，因此循环时间不要超过看门狗复位时间
    uint8_t data i, j;
    while(--ms){
        _nop_();
        i = 32;
        j = 40;
        do{
            while (--j);
        } while (--i);
    }
}
//26800
void main(){
    Intterupt_Init();//打开总中断
    Timer_Init();
    //ADC_Init();
    GPIO_Init();
    UART1_Init();
    TR0 = 0;TR1 = 0;
    //PWMA_Init();
    TouchKey_Init();
    RC_Wakeup_Init();
    //WS2812_Init(13);
    PWR_CTRL(PWR_ON);
    TSCTRL &= ~0X80;
    TSCTRL = 0X0D;
    while(1){
        //Serial_Printstrln("Now Sleep");
        //PWR_CTRL(PWR_OFF);
        
        PCON |= 0X02;
        TSSTA2 |= 0X80;
        //PWR_CTRL(PWR_ON);//成功
        Serial_Printnumln((TSDATH << 8) | TSDATL);
        Serial_Printnumln((TSTH01H << 8) | TSTH01L);
        //Serial_Printstrln("Now Wakeup");
        //Serial_Printnumln(TSSTA2);
        //Serial_Printnumln((TSDATH << 8) | TSDATL);
        //Serial_Printstrln("Normal Working");
        //System_OK = 1;
        WDT_CONTR |= 0X10;
    }
}
void INT0_ROUTINE(void) interrupt 0 {
    //仅用于休眠下的充电唤醒
}