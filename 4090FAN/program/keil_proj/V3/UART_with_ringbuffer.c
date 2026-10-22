#include "STC8H.H"
#include "main.h"
#define UART_Send_Buffer_Length 128U
#define UART_Recv_Buffer_Length 128U
#define UART_Recv_Temp_Length 64U//tip:这里决定了不能一次接收超过64字节的数据
static volatile uint8_t xdata UART_Send_Buffer[UART_Send_Buffer_Length] = {0};//串口发送缓冲区用uchar做索引不需要软件归零
static volatile uint8_t xdata UART_Recv_Buffer[UART_Recv_Buffer_Length] = {0};//串口接收缓冲区
static volatile uint8_t xdata UART_Recv_Temp[UART_Recv_Temp_Length] = {0};//串口接收缓存，用于临时存放接受的不完整字符串
static volatile uint16_t xdata UART_Recv_Temp_Write_Index = 0;
static volatile uint16_t xdata UART_Send_Write_Index = 0;//串口发送，写缓冲区索引，表示程序上写到哪一位了
static volatile uint16_t xdata UART_Send_Read_Index = 0;//串口发送，读缓冲区索引，表示中断读取到哪一位了
static volatile uint16_t xdata UART_Recv_Write_Index = 0;//串口接收，写缓冲区索引，表示中断写到哪一位了
static volatile uint16_t xdata UART_Recv_Read_Index = 0;//串口接收，读缓冲区索引，表示处理到哪一位了
volatile bit UART_Buffer_Overflow = 0;//缓冲区溢出标志，不分接受还是发送
//readindex只有串口中断在单向加，writeindex只有发送函数在单向加，所以可以 用简单的if判断来让变量循环
/*
串口接收的逻辑和发送略有不同，不过要明白，写索引和读索引都指向缓冲区
读索引是表示CPU读取处理到哪里了，写索引是表示现在在写哪里
而且不存在发送时的串口忙问题
读写索引不相同，就说明还有数据未处理，读写索引相同，就说明数据全部处理完了
*/
static void Write_Data_To_UART_Recv_Buffer(void);
static void UART1_ISR() interrupt 4{
    
    if(TI){//如果是发送完触发的中断
        TI = 0;//清零中断请求位
        //检测是否发送完成了//这里做了修改，因为读索引后置自增导致无法发送\0结束符
        if(UART_Send_Write_Index == UART_Send_Read_Index) return;//如果相同，说明已经传输完成，不再读缓冲区
        
        //从缓冲区中读取并填入到sbuf寄存器，自动触发发送
        SBUF = UART_Send_Buffer[UART_Send_Read_Index];//对SBUF寄存器的写，触发串口发送
        UART_Send_Buffer[UART_Send_Read_Index] = 0; UART_Send_Read_Index++;//将这一位数据清零，然后切换下一位
        if(UART_Send_Write_Index == UART_Send_Read_Index) {SBUF = UART_Send_Buffer[UART_Send_Read_Index];}//补齐最后一位
        
        //处理索引溢出
        if(UART_Send_Read_Index > (UART_Send_Buffer_Length - 1)) UART_Send_Read_Index = 0;//如果缓冲区大小128字节，那么index在0~127勋魂
    }
    else if(RI){//如果是接收触发的中断
        RI = 0;
        //读取sbuf寄存器的内容
        UART_Recv_Temp[UART_Recv_Temp_Write_Index] = SBUF;//读寄存器，读完不能清零，因为sbuf是两个寄存器，一个只读一个只写，写sbuf会触发发送
        UART_Recv_Temp_Write_Index++;
        
        //检测是否是字符串结尾
        if(UART_Recv_Temp[UART_Recv_Temp_Write_Index - 1] == '\0'){
            UART_Recv_Temp_Write_Index = 0;
            Write_Data_To_UART_Recv_Buffer();//在中断中执行这个函数，会不会过于耗时？理论上串口的一个8bit周期是70us左右，
            return;
        }
        
        //处理缓存溢出
        if(UART_Recv_Temp_Write_Index > (UART_Recv_Temp_Length - 2)){
            UART_Recv_Temp[UART_Recv_Temp_Write_Index + 1] = '\0';//在下一位写入结束字符串
            UART_Recv_Temp_Write_Index = 0;
            Write_Data_To_UART_Recv_Buffer();
        }
    }
    else return;
}
// 发送字符串(如果要涵盖字符串的话需要
void UART_Send_String(uint8_t *str) {
    //这个函数只能发字符串，如果要发原始数据，需要用另一个函数
    uint16_t data String_Length = 0;//记录要写入缓冲区的字符有多少字节
    uint8_t *str_temp = str;//在对指针进行自增之后，会导致 指针无法指向字符串的开始，因此需要复制一份
    bit UART_Busy = 0;
    
    //判断UART是否忙
    if(UART_Send_Write_Index != UART_Send_Read_Index) UART_Busy = 1;
    
    //计算字符串长度
    for(String_Length = 0; *str_temp != '\0'; str_temp++,String_Length++);
    
    //判断缓冲区空间是否足够放下字符串(为什么要这么算？画个图就明白了)
    if(UART_Send_Write_Index > UART_Send_Read_Index){
        if((String_Length + 1) > (UART_Send_Buffer_Length - (UART_Send_Write_Index - UART_Send_Read_Index))) {UART_Buffer_Overflow = 1;return;}
    }
    else if(UART_Send_Write_Index < UART_Send_Read_Index){
        if((String_Length + 1) > (UART_Send_Buffer_Length - (UART_Send_Read_Index - UART_Send_Write_Index))) {UART_Buffer_Overflow = 1;return;}
    }
    
    //如果上面判断缓冲区空间足够，就执行下面的操作，将字符串填入缓冲区
    while(*str){//这里假设str结尾固定为\0，即使是手动转换的字符，也要给结尾加\0
        UART_Send_Buffer[UART_Send_Write_Index] = *str;//tip:str为指针，指向字符串储存的地址，*str为这个地址读出来的东西
        str++; UART_Send_Write_Index++;//按位填入
    }
    UART_Send_Buffer[UART_Send_Write_Index] = '\0'; //UART_Send_Write_Index++;//给字符串补齐结尾
    
    //检测索引变量是否溢出
    if(UART_Send_Write_Index > (UART_Send_Buffer_Length - 1)) UART_Send_Write_Index = 0;
    
    //如果不相同，就说明串口依然在发送，不会软件触发中断
    if(UART_Busy) return;//如果两个索引不同，说明 串口仍然在传输状态，不需要传输
    else TI = 1;//如果写和读的index相同，说明传输已经完成，需要再次软件触发中断，实现第一次发送
}
static void Write_Data_To_UART_Recv_Buffer(void){//读取固定的缓存区域，不需要传入
    uint16_t data String_Length = 0;
    
    //统计字符串长度
    for(String_Length = 0; UART_Recv_Temp[String_Length] != '\0'; String_Length++);//i为字符串不包括\0的长度，因此后面会加一
    
    //判断缓冲区是否足够放下字符串
    if(UART_Recv_Write_Index > UART_Recv_Read_Index){
        if((String_Length + 1) > (UART_Recv_Buffer_Length - (UART_Recv_Write_Index - UART_Recv_Read_Index))) {UART_Buffer_Overflow = 1;return;}
    }
    else if(UART_Recv_Write_Index < UART_Recv_Read_Index){
        if((String_Length + 1) > (UART_Recv_Buffer_Length - (UART_Recv_Read_Index - UART_Recv_Write_Index))) {UART_Buffer_Overflow = 1;return;}
    }
    
    //将缓存的数组搬运到缓冲区
    while(UART_Recv_Temp[UART_Recv_Write_Index]){//这里假设str结尾固定为\0，即使是手动转换的字符，也要给结尾加\0
        UART_Recv_Buffer[UART_Recv_Write_Index] = UART_Recv_Temp[UART_Recv_Write_Index];
        UART_Recv_Write_Index++;//按位填入
    }
    UART_Recv_Buffer[UART_Recv_Write_Index] = '\0'; UART_Recv_Write_Index++;//补\0
    
    //处理变量溢出
    if(UART_Recv_Write_Index > (UART_Recv_Buffer_Length - 1)) UART_Recv_Write_Index = 0;
    
}
// 发送 uint16_t 为十进制字符串（自动加 \r\n）
void UART_Read_String_From_Buffer(void){
    uint8_t xdata temp[64] = {0};
    //如果相同，缓存区没有新数据，不再读缓冲区
    if(UART_Recv_Write_Index == UART_Recv_Read_Index) return;
    
}
void UART_Send_Data(){//发送原始数据
    
}
/*void Uart1_SendUint16(uint16_t num){
    char buffer[7];  // 最大 "65535\r\n" → 6 字符 + '\0'
    char *p = buffer + sizeof(buffer) - 1;
    *p = '\0';       // 字符串结尾

    if (num == 0) {
        *(--p) = '0';
    } else {
        while (num > 0) {
            *(--p) = (num % 10) + '0';  // 转为 ASCII
            num /= 10;
        }
    }

    // 先发送数字
    Uart1_SendString(p);
    // 再发送换行（Windows 常用 \r\n，Linux/Mac 可只用 \n）
    Uart1_SendByte('\r');
    Uart1_SendByte('\n');
}*/