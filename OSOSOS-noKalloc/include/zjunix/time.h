#ifndef _ZJUNIX_TIME_H
#define _ZJUNIX_TIME_H

/*
高精度计时器
• COP0(COP0( 协处理器 0) 内有一个 64 位高精度计时器
• 该计时器非 MIPS32 规范所定
• 通过 9.6 和9.7 号COP0COP0 寄存器读取计数值
• 计时器的值不受系统复位以外任何事件影响
• 可通过该计时器获取系统启动后经历的间
• 基于此计时器实现获取系统间的函数
• get_timeget_timeget_time (char * buf )
• 将当前计时器的值转换为表示时间的字符串
*/
// Put current time into buffer, at least 8 char size
void get_time(char* buf, int len);

void system_time_proc();

#endif // ! _ZJUNIX_TIME_H