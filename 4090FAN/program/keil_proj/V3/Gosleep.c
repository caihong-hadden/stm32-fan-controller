#include "STC8H.H"
#include "main.h"

volatile bit was_enter_sleep = 0;//
volatile bit need_wakeup_sampling = 0;//被外部中断唤醒，需要进行一次采样

void PWR_CTRL(bit CTRL){
    if(CTRL){
        Touchkey_Normal_Set;//触摸按键关闭中断，自动扫描，等待清零标志，使能低功耗，扫描2次
        WS2812_PWR_ON;//WS2812电源开启
        FAN_PWR_ON;//输出端MOS开启
        MP3431_EN_PIN = 1;//MP3431升压使能
        PWMA_CR1 |= 0X01;//PWM开始输出
        Delay_ms(5);
        TR0 = 1; TR1 = 1; AUXR |= 0X10;//定时器012开启计时
    }
    else{
        WS2812_PWR_OFF;
        FAN_PWR_OFF;//输出端MOS关闭
        MP3431_EN_PIN = 0;//MP3431关闭
        PWMA_CR1 &= ~0X01;//PWMA停止计数
        Delay_ms(1);//延时一段时间，确保PWMA停止，否则引脚电平会不确定
        PWM_PIN = 0;//PWM引脚输出低电平
        WS2812_SPI_PIN = 0;//SPI引脚低电平
        TR0 = 0; TR1 = 0; AUXR &= ~0X10;//定时器012停止计时
        Touchkey_Sleep_Set;//触摸按键睡眠状态，启用定时唤醒，启用数字比较器，内部32K晶振
    }
}

/////进入休眠//////
volatile bit Back_Sleep = 0;
#define Backsleep_Time 60 //60次计数后睡眠
#define Button TK_P11_Pressed//触摸按键P11的状态
void If_Back_Sleep(void){
    static unsigned char Sleep_Counter = 0;//睡眠计时器，用于避免误触唤醒
    if(Button){//只要按键按下
        Sleep_Counter = 0;
        Back_Sleep = 0;
    }
    if(!Charging && !poweron && (Sleep_Counter <= Backsleep_Time)){//误唤醒超时检测
        Sleep_Counter++;}
    else if(!Charging && !poweron && (Sleep_Counter > Backsleep_Time)){
        Sleep_Counter = 0;//超过1800ms，清空状态，重新睡眠
        Back_Sleep = 1;
    }
    else{
        Sleep_Counter = 0;
        Back_Sleep = 0;
    }
}
void Enter_Sleep(void){
    //从这里开始进入休眠
    if(System_OK){
        Save_Set_To_EEPROM(Save_Addr,vbat,Duty_Level);}//如果EEPROM中的数据不是复位值，保存当前占档位
    else if(Read_Set_From_EEPROM(Save_Dutylevel_Addr)  == 0XFFFF){//如果SystemOK=0(即复位后还未执行主循环令ok变1)，且EEPROM里是复位值
        Save_Set_To_EEPROM(Save_Addr,vbat,Duty_Level);}//不过这里是往16位空间里存8位变量，这个地址正常情况只可能是00
    while(1){//外层while
        TR0 = 0;TR1 = 0;//避免中断干扰
        WS2812_Send_Data(1,0,0,0);//给WS2812断电前清空WS2812的寄存器，防止上电之后闪烁
        Duty_Level = 0;//设置占空比为最低
        TK_Write_Th(TK1_Baseline);//休眠前，对触摸按键写入唤醒阈值
        Breath_State = Brt_Rise;Breath_Counter = 0;//呼吸模式,呼吸计数器,用于实现开机后彩灯亮度从0呼吸
        Serial_Printstrln("MCU Shutdown");//阻断式发送
        PWR_CTRL(PWR_OFF);
        Touchkey_Sleep_Set;//触摸按键睡眠状态，启动
        PCON |= 0X02;//进入掉电模式
/////唤醒后从这里继续执行/////
        _nop_();_nop_();_nop_();_nop_();//空指令，避免CPU上电后的不稳定
        TR0 = 1;TR1 = 1;AUXR |= 0X10;//打开定时器0,1,2,开始计时，执行按键检测
        //这里应该添加打开触摸按键的代码，低功耗唤醒后也无法读取按键
        if(WKUP_Handler() == 2){//先检测是否由触摸按键唤醒，再进入常规等待开机流程
            continue;//返回while起点
        }
        Serial_Printstrln("System Waiting Start");
        Touchkey_Normal_Set;//触摸按键禁用中断，TSCTRL = 0XA5
        Delay_ms(5);//在设置触摸按键启动后，等待5ms再读取结果
        while(!poweron){//内层while
            WDT_CONTR |= 0X10;//喂狗
            if(Charging && !poweron){//只有在充电且未开机的情况下，才会返回
                TR1 = 1;//打开定时器1，才能让WS2812彩灯呼吸
                WS2812_PWR_ON;//打开彩灯电源//虽然这里是重复置1，但也无所谓了
                if(Timer0_Sync){
                    //修复：如果通过充电中断唤醒，然后又关机，再触摸按键开机，会导致定时器1关闭，也就失去了显示电量的功能，不过这不是什么严重bug
                    Timer0_Sync = 0;
                    VBAT_CAL();//如果在睡眠状态唤醒，在这里检测电池电压
                    Key_Sampling();
                   Key_Handler();
                }
                if(Timer1_Sync){
                    Timer1_Sync = 0;
                    WS2812_State_Machine();
                    TouchKey_Handler();//扫描触摸按键
                }
                continue;//如果P32 == 0;则直接返回到循环起点，不重新休眠
            }
            if(Timer0_Sync){//这里由于是在一个IF里面的，不会连下面那个定时器1的都执行，不过时间上是错开的，没事
                Timer0_Sync = 0;
                Key_Sampling();
                Key_Handler();
            }
            if(Timer1_Sync){
                Timer1_Sync = 0;
                TouchKey_Handler();//扫描触摸按键，保证能够唤醒
            }
            if(Back_Sleep){
                Back_Sleep = 0;
                break;//跳出内层while，重新执行休眠。
            }
        }
        if(poweron){
            break;//跳出外层while
        }
    }
/////从这里开始执行恢复程序/////
    PWR_CTRL(PWR_ON);
    vbat = Read_Set_From_EEPROM(Save_VBAT_Addr);
    Duty_Level = Read_Set_From_EEPROM(Save_Dutylevel_Addr);
    Duty = Duty_Table[Duty_Level];
    Duty_Reg = (1000U - Duty);
    Serial_Printstrln("MCU Start");
    Serial_Printstrln("Read EEPROM result is");
    Serial_Printstr("VBAT:");
    Serial_Printnumln(vbat);
    Serial_Printstr("DutyLevel:");
    Serial_Printnumln(Duty_Level);
    Serial_Printstr("TK CMP Value:");
    Serial_Printnumln(((TSTH01H << 8) | TSTH01L));
}