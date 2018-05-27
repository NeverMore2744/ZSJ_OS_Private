#include "file.h"
#include <zjunix/log.h>
#include <zjunix/utils.h>
#include <zjunix/type.h>

struct filedescriptor Filetable;
int allocatefd();

int allocatefd() { for (int j = 0; j < FILE_DESCRIPTOR_NUM; ++j)  if (Filetable.lookup_table[j] == 0) return j; }

void file_init()
{
	vfs_node * stdin, *stdout, *stderr;
	for (int i = 0; i < FILE_DESCRIPTOR_NUM; ++i)
		Filetable.lookup_table[i] = 0;
	char * mypath;
	mypath = (char *)kmalloc(kernel_strlen("dev/stdin") + 1);
	kernel_memcpy(mypath, "dev/stdin", kernel_strlen("dev/stdin"));
	mypath[kernel_strlen("dev/stdin")] = 0;
	stdin = vfs_find_relative_node(vfs_get_root(), mypath);
	log(LOG_OK, "Stdin's name: %s", stdin->name);
	kfree(mypath);
	mypath = (char *)kmalloc(kernel_strlen("dev/stdout") + 1);
	kernel_memcpy(mypath, "dev/stdout", kernel_strlen("dev/stdout"));
	mypath[kernel_strlen("dev/stdout")] = 0;
	stdout = vfs_find_relative_node(vfs_get_root(), mypath);
	log(LOG_OK, "Stdout's name: %s", stdout->name);
	kfree(mypath);
	mypath = (char *)kmalloc(kernel_strlen("dev/stderr") + 1);
	kernel_memcpy(mypath, "dev/stderr", kernel_strlen("dev/stderr"));
	mypath[kernel_strlen("dev/stderr")] = 0;
	stderr = vfs_find_relative_node(vfs_get_root(), mypath);
	log(LOG_OK, "Stderr's name: %s", stderr->name);
	kfree(mypath);
	Filetable.lookup_table[0] = (zFILE *)kmalloc(sizeof(zFILE)); 
	Filetable.lookup_table[1] = (zFILE *)kmalloc(sizeof(zFILE)); 
	Filetable.lookup_table[2] = (zFILE *)kmalloc(sizeof(zFILE)); 
	Filetable.lookup_table[0]->ptrnode = stdin;
	Filetable.lookup_table[1]->ptrnode = stdout;
	Filetable.lookup_table[2]->ptrnode = stderr;
	return;
}

int open(char* path, u32 capability)
{
	int fd;
	char * mypath;
	vfs_node * target;
	mypath = (char *)kmalloc(kernel_strlen(path) + 1);
	kernel_memcpy(mypath, path, kernel_strlen(path) );
	mypath[kernel_strlen(path)] = 0;
	if (mypath[0] == '/' && mypath[1] == 0) target = vfs_get_root();
	else
		if (mypath[0] == '/') {
			mypath = mypath + 1;

			if ((target = vfs_find_relative_node(vfs_get_root(), mypath)) == 0) return -1;
		}
		else return -1;
	fd = allocatefd();
	Filetable.lookup_table[fd] = (zFILE *)kmalloc(sizeof(zFILE));
	Filetable.lookup_table[fd]->ptrnode = target;
	return fd;
}

int read(int fd, u32 start, u32 count, u8 * buffer)
{
	vfs_node * node = Filetable.lookup_table[fd]->ptrnode;
	node->in_ops->vop_read(fd, node, start, count, buffer);
	return 1;
}

int write(int fd, u32 start, u32 count, const u8 * buffer)
{
	vfs_node * node = Filetable.lookup_table[fd]->ptrnode;
	node->in_ops->vop_write(fd, node, start, count, buffer);
	return 1;
}

int close(int fd)
{
	vfs_node * node = Filetable.lookup_table[fd]->ptrnode;
	node->in_ops->vop_flush(node->tag);
	kfree(Filetable.lookup_table[fd]);

}