#ifndef ___VGA_G__
#define ___VGA_G__

void print_rectangle(unsigned int color, unsigned int pic_width, 
    unsigned int pic_height, unsigned int width, unsigned int height);

void init_vga_graphics();

// Print a picture, using "pic" as buffer
void print_pure_picture(unsigned int* pic, unsigned int pic_width, 
    unsigned int pic_height, unsigned int width, unsigned int height);

unsigned int print_pixel(unsigned int w, unsigned int h, unsigned int pixel);

// Clear the screen
void clear_black();
void clear_graphics();
// Convert to Graphics mode
void cvt2Gmode();
void cvt2Tmode();

void print_32b_bmp(unsigned char* bmp_buffer, unsigned int len, unsigned int sx, unsigned int sy);

#endif  // ___VGA_G__
