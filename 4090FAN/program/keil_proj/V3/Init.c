#include "STC8H.H"
#include "main.h"
void GPIO_Init(){
    P_SW2 |= 0X80;
    P0M0 = 0x00; P0M1 = 0xff; 
    P0IE = 0x00;//P0禁用数字使能
    //P10控制MP3431的EN，P11触摸按键，P13 SPI输出，P16P17彩灯电源
    //触摸按键会不会在关机后漏电？
    P1M0 = 0xc9; P1M1 = 0x36;
    P1IE = 0X02;//P11使能数字输入使能，其余全关
    P2M0 = 0x00; P2M1 = 0xff;
    P2IE = 0x00;
    //P30P31串口，P32检测充电，P33PWM输出，P34控制电源
    P3M0 = 0x0a; P3M1 = 0xf5;
    P3IE = 0X07; P3PU = 0X05;//P30P32打开上拉，串口RXD需打开上拉电阻，防止关机后漏电
    P4M0 = 0x00; P4M1 = 0xff;
    P4IE = 0X00;
    P5M0 = 0x00; P5M1 = 0xff; 
    P5IE = 0X00;
    TXD_PIN = 1;
    RXD_PIN = 1;
    MP3431_EN_PIN = 0;
    FAN_PWR_OFF;
    WS2812_PWR_OFF;
    PWM_PIN = 0;
}
void Interupt_Init(){
    P_SW2 |= 0X80;//允许访问扩展寄存器（设置上拉，转换速度等扩展寄存器）
    IT0 = 1;//外部中断0，下降沿中断
    IE = 0x8b;//开启定时器0,1中断，开启外部中断int0
    WDT_CONTR = 0X37;//初始化看门狗，时间设置为24MHZ时钟下4.2s
    IRCDB = 0X00;//上电后持续255个时钟后开始执行程序
}
void ADC_Init(void){
    ADC_CONTR = 0X8F;//打开ADC电源，清空中断标志，关闭PWM使能，选择15通道（默认）
    ADCCFG = 0X21;//ADC转换结果设置为右对齐，ADC时钟设置为系统时钟/4
    ADCTIM = 0X4A;//ADC通道时间占1个ADC时钟，通道保持3时钟，采样时间11个时钟。
}
void PWMA_Init(){
    P_SW2 |= 0X80;
    PWMA_PS |= 0XC0;//PWMA4N输出引脚为P33
    PWMA_CCER2 = 0x00; //写 CCMRx 前必须先清零 CCERx 关闭通道
    PWMA_CCMR4 = 0x60; //设置 CC4 为 PWMA 输出模式 
    PWMA_CCER2 = 0xC0; //使能 CC4 通道
    PWMA_IER = 0X00; //禁用中断
    PWMA_CCR4 = Duty; //设置比较值，PWM4//这里可以直接写入16位值
    PWMA_ARR = PWM_Period; //设置计数值//预装载寄存器
    PWMA_ENO = 0x80; //使能 PWM4N 端口输出 //占空比为反比
    PWMA_BKR = 0x80; //使能主输出
    PWMA_CR1 = 0x01; //开始计时
}
void Timer_Init(void){//@24MHZ 12T
    //V2.1版因为使用了SPI，CPU时钟设置到了24MHZ，定时器也快了四倍
    AUXR &= 0x3D;//定时器0，1 12T模式,定时器2为串口计时,允许访问扩展ram
    TCON = 0x01;//外部中断1,下降沿中断
    TMOD = 0X00;//十六位自动重载
    //定时器0 30ms一次中断 @24MHZ
    TL0 = 0xA0;//设置定时初始值
    TH0 = 0x15;//设置定时初始值
    //定时器1 5ms一次中断（仅为WS2812呼吸提供时序支持）为什么不用定时器0？因为要实现充电唤醒闪烁
    TL1 = 0xF0;//设置定时初始值
    TH1 = 0xD8;//设置定时初始值
    //清除中断标志
    TF0 = 0;TF1 = 0;
    //初始关闭
    TR0 = 0;TR1 = 0;
}
void UART1_Init(void){//115200bps@24.000MHz
    SCON = 0x50;	//8位数据,可变波特率，允许接受
    AUXR |= 0x01;//串口1选择定时器2为波特率发生器
    AUXR |= 0x04;//定时器时钟1T模式
    T2H = ((UART_Timer_Reload_Value >> 8) & 0xff);//设置定时初始值
    T2L = (UART_Timer_Reload_Value & 0xff);//设置定时初始值
    AUXR &= ~0X10;//定时器2初始化后停止计时，开机后才继续计时
}
void TouchKey_Init(){
    P_SW2 |= 0X80;//允许访问扩展寄存器
    TSCHEN1 = 0X02;//TK1，P11通道使能//如果有多个按键的话，最好在休眠后关闭除了开关机键以外的按键使能
    TSCFG1 = 0x26;//实测22nf下5000时钟放电已足够//测试发现4MHZ下工作的更好，返回值正常，推测可能是3MHZ下和触摸天线谐振了
    TSCFG2 = 0x03;//触摸按键控制器参考电压3/4 VCC
    TSSTA2 = 0XC0;//清零触摸按键状态寄存器(包括完成标志，溢出标志，以及完成的位号)
    TSWUTC = 0X0D;//休眠下0.1S唤醒一次触摸按键控制器//需打开低功耗唤醒功能
    TSRT = 0X00;//IO仅做触摸按键，不分时复用
    IP2H |= 0X80;
    
    TSCTRL = 0XA5;//TSGO = 1，直接开始扫描，等待手动清零TSIF，内部32K晶振，使能低功耗唤醒，重复扫描2次，关闭数字比较器//如果不关数字比较器会卡死
    Delay_ms(5);//延时5ms，等待触摸按键采样完成
    
    TK_P11_Data = TouchKey_Read();//读取按键
    TK1_Baseline = TK_P11_Data;//初始化基线
    TK1_Threshold = Touchkey_Threshold_CAL(TK1_Baseline);
}
    //实测不需要32k低速时钟，触摸模块内部就有32k低速时钟。与定时唤醒器一样，有独立的32k时钟
void RC_Wakeup_Init(){//通过低速RC振荡器，从休眠中定时唤醒
    IRC32KCR |= 0x80;//使能32K低速时钟，用于唤醒
    WKTCH = 0XFF;//16秒唤醒一次，启用定时唤醒功能
    WKTCL = 0XFE;
}