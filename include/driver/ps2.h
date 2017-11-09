#ifndef _DRIVER_PS2
#define _DRIVER_PS2

#include <intr.h>
#include <zjunix/pc.h>

void init_ps2();
void ps2_handler(unsigned int status, unsigned int cause, context* pt_context);

//获取按键扫描码
//如果缓冲区为空，则返回 0xfff 
int kernel_getkey();

/*
function: keep calling the subfunction until it gets a valid ascii code (rather than scan code)
Special Note: rolling? blocked? 
    do {
        key = kernel_scantoascii(kernel_getkey());
        sleep(1000);
    } while (key == -1);
*/
int kernel_getchar();

#endif // ! _DRIVER_PS2
