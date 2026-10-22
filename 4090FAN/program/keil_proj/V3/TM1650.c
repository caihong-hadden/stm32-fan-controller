#include "STC8H.H"
#include "main.h"
//报错FLAG
bit TM1650_Error = 0;
//显存地址
#define TM1650_CMD1 0x48//TM1650数据命令，
#define TM1650_DIG1 0x68//四段分别地址
#define TM1650_DIG2 0x6A//一般DIG1为最高显示位
#define TM1650_DIG3 0x6C
#define TM1650_DIG4 0x6E
//取模表
#define SEG_0 0X3F
#define SEG_1 0X06
#define SEG_2 0X5B
#define SEG_3 0X4F
#define SEG_4 0X66
#define SEG_5 0X6D
#define SEG_6 0X7D
#define SEG_7 0X07
#define SEG_8 0X7F
#define SEG_9 0X6F
#define SEG_Blank 0X00
#define SEG_Minus 0X40
#define SEG_E 0X79
#define SEG_r 0X50
#define SEG_O 0X3F
#define SEG_F 0X71
#define SEG_n 0X54
#define SEG_U 0X3E
#define SEG_V 0X3E
#define SEG_P 0X73
#define SEG_C 0X39
#define SEG_A 0X77
#define SEG_B 0X7F
#define SEG_b 0X7C
#define SEG_T 0X46
#define SEG_S 0X6D
#define SEG_L 0X38
#define SEG_H 0X76
#define SEG_I 0X06
#define SEG_o 0X5C
//I2C命令表
#define I2C_IDLE 0X00
#define I2C_START 0X01
#define I2C_STOP 0X06
#define I2C_SENDMS 0X02
#define I2C_RECV_ACK 0X03
static const unsigned char TM1650_CMD2[] = {//此表决定了发送的亮度数值
    0x00,// 0 关闭显示
    0x11,// 1级亮度
    0x21,// 2级亮度
    0x31,// 3级亮度
    0x41,// 4级亮度
    0x51,// 5级亮度
    0x61,// 6级亮度
    0x71,// 7级亮度
    0x01 // 8级亮度
};
//frequency单位为k，brightness取0时关闭数码管输出
//num为要显示的数字，取值0到9999，DP为小数点位置，取值0到3
static unsigned char Seg_Table(unsigned char word) {
    switch (word) {
        case '0': return SEG_0;      // 0x3F
        case '1': return SEG_1;      // 0x06
        case '2': return SEG_2;      // 0x5B
        case '3': return SEG_3;      // 0x4F
        case '4': return SEG_4;      // 0x66
        case '5': return SEG_5;      // 0x6D
        case '6': return SEG_6;      // 0x7D
        case '7': return SEG_7;      // 0x07
        case '8': return SEG_8;      // 0x7F
        case '9': return SEG_9;      // 0x6F
        case 'A': return SEG_A;      // 0x77
        case 'B': return SEG_B;      // 0x7F
        case 'C': return SEG_C;      // 0x39
        case 'E': return SEG_E;      // 0x79
        case 'F': return SEG_F;      // 0x71
        case 'G': return SEG_6;      // 0x7D
        case 'H': return SEG_H;      // 0x76
        case 'L': return SEG_L;      // 0x38
        case 'P': return SEG_P;      // 0x73
        case 'S': return SEG_S;      // 0x6D
        case 'T': return SEG_7;      // 0x07
        case 'U': return SEG_U;      // 0x3E
        case 'V': return SEG_V;      // 0x3E
        case 'O': return SEG_O;      // 0x3F
        //小写字母
        case 'b': return SEG_b;      // 0x7C
        case 'n': return SEG_n;      // 0x54
        case 'r': return SEG_r;      // 0x44
        case 'o': return SEG_o;      // 0x3F
        case 'I': return SEG_I;      // 0x06
        case 'g': return SEG_9;      // 0x6F
        //标点
        case ' ': return SEG_Blank;  // 空格显示空白
        case '-': return SEG_Minus;  // 0x40
        default: return SEG_Blank;  // 未定义字符显示空白
    }
}
static void I2C_CMD(unsigned char CMD){//I2C命令设置
    unsigned int Release_Timer = 0;
    I2CMSCR = (I2CMSCR & 0XF0) | CMD;//只填低四位
    //头文件中没有MSIF的定义，因此只能手动做与运算
    while(!(I2CMSST & 0X40)){
        if(++Release_Timer > 10000) {
            TM1650_Error = 1;break;}}//等待I2C控制器执行完命令，超时则退出并报错
    I2CMSCR = (I2CMSCR & 0XF0) | I2C_IDLE;//清空I2C命令
    I2CMSST &= ~0X40;//清除完成标志(~0X40 = 10111111,做与运算，可清零bit6位)
}
static bit I2C_Check_ACK(void){//TM1650不需要确认ACK
    unsigned char a = 0;
    a = (I2CMSST & 0X02);//读取I2CMSST B1位是否为1
    return a;//返回1或0
}
static void TM1650_Write(unsigned char Data,unsigned char Addr){//底层发送数据驱动
    //此处不需I2C开始与停止发送，而是手动添加，可连续发送。
    if(Addr != 0){//如果需要在同一地址发送超过8bit数据，可令Addr=0
        I2C_CMD(I2C_START);
        I2CTXD = Addr;//不需要担心时序，每字节后必须有一个ack，时间足够
        I2C_CMD(I2C_SENDMS);//发送命令
        I2C_CMD(I2C_RECV_ACK);//接受ACK
        I2CTXD = Data;//先发地址，后发数据
        I2C_CMD(I2C_SENDMS);
        I2C_CMD(I2C_RECV_ACK);
        I2CTXD = 0;//清空发送寄存器
        I2C_CMD(I2C_STOP);
    }
    else{//往一个八位地址一直发数据，比如写OLED屏（特殊设备有16位I2C地址，在这里不做讨论）
        I2CTXD = Data;//先发地址，后发数据
        I2C_CMD(I2C_SENDMS);
        I2C_CMD(I2C_RECV_ACK);
    }
}
void TM1650_PinSet(unsigned char SCL,unsigned char SDA){
    P_SW2 = 0X80;
    //仅对STC8H和G系列部分有效
    //本函数请谨慎使用，务必先查看技术手册中的引脚定义，或者手动规定引脚
    //对于所有非法值，都不会改变I2C引脚
    if(SCL == 15 && SDA == 14){
        P_SW2 &= ~0X30;//B5,B4置0
        P1M0 |= 0x30; P1M1 |= 0x30; //P15 P14设置为开漏
    }
    else if(SCL == 25 && SDA == 24){
        P_SW2 &= ~0X30;//B5,B4置0
        P_SW2 |= 0X10;//B4置1
        P2M0 = 0X30;P2M1 = 0X30;//P25 P24开漏
    }
    else if(SCL == 77 && SDA == 76){
        P_SW2 &= ~0X30;//B5,B4置0
        P_SW2 |= 0X20;//B5置1
        P7M0 |= 0xC0;P7M1 |= 0xC0;//P77 P76开漏
    }
    else if(SCL == 32 && SDA == 33){
        P_SW2 &= ~0X30;//B5,B4置0
        P_SW2 |= 0X30;//B5,B4置1
        P3M0 |= 0x0C;P3M1 |= 0X0C;//P32,P33开漏
    }
    else if(SCL == 54 && SDA == 55){
        P_SW2 &= ~0X30;//B5,B4置0
        P_SW2 |= 0X10;//仅对STC8G1K08A 8PIN有效
        P5M0 |= 0X30;P5M1 |= 0X30;
    }
}
void TM1650_Init(unsigned int TM1650_Freq_kHz,unsigned char Brt){
    unsigned char MSSPEED = 0;
    P_SW2 |= 0X80;//允许访问扩展寄存器
    if(TM1650_Freq_kHz != 0){//传入0代表不重新初始化I2C
    I2CCFG = 0XE0;//启用I2C，主机模式
    MSSPEED = (((Main_FOSC/4)/(TM1650_Freq_kHz * 1000UL)) - 2) & 0X3F;//与运算只保留低六位
    I2CCFG = (I2CCFG & 0XC0) | MSSPEED;//写入低六位
    I2CMSCR = 0X00;//开启I2C主机模式中断，清空I2C命令
    I2CTXD = 0X00;//清空发送数据寄存器
    I2CRXD = 0X00;//清空接收数据寄存器
    I2CMSAUX = 0X00;//关闭主机自动发送功能
    I2CMSST = 0X00;//清零标志
    }
    TM1650_Write(TM1650_CMD2[Brt],TM1650_CMD1);//发送命令1(0X48)，再发送命令2(亮度)
    //MSBUSY指示I2C控制器是否忙碌，但本驱动程序不需要检查MSBUSY
}
//在C语言中，字符串自动转换为由二进制码组成的数组
//const char *Num 接收的是这个字符串的首地址
//Num为指针（地址），*Num为这个地址读到的东西
void TM1650_Display_Word(const char *Word){
    bit TM1650_Need_DP = 0;//是否需要小数点标志
    unsigned char TM1650_Current_Digit = 0;//记录当前写的数码管段
    unsigned char TM1650_Write_Buffer = 0;//写过程的缓存
        while(*Word != '\0' && TM1650_Current_Digit < 4){//字符串未结尾，且并没有写完完整4位
        if(*(Word+1) == '.'){//检测下一位是否是小数点(TM1650的小数点与上一个段绑定)
            TM1650_Need_DP = 1;//标记需要小数点
        }
        TM1650_Write_Buffer = Seg_Table(*Word);//按字符ASCII值查表
        if(TM1650_Need_DP){//如果需要小数点
            TM1650_Write_Buffer |= 0x80;//最高位置1
            TM1650_Need_DP = 0;//清零标志
            Word = Word + 1;//下一位存在小数点，直接跳过
            }
        switch(TM1650_Current_Digit){//写入
            case 0: TM1650_Write(TM1650_Write_Buffer,TM1650_DIG1); break;
            case 1: TM1650_Write(TM1650_Write_Buffer,TM1650_DIG2); break;
            case 2: TM1650_Write(TM1650_Write_Buffer,TM1650_DIG3); break;
            case 3: TM1650_Write(TM1650_Write_Buffer,TM1650_DIG4); break;
            }
            TM1650_Current_Digit++;//数码管段+1
            Word++;//指针+1
        }
}
//V1.2更新，displaynum函数支持显示负数
void TM1650_Display_Num(unsigned int Num, unsigned char Dot_pos){
    //整数转字符所用变量
    char Trans_Temp[4];//整数截取低四位的缓存
    char TM1650_String_Buffer[8];//最终输出的字符串缓存区
    char *Ptr = TM1650_String_Buffer;//数组的指针，用于在数组取值
    //unsigned char i = 4; //数据保留低四位
    unsigned char j = 0;
    unsigned char k = 0;
    //写入TM1650所用变量+
    bit TM1650_Need_DP = 0;//是否需要小数点标志
    //bit TM1650_Is_Negtive = 0;//标记为负数//在本项目中不需要显示负数
    unsigned char TM1650_Current_Digit = 0;//记录当前写的数码管段
    unsigned char TM1650_Write_Buffer = 0;//写过程的缓存区域
    //检测是正数还是负数
    /*if(Num < 0){
        Num = -Num;//取反
        TM1650_Is_Negtive = 1;//标记为负数
    }*/
    //即使输入为负数，下面的整数转字符代码也不需要修改，写入时不写DIG1即可
    //整数转字符
    //先把整数拆解成纯数字数组 (倒序存放)
    //比如 1234 -> temp[0]=4, temp[1]=3, temp[2]=2, temp[3]=1
    //比如 -123 → temp[0]=-
    for(k=0; k<4; k++) {
        Trans_Temp[k] = (Num % 10) + '0';
        Num /= 10;
    }
    //根据 dot_pos 组装带点的字符串
    //从高位往低位填入buffer
    for(k=0; k<4; k++) {
        TM1650_String_Buffer[j++] = Trans_Temp[3-k]; //填入数字
        if(Dot_pos == (k + 1) && k < 3) { //如果到了设定的点位
            TM1650_String_Buffer[j++] = '.';
        }
    }
    TM1650_String_Buffer[j] = '\0'; //封口
    //字符数组读取与写入显存
    while(*Ptr != '\0' && TM1650_Current_Digit < 4){//字符串未结尾，且并没有写完完整4位
        if(*(Ptr+1) == '.'){//检测下一位是否是小数点(TM1650的小数点与上一个段绑定)
            TM1650_Need_DP = 1;//标记需要小数点
        }
        TM1650_Write_Buffer = Seg_Table(*Ptr);//按字符ASCII值查表
        if(TM1650_Need_DP){//如果需要小数点
            TM1650_Write_Buffer |= 0x80;//最高位置1
            TM1650_Need_DP = 0;//清零标志
            Ptr = Ptr + 1;//下一位存在小数点，直接跳过
            }
        switch(TM1650_Current_Digit){//写入
            case 0: TM1650_Write(TM1650_Write_Buffer,TM1650_DIG1); break;
            case 1: TM1650_Write(TM1650_Write_Buffer,TM1650_DIG2); break;
            case 2: TM1650_Write(TM1650_Write_Buffer,TM1650_DIG3); break;
            case 3: TM1650_Write(TM1650_Write_Buffer,TM1650_DIG4); break;
            }
            TM1650_Current_Digit++;//数码管段+1
            Ptr++;//指针+1
        }
}
void TM1650_Display_With_SetLocation_Blank(uint16_t Num,uint8_t Blank_POS,uint8_t Dot_POS){
    //整数转字符所用变量
    char Trans_Temp[4];//整数截取低四位的缓存
    char TM1650_String_Buffer[8];//最终输出的字符串缓存区
    //unsigned char i = 4; //数据保留低四位
    unsigned char j = 0;
    unsigned char k = 0;
    //整数转字符
    //先把整数拆解成纯数字数组 (倒序存放)
    //比如 1234 -> temp[0]=4, temp[1]=3, temp[2]=2, temp[3]=1
    //比如 -123 → temp[0]=-
    for(k=0; k<4; k++) {
        Trans_Temp[k] = (Num % 10) + '0';
        Num /= 10;
    }
    Trans_Temp[3-Blank_POS] = ' ';//填入空白位
    //根据 dot_pos 组装带点的字符串
    //从高位往低位填入buffer
    for(k=0; k<4; k++) {
        TM1650_String_Buffer[j++] = Trans_Temp[3-k]; //填入数字
        if(Dot_POS == (k + 1) && k < 3) { //如果到了设定的点位
            TM1650_String_Buffer[j++] = '.';
        }
    }
    TM1650_String_Buffer[j] = '\0'; //封口
    TM1650_Display_Word(TM1650_String_Buffer);//发送到TM1650
}
#define MAX_VOUT 15800U
#define MIN_VOUT 2500U
#define MAX_IOUT 6500U
#define MIN_IOUT 1000U
#define SAVE_TIME 5000U//在设置状态下超时退出的时间
volatile bit Need_Adjust = 1;//表示需要调整输出，初始化为1，上电后直接开始调整
static void TM1650_Twinkle(uint16_t Input,uint8_t Twinkle_POS,uint8_t Dot_POS){//由1ms时序上层函数调用
    //输入仅能为数字
    static uint16_t xdata Twinkle_Timer = 0;
    static uint8_t Twinkle_State = 0;
    switch(Twinkle_State){
        case 0:
            if(++Twinkle_Timer >= 500) {
                Twinkle_State = 1;//1000ms闪烁一个周期
                Twinkle_Timer = 0;}
            TM1650_Display_With_SetLocation_Blank(Input,Twinkle_POS,Dot_POS);
            break;
        case 1:
            if(++Twinkle_Timer >= 500) {
                Twinkle_State = 0;//回到状态0
                Twinkle_Timer = 0;}
            TM1650_Display_Num(Input,Dot_POS);
            break;
    }
}