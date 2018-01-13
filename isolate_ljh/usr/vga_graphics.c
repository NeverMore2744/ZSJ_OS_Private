#include <zjunix/mmu/vga_g.h>
#include <arch.h>
#include <driver/vga.h>
#include <zjunix/slab.h>

#define get_int(i) ((unsigned int)(*(bmp_buffer+i)))

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

unsigned int get4bytes(unsigned int offset, unsigned char* bmp_buffer) {
    return get_int(offset) + (get_int(offset+1) << 8) + 
        (get_int(offset+2) << 16) + (get_int(offset+3) << 24);
}

unsigned int get2bytes(unsigned int offset, unsigned char* bmp_buffer) {
    if (offset & 3 == 0) return get_int(offset) & 0xffff;
    if (offset & 3 == 1 || offset & 3 == 2) 
        return get_int(offset) + (get_int(offset+1) << 8);
    return get_int(offset) + (get_int(offset+1) & 0xff);
}

unsigned int get1byte(unsigned int offset, unsigned char* bmp_buffer) {
    return get_int(offset) & 0xff;
}

// 18-19 pixels: 
void print_32b_bmp(unsigned char* bmp_buffer, unsigned int len, 
    unsigned int sx, unsigned int sy) {
    unsigned int b, g, r, t4, height, width, i;
    unsigned int inverse = 1, step, start, end = 0;
    b = get_int(0);
    if (b & 0xffff != 0x4d42) {
        kernel_printf("Error: This is not a bmp file!\n");
        return;
    }
    unsigned int* buffer;
    unsigned int bptr, cnt=0;
    width = get4bytes(0x12, bmp_buffer);
    height = get4bytes(0x16, bmp_buffer);
    start = get4bytes(0xa, bmp_buffer);     /* start and end are not pointers !!!! */
    kernel_printf("\n  width=%x, height=%x, start=%x\n", width, height, start);
    if (height & 0x80000000) {
        inverse = 0;
        height = -height;   
    }
    kernel_printf("\n  width=%x, height=%x, start=%x\n", width, height, start);
    buffer = (unsigned int*)kmalloc((width * height) << 2);

    if (inverse) {
        end = start;
        start = start + ((width << 2) * (height - 1));
        step = -(width << 2);
    }
    else {
        end = start + ((width << 2) * (height - 1));
        step = width << 2;
    }
    while (1) {
        bptr = start;
        if (bptr+2 >= len) {
            kernel_printf("[print_32b_bmp]The bmp is too small!\n");
            break;
        }
        for (i=0; i<width; i++) {
            b = get1byte(bptr, bmp_buffer);
            g = get1byte(bptr+1, bmp_buffer);
            r = get1byte(bptr+2, bmp_buffer);
            buffer[cnt] = ((b >> 4) << 8) + ((g >> 4) << 4) + (r >> 4);
            cnt++;
            bptr += 4;
        }
        if (start == end) break;
        start += step;
        //kernel_printf("start = %x   ", start);
    }
    if (sx & 0x80000000 || sy & 0x80000000) {
        sx = (640 - width) >> 1;
        sy = (480 - height) >> 1;
    }
    print_pure_picture(buffer, sx, sy, width, height);
    kfree(buffer);
}
