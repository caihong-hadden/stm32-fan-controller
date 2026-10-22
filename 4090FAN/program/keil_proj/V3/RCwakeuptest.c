#include "STC8H.H"
int main(){
    P_SW2 |= 0X80;
    P3M0 = 0xff; P3M1 = 0x00; 
    P3 = 0XFF;
    WKTCH = 0XFF;//约每16秒唤醒一次，检测电容电压是否放净，初始化时启用唤醒功能。之后就不用关了
    WKTCL = 0XFE;//此功能不属于系统中断，不需要中断服务函数
    PCON |= 0X02;
    P3 = 0X00;
    while(1);
}