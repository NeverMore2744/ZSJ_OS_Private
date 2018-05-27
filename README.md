## ZSJ_OS_Private

It was private before. But now the course has finished, so I made it public.

ZSJ = "珠三角". 3 members from "ZSJ" in total are in charge of finishing this project.

ZSJ_OS is developed based on ZJUNIX, see https://github.com/ZJUNIX/ZJUNIX-Exp. 

It is an mini-OS running on a FPGA board.

**My work**:

+ **src/kernel/mm/**: The memory management modules
+ **src/kernel/driver/vga_graphics.c**: The VGA graphics controller
+ **src/kernel/syscall/usr_syscalls.c**: The VGA system calls
+ **src/usr/showimg.c, showimg.h**: Functions of displaying a BMP picture on the screen.