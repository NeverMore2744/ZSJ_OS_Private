#include "showimg.h"
#include <driver/ps2.h>
#include <driver/vga.h>
#include <zjunix/fs/fat.h>
#include <zjunix/ljh/vga_g.h>
#include <zjunix/log.h>
#include <arch.h>


char *img_cut_front_blank(char *str) {
	char *s = str;
	unsigned int index = 0;

	while (*s == ' ') {
		++s;
		++index;
	}

	if (!index)
		return str;

	while (*s) {
		*(s - index) = *s;
		++s;
	}

	--s;
	*s = 0;

	return str;
}

unsigned int img_strlen(unsigned char *str) {
	unsigned int len = 0;
	while (str[len])
		++len;
	return len;
}

unsigned int img_each_param(char *para, char *word, unsigned int off, char ch) {
	int index = 0;

	while (para[off] && para[off] != ch) {
		word[index] = para[off];
		++index;
		++off;
	}

	word[index] = 0;

	return off;
}

unsigned int showimg(char *para)
{
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
		file_size = get_entry_filesize(Filetable.lookup_table[fd]->ptrnode->mdata.node_data.entry.data);
		buf = (u8 *)kmalloc(file_size + 4);
		read(fd, 0, file_size, buf);
		cvt2Gmode();
		clear_graphics();
		//clear_black();
		print_32b_bmp((unsigned char*)buf, file_size, -1, -1);
		//print_pure_picture((unsigned int *)buf, 0, 0, 512, 512);
	}
	return 0;
}
