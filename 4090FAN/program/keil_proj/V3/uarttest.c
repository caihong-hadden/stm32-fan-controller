#include "STC8H.H"
void UartIsr() interrupt 4{
    TI = 0;
    P1 = 0X00;
}
void Uart1_Init(void){//115200bps@24.000MHz
    SCON = 0x50;	//8位数据,可变波特率
    AUXR |= 0x01;//串口1选择定时器2为波特率发生器
    AUXR |= 0x04;//定时器时钟1T模式
    T2L = 0xCC;//设置定时初始值
    T2H = 0xFF;//设置定时初始值
    AUXR |= 0x10;//定时器2开始计时
}
int main(){
    P_SW2 |= 0X80;
    IE |= 0X90;
    P1M0 = 0xff; P1M1 = 0xff; 
    Uart1_Init();
    TI = 1;//尝试软件触发串口中断//成功，写1触发中断
    while(1);
}