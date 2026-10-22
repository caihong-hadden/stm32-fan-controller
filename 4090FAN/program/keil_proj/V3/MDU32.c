#include "STC8H.H"
#include "main.h"
/*
使用STC8H系列的硬件32位乘除法/移位器，加速计算速度
好吧，其实这个STC8H1K08T没有MDU32，此文件废弃
*/
/*
#define MDU_CMD_NOP           0   // 无动作，待机
#define MDU_CMD_MUL32         2   // 32位乘法 (耗时3时钟)
#define MDU_CMD_DIVU32        4   // 无符号32位除法 (耗时19时钟)
#define MDU_CMD_DIVS32        6   // 有符号32位除法 (耗时21时钟)
#define MDU_CMD_NORM          8   // 数据规格化 (Normalization)
#define MDU_CMD_LOGIC_SHIFT   10  // 逻辑左移/右移 (Logical Shift)
#define MDU_CMD_ARITH_SHIFT   11  // 算术右移 (Arithmetic Shift)
#define MDU_CMD_ADD32         12  // 32位加法
#define MDU_CMD_SUB32         14  // 32位减法
*/
