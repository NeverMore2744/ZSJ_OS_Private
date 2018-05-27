#include <driver/vga.h>
#include <zjunix/fs/fat.h>
#include <zjunix/fs/vfs.h>
#include <zjunix/fs/file.h>
#include <zjunix/list.h>
#include <zjunix/log.h>

char *cut_front_blank(char *str) {
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

unsigned int strlen(unsigned char *str) {
    unsigned int len = 0;
    while (str[len])
        ++len;
    return len;
}

unsigned int each_param(char *para, char *word, unsigned int off, char ch) {
    int index = 0;

    while (para[off] && para[off] != ch) {
        word[index] = para[off];
        ++index;
        ++off;
    }

    word[index] = 0;

    return off;
}

int ls(char *para) {
    char pwd[128];
    struct dir_entry_attr entry;
    char name[32];
    char *p = para;
	vfs_node * ptr, *elements;
    unsigned int next;
    unsigned int r;
    unsigned int p_len;
	int fd;

    p = cut_front_blank(p);
    p_len = strlen(p);
	if (p[0] != '\"')
		next = each_param(p, pwd, 0, ' ');
	else
	{
		kernel_memcpy(pwd, p+1, p_len);
		pwd[p_len - 2] = 0;
	}
	if ((fd = open(pwd, 0)) != -1) {
		ptr = Filetable.lookup_table[fd]->ptrnode;
		struct list_head  * pch = &ptr->children;
		do
		{
			pch = list_prev(pch);
			if (pch == &ptr->children) goto LS_END;
			elements = to_struct(pch, vfs_node, thisnode);
			kernel_printf(" %s", elements->name);
		} while (1);

	LS_END:
		kernel_printf("\n");
		return 0;
	}
	else
	{
		kernel_printf("open dir(%s) failed : No such directory!\n", pwd);
		return 1;
	}

    return 0;
}
