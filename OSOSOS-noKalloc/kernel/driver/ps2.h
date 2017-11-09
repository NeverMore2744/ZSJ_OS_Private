#ifndef _PS2_H
#define _PS2_H

#include <driver/ps2.h>

//将扫描码转换为 ASCII 码
//如果扫描码无法转换为 ASCII 码，则返回 -1
int kernel_scantoascii(int key);

#ifdef PS2_DEBUG
void print_wptr();
void print_rptr();
void print_buffer();
void print_curr_key(int key);
void print_curr_char(int key);
#endif  // ! PS2_DEBUG

#endif
