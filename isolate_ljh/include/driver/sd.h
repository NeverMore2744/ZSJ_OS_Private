#ifndef _DRIVER_SD_H
#define _DRIVER_SD_H

#define SECSIZE 512
typedef unsigned long u32;

//function: SD¿¨×èÈû¶ÁÐ´
//SD¿¨³õÊ¼»¯²Ù×÷£ºBootloader 
//count: write single block or multiple blocks
u32 sd_read_block(unsigned char *buf, unsigned long addr, unsigned long count);
u32 sd_write_block(unsigned char *buf, unsigned long addr, unsigned long count);

#endif  // ! _DRIVER_SD_H
