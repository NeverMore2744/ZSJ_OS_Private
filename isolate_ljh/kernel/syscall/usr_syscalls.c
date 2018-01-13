#include<zjunix/mmu/usr_syscalls.h>
#include<arch.h>
#include<driver/ps2.h>
#include<zjunix/mmu/vga_g.h>

void init_usr_syscall() {
    register_syscall(140, syscall_getchar);
    register_syscall(160, syscall_graphics);
}

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
 * 4 - clear the screen
 * 5 - clear the picture
 * 6 - show a 32-bit bmp picture
 * */

unsigned int showimg(char *para) {
	char pwd[128];
	unsigned int next;
	unsigned int p_len;
	char * p;
	u8 *buf;
	u32 file_size;
	vfs_node * ptr;
	int fd;
	p = para;
	p = img_cut_front_blank(p);
	p_len = img_strlen(p);
	next = img_each_param(p, pwd, 0, ' ');
	if ((fd = open(pwd, 0)) == -1)
	{
		kernel_printf("open file(%s) failed : No such file or directory!\n", pwd);
		return 1;
	}
	else
	{
		buf = (u8 *)dummy_memory_remove_kmalloc;
		file_size = get_entry_filesize(Filetable.lookup_table[fd]->ptrnode->mdata.node_data.entry.data);
		read(fd, 0, file_size, buf);
		cvt2Gmode();
		clear_graphics();
		clear_black();
		print_32b_bmp((unsigned char*)buf, file_size, -1, -1);
		//print_pure_picture((unsigned int *)buf, 0, 0, 512, 512);
	}
	return 0;
}


void syscall_graphics(unsigned int status, unsigned int cause, context* pt_context) {
    unsigned int code = pt_context->a0;  // $a0 = code
    unsigned int color, picw, pich, w, h;
    unsigned int* pic;
    unsigned char* pwd_bmp;
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
        case 5:
            clear_picture();
            break;
        case 6:
            pwd_bmp = pt_context -> a1;
            showimg(pwd_bmp);
            break;

        default: break;
    }
}