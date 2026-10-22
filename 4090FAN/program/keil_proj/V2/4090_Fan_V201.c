//V2.1版，修改如下
/*
P54 CHRG→SPI MOSI
P55 POWERON→WS2812PWR
P33 PWMPIN→POWERON
P31 LEDPIN→PWMPIN
P30 LEDPIN→CHRG
电源控制改为直接控制BOOST控制器EN脚，开机后拉高EN
灯光改为SK6812

*/
#include "STC8G.H"
#include "intrins.H"
#include "HSV2RGB.H"
#include "STC8_WS2812.H"
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
/////初始化/////
void Interrupt_Init(void){//初始化中断
    P_SW2 |= 0X80;//允许访问扩展寄存器（设置上拉，转换速度等扩展寄存器）
    IT0 = 1;//外部中断0，下降沿中断
    IE = 0x8b;//开启定时器0,1中断，开启外部中断int0
    INTCLKO |= 0x40;//开启外部中断int4(仅支持下降沿)
    WDT_CONTR = 0X37;//初始化看门狗，时间设置为6M时钟下4.2s，喂狗在主循环添加0X35
    IRCDB = 0X00;//上电后持续255个时钟后开始执行程序
}
#define Charging (!P30)
void IO_Config(void){
    //IO速度与上拉能力均为默认
    //IO模式设置
    // P3配置: 
    // P3.0: CHRG检测，高阻启用上拉(INT4)
    // P3.1: PWM输出，推挽
    // P3.2: 高阻(按键),启用上拉
    // P3.3：悬空
    // 其余 高阻关闭数字输入
    P3M0 = 0x02; P3M1 = 0xfd; P3IE = 0x05;P3PU = 0x05;
    // P5.4: 推挽，MOSI输出
    // P5.5: 推挽，电源控制(PMOS)
    // 其余: 高阻关闭数字输入
    P5M0 = 0x30; P5M1 = 0xcf; P5IE = 0x00;P5PU = 0x00;
    //规定引脚初始电平//
    P31 = 0;//PWM初始低电平
    P55 = 1;//关闭13.5V输出
    P54 = 0;//MOSI = 0;
    //引脚中断设置//
}
void PCA_Init(void){//PCA模块设置
    P_SW1 = 0X10;   //PCA功能脚位切换为P55P31P33P54
    CCON = 0x00;    //复位PCA
    CMOD = 0x0A;    //PCA时钟为系统时钟/4
    CL = 0x00;      //复位低字节
    CH = 0x00;      //复位高字节
    CCAPM0 = 0x42;  //PCA0 PWM模式
    PCA_PWM0 = 0x00;//PWM0,8位PWM模式
    CCAP0H = 0XFF;//初始占空比设置
    CCAP0L = 0XFF;//初始输出低电平
    //CLCH为一个16位寄存器。8位PWM模式下，CL>=CCAP1L,输出高电平
    //在第一次CL溢出之后自动将CCAP1H的值装载到CCAP1L，无需设置CCAP1L
    CR = 0;//默认关闭PWM输出。
}
void Timer_Init(void){//@24MHZ 12T
    //V2.1版因为使用了SPI，CPU时钟设置到了24MHZ，定时器也快了四倍
    AUXR &= 0x7F;//定时器0，1 12T模式
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
void PWR_ON(void){
    CCAPM0 = 0X42;
    //ADC_CONTR |= 0X80;//打开ADC电源
    CR = 1;
    TR0 = 1;TR1 = 1;
    P55 = 0;
    //其余引脚均由硬件控制
}
void PWR_OFF(void){
    CCAPM0 = 0X00;
    //ADC_CONTR &= ~0X80;//关闭ADC电源
    //实际测试发现在休眠状态即使开启ADC电源，耗电也没有区别，说明实际上ADC电源在休眠下是一同关闭的
    CR = 0;
    P31 = 0;//PWM低电平
    P54 = 0;//MOSI＝0
    P55 = 1;//关闭电源
}
////////////ADC///////////////
#define BGV_Channel 0X0F//ADC 15通道带隙电压
volatile unsigned long BGV = 1100;//防止读取失败，保底值(long变量是因为后面计算乘1000会超过int限制)
volatile unsigned int vbat = 4200U;
void ADC_Init(void){
    ADC_CONTR = 0X8F;//打开ADC电源，清空中断标志，关闭PWM使能，选择15通道（默认）
    ADCCFG = 0X21;//ADC转换结果设置为右对齐，ADC时钟设置为系统时钟/4
    ADCTIM = 0X4A;//ADC通道时间占1个ADC时钟，通道保持3时钟，采样时间11个时钟。
}
unsigned int BGV_Read(void){
    //不需要开启添加重要测试参数
    return *((unsigned int idata *)0xEF);
}
unsigned int ADC_Sampling(unsigned char channel){
    unsigned int A;
    ADC_CONTR = 0X80 | (channel & 0X0F);//切换ADC通道
    ADC_CONTR |= 0x40;       // 启动ADC转换
    _nop_(); _nop_(); _nop_(); _nop_(); // 短暂延时
    while (!(ADC_CONTR & 0x20)); // 等待转换完成
    ADC_CONTR &= ~0x20;      // 清除完成标志
    A = (ADC_RES << 8) | ADC_RESL; // 合并高8位和低8位
    return A;
}
unsigned int Code_Per_Volt(void){//计算ADC毫伏每码值
    unsigned int A,B;
    A = ADC_Sampling(BGV_Channel);
    if(BGV == 1100){//只读1次
        BGV = BGV_Read();}
    //计算BGV/RESULT_BGV即可得到ADC的毫伏每码比值
    if(A > 0){//防止ADC无回报
        B = (1000UL * BGV / A);//乘以1000防止丢失小数部分
    }//bgv为1190左右的值，x1000后超过16位，因此BGV为ULONG
    return B;
}
///////电量处理///////
//对应6种LED状态，色温与闪烁共同表示电量
#define BAT_FULL     0  // ≥4000 mV — 紫色常亮
#define BAT_HIGH     1  // ≥3800 mV — 紫色闪烁
#define BAT_MEDIUM   2  // ≥3600 mV — 蓝色常亮
#define BAT_LOW      3  // ≥3400 mV — 蓝色闪烁
#define BAT_CRITICAL 4  // ≥3000 mV — 红色常亮
#define BAT_DRAINED  5  // <3000 mV — 红色闪烁
#define BAT_DEAD     6  // <2750 mv — 直接关机
unsigned char BAT_Status = BAT_FULL;
void BAT_Handler(void){//判断电池电量状态，无迟滞
    if(vbat >= 4000){
        BAT_Status = BAT_FULL;}
    else if(vbat >= 3800){
        BAT_Status = BAT_HIGH;}
    else if(vbat >= 3600){
        BAT_Status = BAT_MEDIUM;}
    else if(vbat >= 3400){
        BAT_Status = BAT_LOW;}
    else if(vbat >= 3000){
        BAT_Status = BAT_CRITICAL;}
    else if(vbat >= 2750){
        BAT_Status = BAT_DRAINED;}
    else if(vbat <2750){
        BAT_Status = BAT_DEAD;
        poweron = 0;}//进入睡眠(不需要延时来闪烁了)
}
void VBAT_CAL(void){//计算电池电压，单位毫伏
    //30ms定时器中调用，每300ms更新一次电池电压
    static signed char k = -1;//初始为位-1，因为if前置自增
    unsigned char i,j;//冒泡排序所需
    unsigned int temp = 0;//冒泡排序缓存
    static unsigned long vbat_Buffer [10] = {0};//32位数组
    unsigned int vbat_raw = ((1024UL * Code_Per_Volt()) / 1000UL);//除以1000
    if(++k >= 10){//前置自增
        k = -1;//t前置自增，t=9的下一次调用为10，数组正好填满0~9,总共10位
        //进行冒泡排序（升序）
        for (j = 0; j < 9; j++) {
            for (i = 0; i < 9 - j; i++) {
                if (vbat_Buffer[i] > vbat_Buffer[i + 1]) {
                    temp = vbat_Buffer[i];
                    vbat_Buffer[i] = vbat_Buffer[i + 1];
                    vbat_Buffer[i + 1] = temp;
                }
        }//32位数组，运算过程中间值也是32位的
        }//取中间四位，求平均
        vbat = (vbat_Buffer[3] + vbat_Buffer[4] +vbat_Buffer[5] + vbat_Buffer[6])/4;//求平均
        //不需要清零buffer，下次采样自动覆写
        BAT_Handler();//调用比较函数，判断电量
    }
    else{
        vbat_Buffer[k] = vbat_raw;}//将电压存入数组
}
//////按键处理///////
#define Button P32//按键输入P32
#define Duty_Reg CCAP0H//占空比寄存器，负逻辑，越小，占空比越高
#define NOKEY 0
#define SINGLEKEY 1
#define DOUBLEKEY 2
#define TRIPLEKEY 3
#define QUADRAKEY 4
#define LONGKEY 5
unsigned char Duty = 0x0f;
volatile bit Key_Happen = 0;
bit was_enter_sleep = 0;//
bit need_wakeup_sampling = 0;//被外部中断唤醒，需要进行一次采样
volatile bit Fine_Tune_Inc = 0;   // 正在微调增加占空比
volatile bit Fine_Tune_Dec = 0;   // 正在微调减小占空比
volatile unsigned char Duty_Level = 1;//当前风速
volatile unsigned char Key_Event = NOKEY;
volatile const unsigned char Duty_Table[8] = {//8挡调节
    48, 64, 85, 100, 120, 155, 183, 255
};
unsigned char Rise_Edge_Counter(unsigned long Input){
  unsigned long Mask;//掩码
    //if(Input == 0xffffffff) return 0xff;//如果程序上出现了误采样的话，防止错误给出0上升沿导致误判关机(已解决，不需要了)
    Mask = ~Input & (Input << 1);
    Mask = (Mask & 0x55555555) + ((Mask >> 1) & 0x55555555);
    Mask = (Mask & 0x33333333) + ((Mask >> 2) & 0x33333333);
    Mask = (Mask + (Mask >> 4)) & 0x0F0F0F0F;
    Mask = Mask + (Mask >> 8);
    Mask = Mask + (Mask >> 16);
    Mask = Mask & 0x0000003F;
    return Mask;
}
void Key_Sampling(void){
  //本函数自带消抖，因为不足一个定时器间隔的下降沿无法触发采样
  //在30ms定时器中断中调用本函数
  static  unsigned char Sampling_Counter = 0;//采样次数计数器
  static  unsigned long Sampling_Temp = 0;//采样结果缓存
  static bit Previous_Key = false;//之前的按键采样记录
  static bit Now_Key = false;//当前的按键采样记录
  static bit Is_Sampling = false;//正在采样标志
  static bit Start_Sampling = false;//开始采样标志，由下降沿触发
  static  unsigned char Key_Release_Counter = 0;//超时计数器，可以更快的响应单击和双击操作
  unsigned char Rise_Edge_Result = 0;//上升沿计算的结果,非static变量，重入自动清零
  bit Read_Temp = Button;//按键状态缓存，每次进入函数时读取
  //采样触发(检测下降沿)
  if(!Is_Sampling){//如果没有在采样，才做判断
  Previous_Key = Now_Key;//存储上一次的结果
  Now_Key = Read_Temp;//读取本次结果
  //Key_Event = NOKEY;//未在采样状态，全局按键事件为无键
  if((!Now_Key && Previous_Key) || need_wakeup_sampling){//上一次为1，这一次为0，是下降沿
  Start_Sampling = true;//开始采样
  }
  else{
      Sampling_Counter = 0;Sampling_Temp = 0;Key_Release_Counter = 0;
    return;}//未触发采样，清空变量，返回
  }
  //开始采样与结果处理
  if(Start_Sampling){
    if(Sampling_Counter < 32){
      Is_Sampling = true;
      Sampling_Temp <<= 1;//整体左移一位,低位补0
      if (Read_Temp){//采样引脚，如果为高电平，最低位置1
        Sampling_Temp |= 0x01;//如果引脚为高才计1，如果为低就继续左移一位，也就是补0
        Key_Release_Counter++;
        if(Key_Release_Counter >= 8){//超过160ms没有按键输入
        Key_Release_Counter = 0;
        //采样超时，提前结束，未采样的部分全部补1
        Sampling_Temp = (Sampling_Temp << (31-Sampling_Counter)) | ((1UL << (31-Sampling_Counter)) - 1);
        Sampling_Counter = 31;//标记为采样结束
        }
        }
        else{//如果按键为低电平
        Key_Release_Counter = 0;
        }
        Sampling_Counter++;
        }
      else{//采满32次，采样结束,开始计算
        Is_Sampling = 0;
        Start_Sampling = 0;
        Sampling_Counter = 0;//清空计数器
        need_wakeup_sampling = 0;
        Rise_Edge_Result = Rise_Edge_Counter(Sampling_Temp);//计算上升沿数量
        Key_Happen = 1;//出现按键事件，用于LED闪烁
        switch(Rise_Edge_Result){
          case 0: Key_Event = LONGKEY;  break;
          case 1: Key_Event = SINGLEKEY;break;
          case 2: Key_Event = DOUBLEKEY;break;
          case 3: Key_Event = TRIPLEKEY;break;
        }
        }
  }
}
//要实现：单击/双击阶梯式调节风速，单击后保持/双击后保持连续调节风速
//持续按下时无法触发超时返回，因此需要960ms后才能开始连续加减Duty
void Key_Handler(void){
    static bit Is_Max_Speed = 0;    //当前是否处于“三击最高速”模式
    static unsigned char Saved_Duty_Level = 0; // 用于三击前保存原有档位
    //Key_Event = KEY_READ();
    //如果当前处于三击最大速模式，且出现了单击或双击，先恢复到设置前的档位
    if (Is_Max_Speed && (Key_Event == SINGLEKEY || Key_Event == DOUBLEKEY)) {
        Duty_Level = Saved_Duty_Level;
        Is_Max_Speed = 0;
        Duty = Duty_Table[Duty_Level]; //同步原有占空比
    }
    switch(Key_Event){
        default:break;
        case NOKEY:break;
        case LONGKEY:
            poweron ^= 1; 
            break;
        case SINGLEKEY:
            if(!Button && poweron){ //单击后按键未抬起
                Fine_Tune_Inc = 1; //启动微调增加模式
            }
            else{//正常单击，增加档位
                Duty_Level = (Duty_Level + 1) & 0x07; // 0~7循环
                Duty = Duty_Table[Duty_Level];
            }
            break;
        case DOUBLEKEY:
            if(!Button && poweron){ //双击后按键未抬起
                Fine_Tune_Dec = 1;//启动微调减小模式
            }
            else{
                Duty_Level = (Duty_Level - 1) & 0x07; 
                Duty = Duty_Table[Duty_Level];
            }
            break;
        case TRIPLEKEY:
            //三击处理
            if(!Is_Max_Speed && poweron){
                Saved_Duty_Level = Duty_Level; //保存现在的档位
                Duty_Level = 7;                //强制最高档
                Is_Max_Speed = 1;
            }
            else{//再次三击，恢复
                Duty_Level = Saved_Duty_Level;
                Is_Max_Speed = 0;
            }
            Duty = Duty_Table[Duty_Level];
            break;
    }
    //电池即将耗尽时，限制最大风速
    if((BAT_Status == BAT_DRAINED) && (Duty_Level > 6)){
        Duty_Level = 6;
        Duty = Duty_Table[Duty_Level];
    }
    Key_Event = NOKEY; //清空事件
}
void Adjust_Duty(void){
    //如果检测到按键已经抬起，终止微调模式
    if (Button) { 
        Fine_Tune_Inc = 0;
        Fine_Tune_Dec = 0;
    }
    //持续微调 Duty(即使是低电量状态，也可以通过微调来使占空比超过低电量限制)
    if (Fine_Tune_Inc) {
        Duty = (Duty < 255) ? Duty + 1 : 255;
    } else if (Fine_Tune_Dec) {
        Duty = (Duty > 0) ? Duty - 1 : 0;
    }
    //写入寄存器
    Duty_Reg = (0xFF - Duty); 
}
///////////////EEPROMWR//////////////////////////////
//在写时，只能把1写0，不能把0写1，如果要写0为1，必须先擦除
#define EEPROM_Addr 0x0100//EEPROM写入地址为0100H
void IAP_OPERATE(unsigned int addr){//IAP操作集成，每次EEPROM操作都需要调用这个
    IAP_TPS = 0X18;//24MHZ下
    IAP_CONTR = 0X80;//使能eeprom操作
    IAP_ADDRL =addr;//取低8位
    IAP_ADDRH =addr >> 8;//取高8位
    IAP_TRIG = 0X5A;
    IAP_TRIG = 0XA5;//写完触发命令后，CPU会进入IDLE，读完才会恢复
    _nop_();_nop_();_nop_();_nop_();
}
void IAP_IDLE(){//IAP关闭
    IAP_CONTR = 0X00;
    IAP_CMD = 0X00;
    IAP_TRIG = 0X00;
    IAP_ADDRH = 0X80;
    IAP_ADDRL = 0X00;
    IAP_DATA = 0X00;
}
void EEPROM_ERASE(unsigned int addr){//擦除EEPROM
    IAP_CMD = 0X03;//擦除命令
    IAP_OPERATE(addr);
    IAP_IDLE();
}
void EEPROM_WRITE(unsigned int addr,unsigned char A){//IAP_DATA是一个八位寄存器
    EEPROM_ERASE(addr);//先擦除
    IAP_DATA = A;//将计数器值存储在eeprom数据寄存器中
    IAP_CMD = 0X02;//写命令
    IAP_OPERATE(addr);
    IAP_IDLE();
}
unsigned char EEPROM_READ(unsigned int addr){
    unsigned char A;
    IAP_CMD = 0X01;//EEPROM读命令
    IAP_OPERATE(addr);
    A = IAP_DATA;
    IAP_IDLE();
    return A;
}
//////LED颜色显示//////
#define Bat_Max_mV 4200
#define Bat_Min_mV 2750
#define Brt_Rise 0
#define Brt_Keep_High 1
#define Brt_Fall 2
#define Brt_Keep_Low 3
volatile unsigned char Breath_State = Brt_Rise;//呼吸模式
volatile unsigned int Breath_Counter = 0;//呼吸计数器，实现呼吸计数
void WS2812_Brt_Breath(uint8_t Red,uint8_t Green,uint8_t Blue){
    unsigned int Redin = Red;
    unsigned int Greenin = Green;//int量防止溢出
    static const unsigned int Brt_Ratio = (255 * 2);//最大亮度127
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
                Breath_Counter = 255;
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
static const uint8_t code POW_Table[256] = {//255 * pow(k,1.8) k∈0~1的结果
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
//下面的代码使用的是浮点+对数计算，虽然CPU本身负载很轻，但是还是替换为查表法。
/*void WS2812_Display_BatVoltage(unsigned int BatVol){
    static unsigned int Range = Bat_Max_mV - Bat_Min_mV;
    unsigned char Red,Green,Blue;
    float k;
    if (BatVol > Bat_Max_mV) BatVol = Bat_Max_mV;//如果电压超过最大或最小，则削顶
    else if(BatVol < Bat_Min_mV) BatVol = Bat_Min_mV;
    k = (float)(BatVol - Bat_Min_mV) / Range;
    k = pow(k, 1.8);//使用指数函数来抵消人眼对红色与绿色的非线性感知
    Green = (unsigned char)(k * 255.0f);
    Red = 255 - Green;
    Blue  = 0;
    WS2812_Brt_Breath(Red,Green,Blue);
}*/
void WS2812_Display_BatVoltage(uint16_t BatVol){//以定时器中断为时基调用
    static uint16_t Range = Bat_Max_mV - Bat_Min_mV;
    uint8_t Red,Green,Blue;
    uint8_t k;
    if (BatVol > Bat_Max_mV) BatVol = Bat_Max_mV;//如果电压超过最大或最小，则削顶
    else if(BatVol < Bat_Min_mV) BatVol = Bat_Min_mV;
    k = POW_Table[(uint8_t)((((uint32_t)(BatVol - Bat_Min_mV)) * 255) / Range)];//计算电量比例，查对数表
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
    if(Duty_Level >= 7){//共8档//这个在第一次烧录程序复位后会因为读取到0xff而变紫，是正常的
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
            HSV2RGB(Duty,255,127,&Red,&Green,&Blue);//填入H,S,V,R,G,B
            WS2812_Send_Data(1,Red,Green,Blue);
            break;
        case WS2812_Max_Level:
            HSV2RGB(242,255,127,&Red,&Green,&Blue);//显示紫色
            WS2812_Send_Data(1,Red,Green,Blue);
            break;
    }
}
/////进入休眠//////
volatile bit Back_Sleep = 0;
void If_Back_Sleep(void){
    static unsigned char Sleep_Counter = 0;//睡眠计时器，用于避免误触唤醒
    if(!Button){//只要按键按下
        Sleep_Counter = 0;
        Back_Sleep = 0;
    }
    if(!Charging && !poweron && (Sleep_Counter <= 60)){//误唤醒超时检测
        Sleep_Counter++;}
    else if(!Charging && !poweron && (Sleep_Counter > 60)){
        Sleep_Counter = 0;//超过1500ms，清空状态，重新睡眠
        Back_Sleep = 1;
    }
    else{
        Sleep_Counter = 0;
        Back_Sleep = 0;
    }
}
volatile bit Wakeup_From_Sleep = 1;//初始化为1，主循环将在复位后读一次eeprom，即使读到的值是0xff
void Enter_Sleep(void){
    //从这里开始进入休眠
    if(System_OK){
        EEPROM_WRITE(EEPROM_Addr,Duty_Level);}//如果EEPROM中的数据不是复位值，保存当前占档位
    else if(EEPROM_READ(EEPROM_Addr) == 0XFF){
        EEPROM_WRITE(EEPROM_Addr,Duty_Level);}
    while(1){//外层while
    //Breath_State = Brt_Rise;Breath_Counter = 0;//呼吸模式,呼吸计数器,实现呼吸计数
    Duty_Level = 0;//设置占空比为最低
    TR0 = 0;TR1 = 0;//关闭定时器0,1，避免中断干扰
    WS2812_Send_Data(1,0,0,0);//给WS2812断电前清空WS2812的寄存器，防止上电之后闪烁
    Delay_ms(1);
    PWR_OFF();
    was_enter_sleep = 1;//用于唤醒后采样按键//为了解决触摸按键的唤醒后吞按键问题，只能改为单击后长按开机
    PCON = 0X02;//进入掉电模式
/////唤醒后从这里继续执行/////
    Delay_ms(10);//延时10ms
    //_nop_();_nop_();_nop_();_nop_();//空指令，避免CPU上电后的不稳定
    TR0 = 1;//打开定时器0,开始计时，在中断中执行按键检测
    while(!poweron){//内层while
        WDT_CONTR |= 0X10;//喂狗
        if(Charging && !poweron){//只有在充电且未开机的情况下，才会返回
            if(Timer0_Sync){
                TR1 = 1;//修复：如果通过充电中断唤醒，然后又关机，再触摸按键开机，会导致定时器1关闭，也就失去了显示电量的功能，不过这不是什么严重bug
                Timer0_Sync = 0;
                VBAT_CAL();//如果在睡眠状态唤醒，在这里检测电池电压
                P55 = 0;//打开功率电源，但占空比无输出，风扇不转
        }
            continue;//如果P30 == 0;则直接返回到循环起点，不重新休眠
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
    //由于已经在上面的循环中检测了电池电压，因此没有必要再这里再对电池电压修改
    //TR0 = 1;TR1 = 1;
    //PWR_ON();
    IAP_CONTR |= 0X60;//复位
    //我是真的没招了，不知道到底是单片机问题还是硬件问题，每次关机再开机之后死活读不到eeprom里的档位
    //所以只能出这招了，开机等于复位，复位就是正常工作的状态，居然意外的可以工作。
}
/////中断服务函数//////
void INT0_ROUTINE(void) interrupt 0 {//外部中断0服务函数
    //_nop_();//空指令，仅用于唤醒
    if(was_enter_sleep){
        need_wakeup_sampling = 1;
        was_enter_sleep = 0;
    }
}
void INT4_ROUTINE(void) interrupt 16{//INT4 ON P30，仅用于充电唤醒
    //TR1 = 1;
}
void TIMER0_ROUTINE(void) interrupt 1 {//定时器0 30ms中断服务函数
    Timer0_Sync = 1;
    Key_Sampling();
    Key_Handler();//这里做了修改，现在无论是否有按键事件都会去执行handler
    Adjust_Duty();
    If_Back_Sleep();
}
void TIMER1_ROUTINE(void) interrupt 3 {//定时器5ms中断服务函数
    Timer1_Sync = 1;
    if(poweron || Charging){
    WS2812_State_Machine();}
}
////////主函数/////////
void main(void){
    Interrupt_Init();
    IO_Config();
    Timer_Init();
    ADC_Init();
    PCA_Init();
    WS2812_Init(54);
    PWR_ON();
    for(;;){
        if(!poweron){
            Enter_Sleep();
        }
        else{
            PWR_ON();
        }
        if(Wakeup_From_Sleep){//这个值初始化就是1，也就是复位后执行一次
            Wakeup_From_Sleep = 0;
            Duty_Level = EEPROM_READ(EEPROM_Addr);//读取EEPROM中数据
            Duty = Duty_Table[Duty_Level];
            Duty_Reg = (0xFF - Duty);//给寄存器赋值
            vbat = ((1024UL * Code_Per_Volt()) / 1000UL);
        }
        if(Timer0_Sync){
            Timer0_Sync = 0;
            System_OK = 1;
            VBAT_CAL();
        }
        if(Timer1_Sync){
            Timer1_Sync = 0;
        }
        WDT_CONTR |= 0X10;//喂狗
        }
}






