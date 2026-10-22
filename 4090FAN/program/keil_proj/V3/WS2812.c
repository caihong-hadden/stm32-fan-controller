#include "STC8H.H"
#include "main.h"

//HSV颜色空间转RGB
static void HSV2RGB(unsigned char h, unsigned char s, unsigned char v, unsigned char *r, unsigned char *g, unsigned char *b) {
    unsigned char region,p,q,t;
    unsigned int remainder;
    if (s == 0) { // 灰度
        *r = *g = *b = v;
        return;
    }
    region = h / 43; // 将 H 分为 6 个区域，每个区域约 43 度
    remainder = (h - (region * 43)) * 6; // 计算当前区域内的偏移量
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    switch (region) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}
//检测并通过WS2812显示电量
//////WS2812驱动//////
#define High_Code 0xf8//11111000
#define Zero_Code 0xC0//11000000
//开始发送时，SPIF = 0，结束发送后SPIF = 1;
static void SPI_Send_1_Byte(unsigned char SPI_Data){
    SPDAT = SPI_Data;//写入后，硬件置SPIF = 0;
    while(!(SPSTAT & 0x80));//SPI控制器发送完数据后，退出while循环
    SPSTAT = 0xC0;//清空中断标志
}
static void WS2812_Lightup(){
    //调用SPI_Send_1_Byte,发送80us的0x00，使灯珠亮起
    //80us/0.1667*8 = 60,循环发送40个0
    unsigned char i;
    for(i = 0;i < 40;i++){//循环40次
        SPI_Send_1_Byte(0x00);}
}
static void WS2812_PinSet(unsigned char Pin){
    //传入引脚，配置引脚为推挽
    if(Pin == 34){//STC8 MOSI
        P_SW1 |= 0X0C;//B3B2 = 1;
        SPCTL |= 0X10;//B4 MSTR = 1;
        P3IE &= 0XF7; //P33IE = 0;
        P3M0 |= 0x10; P3M1 &= ~0x10;}//P34推挽
    else if(Pin == 40){//MOSI
        P_SW1 |= 0X08;//B3 = 1;
        SPCTL |= 0X10;//MSTR = 1;
        P4IE &= 0XFD;//P41IE = 0;
        P4M0 |= 0x01; P4M1 &= ~0x01;}//P40推挽
    else if(Pin == 23){//MOSI
        P_SW1 |= 0X04;//B2 = 1;
        SPCTL |= 0X10;//MSTR = 1;
        P2IE &= 0XEF;//P24IE = 0;
        P2M0 |= 0x08; P2M1 &= ~0x08;}//P23推挽
    else if(Pin == 13){//MOSI
        P_SW1 &= 0XF3;//B3B2 = 0;
        SPCTL |= 0X10;//MSTR = 1;
        P1IE &= 0XEF;//B4 P14IE = 0
        P1M0 |= 0x08; P1M1 &= ~0x08;}//P13推挽
    else if(Pin == 54){//STC8G1K08A 8PIN MOSI
        P_SW1 &= 0XF3;//B3B2=0
        SPCTL |= 0X10;//B4 MSTR = 1,主机
        P3IE &= 0xF7;//关闭P33 MISO数字输入
        P5M0 |= 0x10; P5M1 &= ~0x10;}//P54推挽
}
void WS2812_Init(unsigned char Pin_Sel){//对于STC8G1K08A 8Pin需要执行不同的引脚初始化
    //初始化，并决定是否为主机从机
    P_SW1 &= 0XF3;//B3B2 = 0;
    P_SW2 |= 0X80;//允许访问扩展寄存器
    SPSTAT = 0XC0;//清零中断标志
    SPCTL = 0XD4;//SPEN = 1;SSIG = 1;先发高位;主机模式;时钟空闲低电平;后时钟沿采样;SPI CLK为sysclk/4;
    WS2812_PinSet(Pin_Sel);
}
void WS2812_Deinit(){//关闭SPI,但引脚解绑需要根据需求手动配置
    SPSTAT = 0XC0;
    SPCTL &= 0XBF;//SPEN = 0;
}
void WS2812_Send_Data(unsigned char WS2812_Num,unsigned char Red,unsigned char Green,unsigned char Blue){
    //发送数据顺序为GRB
    unsigned char i,j;
    unsigned char dat;
    for(j = 0;j < WS2812_Num;j++){
    //Green
    dat = Green;
    for(i = 0; i < 8; i++){
        if(dat & 0x80) SPI_Send_1_Byte(High_Code); // 提取最高位，判断发1码还是0码
        else           SPI_Send_1_Byte(Zero_Code);
        dat <<= 1; // 左移一位，准备发送下一位
    }
    //Red
    dat = Red;
    for(i = 0; i < 8; i++){
        if(dat & 0x80) SPI_Send_1_Byte(High_Code);
        else           SPI_Send_1_Byte(Zero_Code);
        dat <<= 1;
    }
    //Blue
    dat = Blue;
    for(i = 0; i < 8; i++){
        if(dat & 0x80) SPI_Send_1_Byte(High_Code);
        else           SPI_Send_1_Byte(Zero_Code);
        dat <<= 1;
    }
    }
    WS2812_Lightup();
}
//////LED颜色显示//////
static const unsigned char code POW_Table[256] = {//255 * pow(k,1.8) k∈0~1的结果
0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,2,
2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,6,
6,6,7,7,8,8,8,9,9,10,10,10,11,11,12,12,
13,13,14,14,15,15,16,16,17,17,18,18,19,19,20,21,
21,22,22,23,24,24,25,26,26,27,28,28,29,30,30,31,
32,32,33,34,35,35,36,37,38,38,39,40,41,41,42,43,
44,45,46,46,47,48,49,50,51,52,53,53,54,55,56,57,
58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,
74,75,76,77,78,79,80,81,82,83,84,86,87,88,89,90,
91,92,93,95,96,97,98,99,100,102,103,104,105,107,108,109,
110,111,113,114,115,116,118,119,120,122,123,124,126,127,128,129,
131,132,134,135,136,138,139,140,142,143,145,146,147,149,150,152,
153,154,156,157,159,160,162,163,165,166,168,169,171,172,174,175,
177,178,180,181,183,184,186,188,189,191,192,194,195,197,199,200,
202,204,205,207,208,210,212,213,215,217,218,220,222,224,225,227,
229,230,232,234,236,237,239,241,243,244,246,248,250,251,253,255
};
volatile unsigned char Breath_State = Brt_Rise;//呼吸模式
volatile unsigned int Breath_Counter = 0;//呼吸计数器，实现呼吸计数
#define Bat_Max_mV 4200
#define Bat_Min_mV 2750
static void WS2812_Brt_Breath(unsigned char Red,unsigned char Green,unsigned char Blue){//30ms时序调用
    unsigned int Redin = Red;
    unsigned int Greenin = Green;//转换为int量防止溢出
    unsigned int Brt_Ratio = (255 * 2);//最大亮度127
    static unsigned char Redout,Greenout;//在亮度保持状态，退出函数不会丢失
    switch(Breath_State){
        case Brt_Rise://亮度增加状态
            Redout = (Redin * Breath_Counter)/Brt_Ratio;
            Greenout = (Greenin * Breath_Counter)/Brt_Ratio;
            if(++Breath_Counter == 255){
                Breath_State = Brt_Keep_High;
                Breath_Counter = 0;//清空计数器
            }
            break;
        case Brt_Keep_High:
            if(++Breath_Counter >= 510){
                Breath_State = Brt_Fall;
                Breath_Counter = 255;//下面会使用这个来计算亮度，因此先赋值
            }
            break;
        case Brt_Fall:
            Redout = (Redin * Breath_Counter)/Brt_Ratio;
            Greenout = (Greenin * Breath_Counter)/Brt_Ratio;
            if(--Breath_Counter == 0){
                Breath_State = Brt_Keep_Low;
                Breath_Counter = 0;
            }
            break;
        case Brt_Keep_Low:
            if(++Breath_Counter >= 365){
                Breath_State = Brt_Rise;
                Breath_Counter = 0;
            }
            break;
    }
    WS2812_Send_Data(1, Redout, Greenout, Blue);
}
static void WS2812_Display_BatVoltage(unsigned int BatVol){//以定时器中断为时基调用
    static unsigned int Range = Bat_Max_mV - Bat_Min_mV;
    unsigned char Red,Green,Blue;
    unsigned char k;
    if (BatVol > Bat_Max_mV) BatVol = Bat_Max_mV;//如果电压超过最大或最小，则削顶
    else if(BatVol < Bat_Min_mV) BatVol = Bat_Min_mV;
    k = POW_Table[(unsigned char)((((unsigned long)(BatVol - Bat_Min_mV)) * 255) / Range)];//计算电量比例，查对数表
    Green = k;
    Red = 255 - k;
    Blue  = 0;
    WS2812_Brt_Breath(Red,Green,Blue);
}
#define WS2812_Normal 0
#define WS2812_Tune_Duty 1
#define WS2812_Max_Level 2
void WS2812_State_Machine(){//ws2812状态机，用于指示多种功能状态
    //实现什么功能？常态下呼吸显示电量，微调占空比时通过颜色显示当前占空比值，当风速最大时显示紫色
    unsigned char WS2812_State = WS2812_Normal;
    unsigned char Red,Green,Blue;
    if(Duty_Level >= 10){//共8档//这个在第一次烧录程序复位后会因为读取到0xff而变紫，是正常的
        WS2812_State = WS2812_Max_Level;}
    else if(Fine_Tune_Inc || Fine_Tune_Dec){
        WS2812_State = WS2812_Tune_Duty;}
    else{
        WS2812_State = WS2812_Normal;}
    switch(WS2812_State){
        case WS2812_Normal:
            WS2812_Display_BatVoltage(vbat);//单位mv
        break;
        case WS2812_Tune_Duty://亮度127，防止刺眼，饱和度255，DUTY决定显示什么颜色
            HSV2RGB((Duty + 2)/4,255,127,&Red,&Green,&Blue);//填入H,S,V,R,G,B
            WS2812_Send_Data(1,Red,Green,Blue);
            break;
        case WS2812_Max_Level:
            HSV2RGB(242,255,127,&Red,&Green,&Blue);//显示紫色
            WS2812_Send_Data(1,Red,Green,Blue);
            break;
    }
}
