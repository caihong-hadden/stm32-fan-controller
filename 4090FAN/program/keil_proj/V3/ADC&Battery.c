#include "main.h"

volatile unsigned int vbat = 0;

void VBAT_CAL(void){
    static unsigned char k = 0;
    unsigned int vbat_raw = 0;
    
    // 这里假设已经通过 ADC 读取了 raw 值
    // vbat_raw = ADC_Read(); 
    
    // 软件低通滤波 (IIR): vbat = 0.2 * raw + 0.8 * old_vbat
    vbat = (unsigned int)((0.2f * vbat_raw) + (0.8f * vbat));
    
    k++;
    if(k >= 10) k = 0;
}

void BAT_Handler(unsigned int BatVol){
    vbat = (unsigned int)((BatVol * 2 + vbat * 8) / 10);
}
