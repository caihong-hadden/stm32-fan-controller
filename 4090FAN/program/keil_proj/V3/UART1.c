#include "STC8H.H"
#include "main.h"
/*
本来打算做成环形缓冲区的
但是环形环形缓冲区有点太费ram了
而且在串口波特率设置为1M之后，串口阻断循环的时间也是可以接受的
实际上即使是115200波特率，发7字节也只需要500us
*/
//底层阻断式发送函数
void Serial_SendByte(unsigned char dat) {
    SBUF = dat;           // 写入发送缓冲区
    while (!TI);          // 等待发送完成（TI=1）
    TI = 0;               // 清除发送中断标志
}

void Serial_Printstr(unsigned char *str){//打印字符串
    while (*str != '\0') {
        Serial_SendByte(*str);
        str++;
    }
}

void Serial_Printstrln(unsigned char *str){//打印字符串并换行
    while (*str != '\0') {
        Serial_SendByte(*str);
        str++;
    }
    Serial_SendByte('\r');
    Serial_SendByte('\n');
}

void Serial_Printnumln_with_Word(unsigned char *str,unsigned int num){//带语句和换行的格式化输出
    int8_t buffer[7];//声明缓冲区，只需要6位，多一位防止溢出
    int8_t *p = buffer + sizeof(buffer) - 1;//定义指针，使指针指向数组末尾,因为要从后往前写入字符
    //整数转字符
    if (num == 0) {
        *(--p) = '0';
    } else {
        while (num > 0) {
            *(--p) = (num % 10) + '0';  // 转为 ASCII
            num /= 10;
        }
    }
    Serial_Printstr(str);
    Serial_Printstr(p);
    Serial_SendByte('\r');
    Serial_SendByte('\n');
}

void Serial_Printnum(unsigned int num){//格式化输出，不换行
    int8_t buffer[7];//声明缓冲区，只需要6位，多一位防止溢出
    int8_t *p = buffer + sizeof(buffer) - 1;//定义指针，使指针指向数组末尾,因为要从后往前写入字符
    *p = '\0';       // 字符串结尾为\0
    //整数转字符
    if (num == 0) {
        *(--p) = '0';
    } else {
        while (num > 0) {
            *(--p) = (num % 10) + '0';  // 转为 ASCII
            num /= 10;
        }
    }
    //发送转换的字符
    Serial_Printstr(p);
}
void Serial_Printint16ln(int16_t num){//格式化输出int16并换行
    int8_t buffer[7];//声明缓冲区，只需要6位，多一位防止溢出
    int8_t *p = buffer + sizeof(buffer) - 1;//使指针指向数组末尾,因为要从后往前写入字符
    if(num < 0){
        num = (unsigned int)(-1 * (int32_t)num); 
        Serial_SendByte('-');//先传负号
    }
    else{
        num = (unsigned int)num;
    }
    
    *p = '\0';       // 字符串结尾为\0

    if (num == 0) {
        *(--p) = '0';
    } else {
        while (num > 0) {
            *(--p) = (num % 10) + '0';  // 转为 ASCII
            num /= 10;
        }
    }

    // 先发送数字
    Serial_Printstr(p);
    //再发送换行（Windows 常用 \r\n，Linux/Mac 可只用 \n）
    Serial_SendByte('\r');
    Serial_SendByte('\n');
}
void Serial_Printnumln(unsigned int num){//格式化输出并换行
    int8_t buffer[7];//声明缓冲区，只需要6位，多一位防止溢出
    int8_t *p = buffer + sizeof(buffer) - 1;//使指针指向数组末尾,因为要从后往前写入字符
    *p = '\0';       // 字符串结尾为\0

    if (num == 0) {
        *(--p) = '0';
    } else {
        while (num > 0) {
            *(--p) = (num % 10) + '0';  // 转为 ASCII
            num /= 10;
        }
    }

    // 先发送数字
    Serial_Printstr(p);
    //再发送换行（Windows 常用 \r\n，Linux/Mac 可只用 \n）
    Serial_SendByte('\r');
    Serial_SendByte('\n');
}
