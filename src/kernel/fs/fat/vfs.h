#ifndef ZJUNIX_VFS_H
#define ZJUNIX_VFS_H
#include <zjunix/fs/vfs.h>

/* VFS flags */
#define O_RDONLY                                0            // open for reading only
#define O_WRONLY                                0x00000001   // open for writing only
#define O_RDWR                                  0x00000002   // open for reading and writing 
#define O_CREAT                                 0x00000004   // create file if it doesn't exist
#define O_EXCL                                  0x00000008   // error if O_CREAT and the file exist
#define O_TRUNC                                 0x00000010   // truncate file upon open
#define O_APPEND                                0x00000020   // append on each write
#define FS_MAX_DNAME_LEN                        31  


#endif