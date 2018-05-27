#ifndef ZJUNIX_FILE_H
#define ZJUNIX_FILE_H

#include <zjunix/fs/vfs.h>
#include <zjunix/type.h>

#define FILE_DESCRIPTOR_NUM 10
extern struct filedescriptor Filetable;
struct zFILE
{
	vfs_node * ptrnode;
};


typedef struct zFILE zFILE;

struct filedescriptor
{
	zFILE * lookup_table[FILE_DESCRIPTOR_NUM];
};


void file_init();

int open(char* path, u32 capability);

int read(int fd, u32 start, u32 count, u8 * buffer);

int write(int fd, u32 start, u32 count, const u8 * buffer);

int close(int fd);
#endif