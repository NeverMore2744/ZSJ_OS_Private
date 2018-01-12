#include<zjunix/mmu/usr_syscalls.h>
#include<arch.h>
#include<driver/ps2.h>
#include<zjunix/mmu/vga_g.h>

// 140: getchar
void syscall_getchar(unsigned int status, unsigned int cause, context* pt_context) {
    unsigned char c;
    c = kernel_getchar();
    pt_context->v0 = (unsigned int)c;
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
 * */

void syscall_graphics(unsigned int status, unsigned int cause, context* pt_context) {
    unsigned int code = pt_context->a0;
    switch (code) {
        case 0: 
            *VGA_MODE = 0;
            break;
        case 1:
            *VGA_MODE = 1;
            break;
        case 2:
            /* 2 - Print a square
             *      a0 = color
             *      high-16bits(a1) = picw
             *      low-16bits(a1) = pich
             *      high-16bits(a2) = w
             *      low-16bits(a2) = h
             * */
            if (*VGA_MODE == 0) break;
            unsigned int color, picw, pich, w, h;
            color = pt_context -> a0;
            picw = pt_context -> a1;
            pich = picw & ((1 << 16) - 1);
            picw = picw >> 16;
            w = pt_context -> a2;
            h = w & ((1 << 16) - 1);
            print_rectangle(color, picw, pich, w, h);
            break;
        case 3:
            if (*VGA_MODE == 0) break;
            unsigned int color, picw, pich, w, h;
            color = pt_context -> a0;
            picw = pt_context -> a1;
            pich = picw & ((1 << 16) - 1);
            picw = picw >> 16;
            w = pt_context -> a2;
            h = w & ((1 << 16) - 1);
            print_rectangle(color, picw, pich, w, h);
            break;
        default: break;
    }
}