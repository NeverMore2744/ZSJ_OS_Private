#ifndef ___ZSJOS__TOWER___H___
#define ___ZSJOS__TOWER___H___

//extern static unsigned int PIC_MONSTER[6][904];
//unsigned int get_tower_pic(unsigned int pic_num, unsigned int offset);
unsigned int get_tower_map(unsigned int floor, unsigned int offset);
unsigned int* get_tower_pic_ptr();

unsigned int* get_mario_pic_ptr();
unsigned int get_mario_bmp_size();

#endif