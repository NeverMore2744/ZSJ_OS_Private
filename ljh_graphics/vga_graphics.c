#include<zjunix/mmu/vga_g.h>
#include<arch.h>
#include<driver/vga.h>

#define MH 480
#define MW 640
#define GRAPHICS_ADDR 0xbfe00000
#define GADDR(W, H) ((unsigned int*)(GRAPHICS_ADDR + (H << 12) + (W << 2)))
#define HEIGHT_ADD(H, DH) ((H + DH) >= 480 ? 479: (H + DH))
#define WIDTH_ADD(W, DW) ((W + DW) >= 640 ? 639: (W + DW))

unsigned int print_pixel(unsigned int w, unsigned int h, unsigned int pixel) {
    unsigned int* infra;
    if (!(*VGA_MODE)) {
        kernel_printf("Error: It is not in the Graphics mode!");
        while (1) ;
    }
    infra = GADDR(w, h);
    *infra = pixel;
    return 1;
}

void cvt2Gmode() {
    *VGA_MODE = 1;
}

/*
 * -------------------------------------------...
 * |                        ^
 * |                        |
 * |                   pic_height
 * |                        |
 * |                        v
 * |<-- pic_width --> ------------
 * |                  |          |    ^
 * |                  | rectangle|  height
 * |                  |          |    v
 * |                  ------------              
 * |                  <   width  >
 * ...
 * 
 * @param: pic: The base address of pixels
 *  It is followed by width*height pixels (width*height*4 bytes)
 * @param: pic_width: The left-up x-position
 * @param: pic_height: The left-up y-position
 * @param: width: The width of picture
 * @param: height: The height of picture
 * */

void print_rectangle(unsigned int color, unsigned int pic_width, 
    unsigned int pic_height, unsigned int width, unsigned int height) {
    unsigned int i, j;
    unsigned int* infra;
    for (i = pic_height; i < HEIGHT_ADD(pic_height, height); i++) {
        for (j = pic_width; j < WIDTH_ADD(pic_width, width); j++) {
            infra = GADDR(j, i);
            *infra = color;
        }
    }
}

void clear_black() {
    unsigned int i, j;
    unsigned int* infra;
    for (i = 0; i < MH; i++) {
        for (j = 0; j < MW; j++) {
            infra = GADDR(j, i);
            *infra = 0;
        }
    }
    kernel_clear_screen(31);
}

/*
 * -------------------------------------------...
 * |                        ^
 * |                        |
 * |                   pic_height
 * |                        |
 * |                        v
 * |<-- pic_width --> ------------
 * |                  |          |    ^
 * |                  | picture  |  height
 * |                  |          |    v
 * |                  ------------              
 * |                  <   width  >
 * ...
 * 
 * @param: pic: The base address of pixels
 *  It is followed by width*height pixels (width*height*4 bytes)
 * @param: pic_width: The left-up x-position
 * @param: pic_height: The left-up y-position
 * @param: width: The width of picture
 * @param: height: The height of picture
 * */
void print_pure_picture(unsigned int* pic, unsigned int pic_width, 
    unsigned int pic_height, unsigned int width, unsigned int height) {
    unsigned int i, j;
    unsigned int* infra;
    unsigned int* pic_ptr;
    pic_ptr = pic;
    for (i = pic_height; i < HEIGHT_ADD(pic_height, height); i++) {
        for (j = pic_width; j < WIDTH_ADD(pic_width, width); j++) {
            infra = GADDR(j, i);
            *infra = (unsigned int)(*(pic_ptr));
            pic_ptr = pic_ptr + 1;
        }
    }
}