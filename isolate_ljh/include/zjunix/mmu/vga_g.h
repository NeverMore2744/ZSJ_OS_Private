#ifndef ___VGA_G__
#define ___VGA_G__

void print_rectangle(unsigned int color, unsigned int pic_width, 
    unsigned int pic_height, unsigned int width, unsigned int height);

void print_pure_picture(unsigned char* pic, unsigned int pic_width, 
    unsigned int pic_height, unsigned int width, unsigned int height);

unsigned int print_pixel(unsigned int w, unsigned int h, unsigned int pixel);

void print_rectangle(unsigned int color, unsigned int pic_width, 
    unsigned int pic_height, unsigned int width, unsigned int height);

void clear_black();

void cvt2Gmode();

#endif  // ___VGA_G__
