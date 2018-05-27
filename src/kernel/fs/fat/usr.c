#include <driver/vga.h>
#include <zjunix/log.h>
#include <zjunix/slab.h>
#include <zjunix/fs/file.h>
#include "fat.h"
#include "utils.h"
extern struct filedescriptor Filetable;
u8 mk_dir_buf[32];
FILE file_create;

/* remove directory entry */
u32 fs_rm(u8 *filename) {
    u32 clus;
    u32 next_clus;
    FILE mk_dir;

    if (fs_open(&mk_dir, filename) == 1)
        goto fs_rm_err;

    /* Mark 0xE5 */
    mk_dir.entry.data[0] = 0xE5;

    /* Release all allocated block */
    clus = get_start_cluster(&mk_dir);

    while (clus != 0 && clus <= fat_info.total_data_clusters + 1) {
        if (get_fat_entry_value(clus, &next_clus) == 1)
            goto fs_rm_err;

        if (fs_modify_fat(clus, 0) == 1)
            goto fs_rm_err;

        clus = next_clus;
    }

    if (fs_close(&mk_dir) == 1)
        goto fs_rm_err;

    return 0;
fs_rm_err:
    return 1;
}

/* move directory entry */
u32 fs_mv(u8 *src, u8 *dest) {
    u32 i;
    FILE mk_dir;
    u8 filename11[13];

    /* if src not exists */
    if (fs_open(&mk_dir, src) == 1)
        goto fs_mv_err;

    /* create dest */
    if (fs_create_with_attr(dest, mk_dir.entry.data[11]) == 1)
        goto fs_mv_err;

    /* copy directory entry */
    for (i = 0; i < 32; i++)
        mk_dir_buf[i] = mk_dir.entry.data[i];

    /* new path */
    for (i = 0; i < 11; i++)
        mk_dir_buf[i] = filename11[i];

    if (fs_open(&file_create, dest) == 1)
        goto fs_mv_err;

    /* copy directory entry to dest */
    for (i = 0; i < 32; i++)
        file_create.entry.data[i] = mk_dir_buf[i];

    if (fs_close(&file_create) == 1)
        goto fs_mv_err;

    /* mark src directory entry 0xE5 */
    mk_dir.entry.data[0] = 0xE5;

    if (fs_close(&mk_dir) == 1)
        goto fs_mv_err;

    return 0;
fs_mv_err:
    return 1;
}

/* mkdir, create a new file and write . and .. */
u32 fs_mkdir(u8 *filename) {
    u32 i;
    FILE mk_dir;
    FILE file_creat;

    if (fs_create_with_attr(filename, 0x30) == 1)
        goto fs_mkdir_err;

    if (fs_open(&mk_dir, filename) == 1)
        goto fs_mkdir_err;

    mk_dir_buf[0] = '.';
    for (i = 1; i < 11; i++)
        mk_dir_buf[i] = 0x20;

    mk_dir_buf[11] = 0x30;
    for (i = 12; i < 32; i++)
        mk_dir_buf[i] = 0;

    if (fs_write(&mk_dir, mk_dir_buf, 32) == 1)
        goto fs_mkdir_err;

    fs_lseek(&mk_dir, 0);

    mk_dir_buf[20] = mk_dir.entry.data[20];
    mk_dir_buf[21] = mk_dir.entry.data[21];
    mk_dir_buf[26] = mk_dir.entry.data[26];
    mk_dir_buf[27] = mk_dir.entry.data[27];

    if (fs_write(&mk_dir, mk_dir_buf, 32) == 1)
        goto fs_mkdir_err;

    mk_dir_buf[0] = '.';
    mk_dir_buf[1] = '.';

    for (i = 2; i < 11; i++)
        mk_dir_buf[i] = 0x20;

    mk_dir_buf[11] = 0x30;
    for (i = 12; i < 32; i++)
        mk_dir_buf[i] = 0;

    set_u16(mk_dir_buf + 20, (file_creat.dir_entry_pos >> 16) & 0xFFFF);
    set_u16(mk_dir_buf + 26, file_creat.dir_entry_pos & 0xFFFF);

    if (fs_write(&mk_dir, mk_dir_buf, 32) == 1)
        goto fs_mkdir_err;

    for (i = 28; i < 32; i++)
        mk_dir.entry.data[i] = 0;

    if (fs_close(&mk_dir) == 1)
        goto fs_mkdir_err;

    return 0;
fs_mkdir_err:
    return 1;
}

//static char dummy_memory_remove_kmalloc[32768];


u32 fs_cat(u8 *path) {
    u8 filename[12];
	char * mypath;
	u32 file_size;
	int fd;
	mypath = (char *)kmalloc(kernel_strlen(path) + 1);
	kernel_memcpy(mypath, path, kernel_strlen(path));
	mypath[kernel_strlen(path)] = 0;
	
    /* Open */
    if ((fd = open(path, 1)) == -1) {
        log(LOG_FAIL, "File %s open failed", path);
        return 1;
    }
	file_size = get_entry_filesize(Filetable.lookup_table[fd]->ptrnode->mdata.node_data.entry.data);
	u8 *buf = (u8 *)kmalloc(file_size + 4);//(u8 *)kmalloc(file_size + 1);
	read(fd, 0, file_size, buf);
    buf[file_size] = 0;
    kernel_printf("%s\n", buf);
    kfree(buf);
    close(fd);
    return 0;
}

u32 writetest() {
	u32 file_size;
	int fd;
	fd = open("/user/A.TXT", 1);
	file_size = get_entry_filesize(Filetable.lookup_table[fd]->ptrnode->mdata.node_data.entry.data);
	u8 *buf = (u8 *)kmalloc(file_size + 16);
	read(fd, 0, file_size, buf);
	buf[file_size] = 0;
	buf[file_size - 1] = buf[file_size - 1] + 1 > '9' ? '0' : buf[file_size - 1] + 1;
	write(fd, 0, file_size, buf);
    kfree(buf);
	close(fd);
	return 0;
}


