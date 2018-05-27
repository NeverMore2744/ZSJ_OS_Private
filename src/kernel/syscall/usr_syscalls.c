#include<zjunix/ljh/usr_syscalls.h>
#include<arch.h>
#include<driver/ps2.h>
#include<zjunix/ljh/vga_g.h>
/*
void init_usr_syscall() {
    register_syscall(140, syscall_getchar);
    register_syscall(160, syscall_graphics);
}*/

// 140: getchar
int syscall_getchar(unsigned int status, unsigned int cause, context* pt_context) {
    unsigned char c;
    c = kernel_getchar();
    pt_context->v0 = (unsigned int)c;
    return 0;
}

/*
 * syscall number: 160
 * The GVram address: 0xbfe00000 - 0xbfffffff;
 * Enable: 0xbfc09024
 * Graphics operations: 
 * 0 - Text mode
 * 1 - Graphics mode
 * 2 - Print a square
 * 3 - Print a picture
 * 4 - clear the screen
 * */

int syscall_graphics(unsigned int status, unsigned int cause, context* pt_context) {
    unsigned int code = pt_context->a0;  // $a0 = code
    unsigned int color, picw, pich, w, h;
    unsigned int* pic;
    switch (code) {
        case 0: 
            *VGA_MODE = 0;
            break;
        case 1:
            *VGA_MODE = 1;
            break;
        case 2:
            /* 2 - Print a square
             *      $a1 = color
             *      high-16bits($a2) = picw
             *      low-16bits($a2) = pich
             *      high-16bits($a3) = w
             *      low-16bits($a3) = h
             * */
            if (*VGA_MODE == 0) break;
            color = pt_context -> a1;
            picw = pt_context -> a2;
            pich = picw & ((1 << 16) - 1);
            picw >>= 16;
            w = pt_context -> a3;
            h = w & ((1 << 16) - 1);
            w >>= 16;
            print_rectangle(color, picw, pich, w, h);
            break;
        case 3:
            if (*VGA_MODE == 0) break;
            pic = (unsigned int*)(pt_context -> a1);
            picw = pt_context -> a2;
            pich = picw & ((1 << 16) - 1);
            picw >>= 16;
            w = pt_context -> a3;
            h = w & ((1 << 16) - 1);
            w >>= 16;
            print_pure_picture(pic, picw, pich, w, h);
            break;
        case 4:
            clear_black();
            break;
        default: break;
    }
    return 0;
}