#include "fat.h"
#include "vfs.h"
#include <driver/vga.h>
#include <zjunix/log.h>
#include "utils.h"


#ifdef FS_DEBUG
#include <intr.h>
#include <zjunix/log.h>
#include "debug.h"
#endif  // ! FS_DEBUG

extern PAGES PAGE_BUFFER;
/* fat buffer clock head */
u32 fat_clock_head = 0;
BUF_512 fat_buf[FAT_BUF_NUM];

u8 filename11[13];
u8 new_alloc_empty[PAGE_SIZE];

#define DIR_DATA_BUF_NUM 4
BUF_512 dir_data_buf[DIR_DATA_BUF_NUM];
u32 dir_data_clock_head = 0;

struct fs_info fat_info;

unsigned long  fat_fs_open(struct vfs_node *node, u32 open_flags);
unsigned long  fat_fs_close(struct vfs_node *node);

void fat_fs_get_short_name(struct dir_entry_attr* entry, char buffer[13]);

u8 fat_fs_read_by_data_cluster(vfs_node * mount_point, u32 offset, BUF_4K * ptr_buffer);

u32 fat_to_vfs_attr(u32 fat_attr);
u32 fat_fs_get_entry_data_cluster(struct dir_entry_attr* e);

u8 fat_fs_read_page(vfs_node * mount_point, vfs_node * file, u32 clus_start, void * buf);
u8 fat_fs_write_by_page(vfs_node * mount_point, vfs_node * file, u32 clus, const u8* buf, u32 count);
u32 fat_fs_alloc(u32 *new_alloc, vfs_node * mount_point);

unsigned long  fat_fs_read(u32 fd, struct vfs_node *node, u32 start, u32 count, void *iob);
unsigned long  fat_fs_write(u32 fd, struct vfs_node *node, u32 start, u32 count, const u8 *buf);
unsigned long  fat_fs_sync(u32 fd, struct vfs_node *node, u32 start_page, u32 end_page);
unsigned long  fat_fs_lookup(struct vfs_node *node, char * path, struct vfs_node ** result);
unsigned long  fat_fs_flush(struct vfs_node * mount_point);

static struct inode_ops fat_ops =
{
	fat_fs_open,
	NULL,
	fat_fs_read,
	fat_fs_write,
	fat_fs_sync,
	NULL,
	fat_fs_flush
};

// Init, get the MBR and load the LBA. almost
u32 init_fat_info(struct fs_info *fat_info) {   // slightly modified
												/* DBR 引导扇区 512字节,存在meta_buf里 */
	u8 meta_buf[512];

	/* Init bufs, 512byte DBR + fs_info(stored in memory) */
	kernel_memset(meta_buf, 0, sizeof(meta_buf));
	kernel_memset(fat_info, 0, sizeof(struct fs_info));

	/* Get MBR sector */
	/* 调用read_block -> sd_read_block, 读入一个block, starts from 0, 1 block = 512 bytes */
	if (read_block(meta_buf, 0, 1) == 1) {
		goto init_fat_info_err;
	}

	log(LOG_OK, "Get MBR sector info");

	/* MBR partition 1 entry starts from +446, and LBA starts from +8 */
	/* Get base_addr */
	fat_info->base_addr = get_u32(meta_buf + 446 + 8);

	/* Get FAT BPB  BIOS Paramter Block */
	if (read_block(fat_info->BPB.data, fat_info->base_addr, 1) == 1)
		goto init_fat_info_err;
	/* FAT BPB read, BPB 手动内存对齐 */

	log(LOG_OK, "Get FAT BPB");
#ifdef FS_DEBUG
	dump_bpb_info(&(fat_info->BPB.attr));
#endif

	/* Sector size (MBR[11]) must be SECTOR_SIZE bytes */
	if (fat_info->BPB.attr.sector_size != SECTOR_SIZE) {
		log(LOG_FAIL, "FAT32 Sector size must be %d bytes, but get %d bytes.", SECTOR_SIZE, fat_info->BPB.attr.sector_size);
		goto init_fat_info_err;
	}

	/* Determine FAT type */
	/* For FAT32, max root dir entries must be 0 */
	if (fat_info->BPB.attr.max_root_dir_entries != 0) {
		goto init_fat_info_err;
	}
	/* For FAT32, sectors per fat at BPB[0x16] is 0 */
	if (fat_info->BPB.attr.sectors_per_fat != 0) {
		goto init_fat_info_err;
	}
	/* For FAT32, total sectors at BPB[0x16] is 0 */
	if (fat_info->BPB.attr.num_of_small_sectors != 0) {
		goto init_fat_info_err;
	}

	/* If not FAT32, goto error state  以上 以下代码都是检测是否是合法的fat 32 系统*/
	u32 total_sectors = fat_info->BPB.attr.num_of_sectors;
	u32 reserved_sectors = fat_info->BPB.attr.reserved_sectors;  //保留扇区数
	u32 sectors_per_fat = fat_info->BPB.attr.num_of_sectors_per_fat; //每FAT 扇区数
	u32 total_data_sectors = total_sectors - reserved_sectors - sectors_per_fat * 2;
	u8 sectors_per_cluster = fat_info->BPB.attr.sectors_per_cluster;
	fat_info->total_data_clusters = total_data_sectors / sectors_per_cluster;
	if (fat_info->total_data_clusters < 65525) {
		goto init_fat_info_err;
	}

	/* Get root dir sector */
	fat_info->first_data_sector = reserved_sectors + sectors_per_fat * 2;
	log(LOG_OK, "Partition type determined: FAT32");

	/* Keep FSInfo in buf */
	read_block(fat_info->fat_fs_info, 1 + fat_info->base_addr, 1);
	log(LOG_OK, "Get FSInfo sector");
	//读取fat_fs_info
	//
#ifdef FS_DEBUG
	dump_fat_info(fat_info);
#endif

	/* Init success */
	return 0;

init_fat_info_err:
	return 1;
}

void init_fat_buf() {
    int i = 0;
    for (i = 0; i < FAT_BUF_NUM; i++) {
        fat_buf[i].cur = 0xffffffff;
        fat_buf[i].state = 0;
    }
}

void init_dir_buf() {
    int i = 0;
    for (i = 0; i < DIR_DATA_BUF_NUM; i++) {
        dir_data_buf[i].cur = 0xffffffff;
        dir_data_buf[i].state = 0;
    }
}

void init_page_buf() {
	int i = 0;
	for (i = 0; i < PAGE_BUFFER_NUM; i++) {
		PAGE_BUFFER.page[i].cur = 0xffffffff;
		PAGE_BUFFER.page[i].state = 0;
	}
}

/* FAT Initialize */
u32 init_fs() {
    u32 succ = init_fat_info(&fat_info);
    if (0 != succ)
        goto fs_init_err;
    init_fat_buf();
    init_dir_buf();
    return 0;

fs_init_err:
    log(LOG_FAIL, "File system init fail.");
    return 1;
}

/* Write current fat sector */
u32 write_fat_sector(u32 index) {
    if ((fat_buf[index].cur != 0xffffffff) && (((fat_buf[index].state) & 0x02) != 0)) {
        /* Write FAT and FAT copy */
        if (write_block(fat_buf[index].buf, fat_buf[index].cur, 1) == 1)
            goto write_fat_sector_err;
        if (write_block(fat_buf[index].buf, fat_info.BPB.attr.num_of_sectors_per_fat + fat_buf[index].cur, 1) == 1)
            goto write_fat_sector_err;
        fat_buf[index].state &= 0x01;
    }
    return 0;
write_fat_sector_err:
    return 1;
}

/* Read fat sector */
u32 read_fat_sector(u32 ThisFATSecNum) {
    u32 index;
    /* try to find in buffer */
    for (index = 0; (index < FAT_BUF_NUM) && (fat_buf[index].cur != ThisFATSecNum); index++)
        ;

    /* if not in buffer, find victim & replace, otherwise set reference bit */
    if (index == FAT_BUF_NUM) {
        index = fs_victim_512(fat_buf, &fat_clock_head, FAT_BUF_NUM);

        if (write_fat_sector(index) == 1)
            goto read_fat_sector_err;

        if (read_block(fat_buf[index].buf, ThisFATSecNum, 1) == 1)
            goto read_fat_sector_err;

        fat_buf[index].cur = ThisFATSecNum;
        fat_buf[index].state = 1;
    } else
        fat_buf[index].state |= 0x01;

    return index;
read_fat_sector_err:
    return 0xffffffff;
}

/* path convertion */
u32 fs_next_slash(u8 *f) {
    u32 i, j, k;
    u8 chr11[13];
    for (i = 0; (*(f + i) != 0) && (*(f + i) != '/'); i++)
        ;

    for (j = 0; j < 12; j++) {
        chr11[j] = 0;
        filename11[j] = 0x20;
    }
    for (j = 0; j < 12 && j < i; j++) {
        chr11[j] = *(f + j);
        if (chr11[j] >= 'a' && chr11[j] <= 'z')
            chr11[j] = (u8)(chr11[j] - 'a' + 'A');
    }
    chr11[12] = 0;

    for (j = 0; (chr11[j] != 0) && (j < 12); j++) {
        if (chr11[j] == '.')
            break;

        filename11[j] = chr11[j];
    }

    if (chr11[j] == '.') {
        j++;
        for (k = 8; (chr11[j] != 0) && (j < 12) && (k < 11); j++, k++) {
            filename11[k] = chr11[j];
        }
    }

    filename11[11] = 0;

    return i;
}

/* strcmp */
u32 fs_cmp_filename(const u8 *f1, const u8 *f2) {
    u32 i;
    for (i = 0; i < 11; i++) {
        if (f1[i] != f2[i])
            return 1;
    }

    return 0;
}

/* Find a file, only absolute path with starting '/' accepted */
u32 fs_find(FILE *file) {
    u8 *f = file->path;
    u32 next_slash;
    u32 i, k;
    u32 next_clus;
    u32 index;
    u32 sec;

    if (*(f++) != '/')
        goto fs_find_err;

    index = fs_read_512(dir_data_buf, fs_dataclus2sec(2), &dir_data_clock_head, DIR_DATA_BUF_NUM);
    /* Open root directory */
    if (index == 0xffffffff)
        goto fs_find_err;

    /* Find directory entry */
    while (1) {
        file->dir_entry_pos = 0xFFFFFFFF;

        next_slash = fs_next_slash(f);

        while (1) {
            for (sec = 1; sec <= fat_info.BPB.attr.sectors_per_cluster; sec++) {
                /* Find directory entry in current cluster */
                for (i = 0; i < 512; i += 32) {
                    if (*(dir_data_buf[index].buf + i) == 0)
                        goto after_fs_find;

                    /* Ignore long path */
                    if ((fs_cmp_filename(dir_data_buf[index].buf + i, filename11) == 0) &&
                        ((*(dir_data_buf[index].buf + i + 11) & 0x08) == 0)) {
                        file->dir_entry_pos = i;
                        // refer to the issue in fs_close()
                        file->dir_entry_sector = dir_data_buf[index].cur - fat_info.base_addr;

                        for (k = 0; k < 32; k++)
                            file->entry.data[k] = *(dir_data_buf[index].buf + i + k);

                        goto after_fs_find;
                    }
                }
                /* next sector in current cluster */
                if (sec < fat_info.BPB.attr.sectors_per_cluster) {
                    index = fs_read_512(dir_data_buf, dir_data_buf[index].cur + 1, &dir_data_clock_head, DIR_DATA_BUF_NUM);
                    if (index == 0xffffffff)
                        goto fs_find_err;
                } else {
                    /* Read next cluster of current directory */
                    if (get_fat_entry_value(dir_data_buf[index].cur - fat_info.BPB.attr.sectors_per_cluster + 1, &next_clus) == 1)
                        goto fs_find_err;

                    if (next_clus <= fat_info.total_data_clusters + 1) {
                        index = fs_read_512(dir_data_buf, fs_dataclus2sec(next_clus), &dir_data_clock_head, DIR_DATA_BUF_NUM);
                        if (index == 0xffffffff)
                            goto fs_find_err;
                    } else
                        goto after_fs_find;
                }
            }
        }

    after_fs_find:
        /* If not found */
        if (file->dir_entry_pos == 0xFFFFFFFF)
            goto fs_find_ok;

        /* If path parsing completes */
        if (f[next_slash] == 0)
            goto fs_find_ok;

        /* If not a sub directory */
        if ((file->entry.data[11] & 0x10) == 0)
            goto fs_find_err;

        f += next_slash + 1;

        /* Open sub directory, high word(+20), low word(+26) */
        next_clus = get_start_cluster(file);

        if (next_clus <= fat_info.total_data_clusters + 1) {
            index = fs_read_512(dir_data_buf, fs_dataclus2sec(next_clus), &dir_data_clock_head, DIR_DATA_BUF_NUM);
            if (index == 0xffffffff)
                goto fs_find_err;
        } else
            goto fs_find_err;
    }
fs_find_ok:
    return 0;
fs_find_err:
    return 1;
}

/* Open: just do initializing & fs_find */
u32 fs_open(FILE *file, u8 *filename) {
    u32 i;

    /* Local buffer initialize */
    for (i = 0; i < LOCAL_DATA_BUF_NUM; i++) {
        file->data_buf[i].cur = 0xffffffff;
        file->data_buf[i].state = 0;
    }

    file->clock_head = 0;

    for (i = 0; i < 256; i++)
        file->path[i] = 0;
    for (i = 0; i < 256 && filename[i] != 0; i++)
        file->path[i] = filename[i];

    file->loc = 0;

    if (fs_find(file) == 1)
        goto fs_open_err;

    /* If file not exists */
    if (file->dir_entry_pos == 0xFFFFFFFF)
        goto fs_open_err;

    return 0;
fs_open_err:
    return 1;
}
/* fflush, write global buffers to sd */
u32 fs_fflush() {
    u32 i;

    // FSInfo shoud add base_addr
    if (write_block(fat_info.fat_fs_info, 1 + fat_info.base_addr, 1) == 1)
        goto fs_fflush_err;

    if (write_block(fat_info.fat_fs_info, 7 + fat_info.base_addr, 1) == 1)
        goto fs_fflush_err;

    for (i = 0; i < FAT_BUF_NUM; i++)
        if (write_fat_sector(i) == 1)
            goto fs_fflush_err;

    for (i = 0; i < DIR_DATA_BUF_NUM; i++)
        if (fs_write_512(dir_data_buf + i) == 1)
            goto fs_fflush_err;

    return 0;

fs_fflush_err:
    return 1;
}

/* Close: write all buf in memory to SD */
u32 fs_close(FILE *file) {
    u32 i;
    u32 index;

    /* Write directory entry */
    index = fs_read_512(dir_data_buf, file->dir_entry_sector, &dir_data_clock_head, DIR_DATA_BUF_NUM);
    if (index == 0xffffffff)
        goto fs_close_err;

    dir_data_buf[index].state = 3;

    // Issue: need file->dir_entry to be local partition offset
    for (i = 0; i < 32; i++)
        *(dir_data_buf[index].buf + file->dir_entry_pos + i) = file->entry.data[i];
    /* do fflush to write global buffers */
    if (fs_fflush() == 1)
        goto fs_close_err;
    /* write local data buffer */
    for (i = 0; i < LOCAL_DATA_BUF_NUM; i++)
        if (fs_write_4k(file->data_buf + i) == 1)
            goto fs_close_err;

    return 0;
fs_close_err:
    return 1;
}

/* Read from file */
u32 fs_read(FILE *file, u8 *buf, u32 count) {
    u32 start_clus, start_byte;
    u32 end_clus, end_byte;
    u32 filesize = file->entry.attr.size;
    u32 clus = get_start_cluster(file);
    u32 next_clus;
    u32 i;
    u32 cc;
    u32 index;

#ifdef FS_DEBUG
    kernel_printf("fs_read: count %d\n", count);
    disable_interrupts();
#endif  // ! FS_DEBUG
    /* If file is empty */
    if (clus == 0)
        return 0;

    /* If loc + count > filesize, only up to EOF will be read */
    if (file->loc + count > filesize)
        count = filesize - file->loc;

    /* If read 0 byte */
    if (count == 0)
        return 0;

    start_clus = file->loc >> fs_wa(fat_info.BPB.attr.sectors_per_cluster << 9);
    start_byte = file->loc & ((fat_info.BPB.attr.sectors_per_cluster << 9) - 1);
    end_clus = (file->loc + count - 1) >> fs_wa(fat_info.BPB.attr.sectors_per_cluster << 9);
    end_byte = (file->loc + count - 1) & ((fat_info.BPB.attr.sectors_per_cluster << 9) - 1);

#ifdef FS_DEBUG
    kernel_printf("start cluster: %d\n", start_clus);
    kernel_printf("start byte: %d\n", start_byte);
    kernel_printf("end cluster: %d\n", end_clus);
    kernel_printf("end byte: %d\n", end_byte);
#endif  // ! FS_DEBUG
    /* Open first cluster to read */
    for (i = 0; i < start_clus; i++) {
        if (get_fat_entry_value(clus, &next_clus) == 1)
            goto fs_read_err;

        clus = next_clus;
    }

    cc = 0;
    while (start_clus <= end_clus) {
        index = fs_read_4k(file->data_buf, fs_dataclus2sec(clus), &(file->clock_head), LOCAL_DATA_BUF_NUM);
        if (index == 0xffffffff)
            goto fs_read_err;

        /* If in same cluster, just read */
        if (start_clus == end_clus) {
            for (i = start_byte; i <= end_byte; i++)
                buf[cc++] = file->data_buf[index].buf[i];
            goto fs_read_end;
        }
        /* otherwise, read clusters one by one */
        else {
            for (i = start_byte; i < (fat_info.BPB.attr.sectors_per_cluster << 9); i++)
                buf[cc++] = file->data_buf[index].buf[i];

            start_clus++;
            start_byte = 0;

            if (get_fat_entry_value(clus, &next_clus) == 1)
                goto fs_read_err;

            clus = next_clus;
        }
    }
fs_read_end:

#ifdef FS_DEBUG
    kernel_printf("fs_read: count %d\n", count);
    enable_interrupts();
#endif  // ! FS_DEBUG
    /* modify file pointer */
    file->loc += count;
    return cc;
fs_read_err:
    return 0xFFFFFFFF;
}

/* Find a free data cluster */
u32 fs_next_free(u32 start, u32 *next_free) {
    u32 clus;
    u32 ClusEntryVal;

    *next_free = 0xFFFFFFFF;

    for (clus = start; clus <= fat_info.total_data_clusters + 1; clus++) {
        if (get_fat_entry_value(clus, &ClusEntryVal) == 1)
            goto fs_next_free_err;

        if (ClusEntryVal == 0) {
            *next_free = clus;
            break;
        }
    }

    return 0;
fs_next_free_err:
    return 1;
}

/* Alloc a new free data cluster */
u32 fs_alloc(u32 *new_alloc) {
    u32 clus;
    u32 next_free;

    clus = get_u32(fat_info.fat_fs_info + 492) + 1;

    /* If FSI_Nxt_Free is illegal (> FSI_Free_Count), find a free data cluster
     * from beginning */
    if (clus > get_u32(fat_info.fat_fs_info + 488) + 1) {
        if (fs_next_free(2, &clus) == 1)
            goto fs_alloc_err;

        if (fs_modify_fat(clus, 0xFFFFFFFF) == 1)
            goto fs_alloc_err;
    }

    /* FAT allocated and update FSI_Nxt_Free */
    if (fs_modify_fat(clus, 0xFFFFFFFF) == 1)
        goto fs_alloc_err;

    if (fs_next_free(clus, &next_free) == 1)
        goto fs_alloc_err;

    /* no available free cluster */
    if (next_free > fat_info.total_data_clusters + 1)
        goto fs_alloc_err;

    set_u32(fat_info.fat_fs_info + 492, next_free - 1);

    *new_alloc = clus;

    /* Erase new allocated cluster */
    if (write_block(new_alloc_empty, fs_dataclus2sec(clus), fat_info.BPB.attr.sectors_per_cluster) == 1)
        goto fs_alloc_err;

    return 0;
fs_alloc_err:
    return 1;
}

/* Write to file */
u32 fs_write(FILE *file, const u8 *buf, u32 count) {
    /* If write 0 bytes */
    if (count == 0) {
        return 0;
    }

    u32 start_clus = file->loc >> fs_wa(fat_info.BPB.attr.sectors_per_cluster << 9);
    u32 start_byte = file->loc & ((fat_info.BPB.attr.sectors_per_cluster << 9) - 1);
    u32 end_clus = (file->loc + count - 1) >> fs_wa(fat_info.BPB.attr.sectors_per_cluster << 9);
    u32 end_byte = (file->loc + count - 1) & ((fat_info.BPB.attr.sectors_per_cluster << 9) - 1);

    /* If file is empty, alloc a new data cluster */
    u32 curr_cluster = get_start_cluster(file);
    if (curr_cluster == 0) {
        if (fs_alloc(&curr_cluster) == 1) {
            goto fs_write_err;
        }
        file->entry.attr.starthi = (u16)(((curr_cluster >> 16) & 0xFFFF));
        file->entry.attr.startlow = (u16)((curr_cluster & 0xFFFF));
        if (fs_clr_4k(file->data_buf, &(file->clock_head), LOCAL_DATA_BUF_NUM, fs_dataclus2sec(curr_cluster)) == 1)
            goto fs_write_err;
    }

    /* Open first cluster to read */
    u32 next_cluster;
    for (u32 i = 0; i < start_clus; i++) {
        if (get_fat_entry_value(curr_cluster, &next_cluster) == 1)
            goto fs_write_err;

        /* If this is the last cluster in file, and still need to open next
         * cluster, just alloc a new data cluster */
        if (next_cluster > fat_info.total_data_clusters + 1) {
            if (fs_alloc(&next_cluster) == 1)
                goto fs_write_err;

            if (fs_modify_fat(curr_cluster, next_cluster) == 1)
                goto fs_write_err;

            if (fs_clr_4k(file->data_buf, &(file->clock_head), LOCAL_DATA_BUF_NUM, fs_dataclus2sec(next_cluster)) == 1)
                goto fs_write_err;
        }

        curr_cluster = next_cluster;
    }

    u32 cc = 0;
    u32 index = 0;
    while (start_clus <= end_clus) {
        index = fs_read_4k(file->data_buf, fs_dataclus2sec(curr_cluster), &(file->clock_head), LOCAL_DATA_BUF_NUM);
        if (index == 0xffffffff)
            goto fs_write_err;

        file->data_buf[index].state = 3;

        /* If in same cluster, just write */
        if (start_clus == end_clus) {
            for (u32 i = start_byte; i <= end_byte; i++)
                file->data_buf[index].buf[i] = buf[cc++];
            goto fs_write_end;
        }
        /* otherwise, write clusters one by one */
        else {
            for (u32 i = start_byte; i < (fat_info.BPB.attr.sectors_per_cluster << 9); i++)
                file->data_buf[index].buf[i] = buf[cc++];

            start_clus++;
            start_byte = 0;

            if (get_fat_entry_value(curr_cluster, &next_cluster) == 1)
                goto fs_write_err;

            /* If this is the last cluster in file, and still need to open next
             * cluster, just alloc a new data cluster */
            if (next_cluster > fat_info.total_data_clusters + 1) {
                if (fs_alloc(&next_cluster) == 1)
                    goto fs_write_err;

                if (fs_modify_fat(curr_cluster, next_cluster) == 1)
                    goto fs_write_err;

                if (fs_clr_4k(file->data_buf, &(file->clock_head), LOCAL_DATA_BUF_NUM, fs_dataclus2sec(next_cluster)) == 1)
                    goto fs_write_err;
            }

            curr_cluster = next_cluster;
        }
    }

fs_write_end:

    /* update file size */
    if (file->loc + count > file->entry.attr.size)
        file->entry.attr.size = file->loc + count;

    /* update location */
    file->loc += count;

    return cc;
fs_write_err:
    return 0xFFFFFFFF;
}

/* lseek */
void fs_lseek(FILE *file, u32 new_loc) {
    u32 filesize = file->entry.attr.size;

    if (new_loc < filesize)
        file->loc = new_loc;
    else
        file->loc = filesize;
}

/* find an empty directory entry */
u32 fs_find_empty_entry(u32 *empty_entry, u32 index) {
    u32 i;
    u32 next_clus;
    u32 sec;

    while (1) {
        for (sec = 1; sec <= fat_info.BPB.attr.sectors_per_cluster; sec++) {
            /* Find directory entry in current cluster */
            for (i = 0; i < 512; i += 32) {
                /* If entry is empty */
                if ((*(dir_data_buf[index].buf + i) == 0) || (*(dir_data_buf[index].buf + i) == 0xE5)) {
                    *empty_entry = i;
                    goto after_fs_find_empty_entry;
                }
            }

            if (sec < fat_info.BPB.attr.sectors_per_cluster) {
                index = fs_read_512(dir_data_buf, dir_data_buf[index].cur + sec, &dir_data_clock_head, DIR_DATA_BUF_NUM);
                if (index == 0xffffffff)
                    goto fs_find_empty_entry_err;
            } else {
                /* Read next cluster of current directory */
                if (get_fat_entry_value(dir_data_buf[index].cur - fat_info.BPB.attr.sectors_per_cluster + 1, &next_clus) == 1)
                    goto fs_find_empty_entry_err;

                /* need to alloc a new cluster */
                if (next_clus > fat_info.total_data_clusters + 1) {
                    if (fs_alloc(&next_clus) == 1)
                        goto fs_find_empty_entry_err;

                    if (fs_modify_fat(fs_sec2dataclus(dir_data_buf[index].cur - fat_info.BPB.attr.sectors_per_cluster + 1), next_clus) == 1)
                        goto fs_find_empty_entry_err;

                    *empty_entry = 0;

                    if (fs_clr_512(dir_data_buf, &dir_data_clock_head, DIR_DATA_BUF_NUM, fs_dataclus2sec(next_clus)) == 1)
                        goto fs_find_empty_entry_err;
                }

                index = fs_read_512(dir_data_buf, fs_dataclus2sec(next_clus), &dir_data_clock_head, DIR_DATA_BUF_NUM);
                if (index == 0xffffffff)
                    goto fs_find_empty_entry_err;
            }
        }
    }

after_fs_find_empty_entry:
    return index;
fs_find_empty_entry_err:
    return 0xffffffff;
}

/* create an empty file with attr */
u32 fs_create_with_attr(u8 *filename, u8 attr) {
    u32 i;
    u32 l1 = 0;
    u32 l2 = 0;
    u32 empty_entry;
    u32 clus;
    u32 index;
    FILE file_creat;
    /* If file exists */
    if (fs_open(&file_creat, filename) == 0)
        goto fs_creat_err;

    for (i = 255; i >= 0; i--)
        if (file_creat.path[i] != 0) {
            l2 = i;
            break;
        }

    for (i = 255; i >= 0; i--)
        if (file_creat.path[i] == '/') {
            l1 = i;
            break;
        }

    /* If not root directory, find that directory */
    if (l1 != 0) {
        for (i = l1; i <= l2; i++)
            file_creat.path[i] = 0;

        if (fs_find(&file_creat) == 1)
            goto fs_creat_err;

        /* If path not found */
        if (file_creat.dir_entry_pos == 0xFFFFFFFF)
            goto fs_creat_err;

        clus = get_start_cluster(&file_creat);
        /* Open that directory */
        index = fs_read_512(dir_data_buf, fs_dataclus2sec(clus), &dir_data_clock_head, DIR_DATA_BUF_NUM);
        if (index == 0xffffffff)
            goto fs_creat_err;

        file_creat.dir_entry_pos = clus;
    }
    /* otherwise, open root directory */
    else {
        index = fs_read_512(dir_data_buf, fs_dataclus2sec(2), &dir_data_clock_head, DIR_DATA_BUF_NUM);
        if (index == 0xffffffff)
            goto fs_creat_err;

        file_creat.dir_entry_pos = 2;
    }

    /* find an empty entry */
    index = fs_find_empty_entry(&empty_entry, index);
    if (index == 0xffffffff)
        goto fs_creat_err;

    for (i = l1 + 1; i <= l2; i++)
        file_creat.path[i - l1 - 1] = filename[i];

    file_creat.path[l2 - l1] = 0;
    fs_next_slash(file_creat.path);

    dir_data_buf[index].state = 3;

    /* write path */
    for (i = 0; i < 11; i++)
        *(dir_data_buf[index].buf + empty_entry + i) = filename11[i];

    /* write file attr */
    *(dir_data_buf[index].buf + empty_entry + 11) = attr;

    /* other should be zero */
    for (i = 12; i < 32; i++)
        *(dir_data_buf[index].buf + empty_entry + i) = 0;

    if (fs_fflush() == 1)
        goto fs_creat_err;

    return 0;
fs_creat_err:
    return 1;
}

u32 fs_create(u8 *filename) {
    return fs_create_with_attr(filename, 0x20);
}




//vfs_node* vfs_create_node(char *filename, bool copy_name, u32 attributes, u32 capabilities, \
							u32 file_length, void * m_data, vfs_node * tag, vfs_node * parent,  \ tag = device_node 
//							inode_ops* fileops)   
//
struct list_head fat_fs_read_directory(vfs_node* mount_point, u32 current_cluster, vfs_node* parent);

//struct list_head fat_fs_read_directory(vfs_node* mount_point, u32 current_cluster, vfs_node* parent){}


unsigned long fat_fs_open(vfs_node* node, u32 capabilities)
{
	if (!node->tag->tag)
	{
		log(LOG_FAIL, "Mount point's device not found");
		while (1);
	}
	vfs_node *mount_point = node->tag;
	// Currently doing nothing here.
	return 1;

}

// 
unsigned long  fat_fs_write(u32 fd, struct vfs_node *node, u32 start, u32 count, const u8* buf)
{
	u32 i;
	u32 pages;
	u32 rem;
	u32 next_clus;
	u32 index;
	struct dir_entry_attr * entry;
	struct fat_mount_data * mount_data = &node->tag->mdata.mount_data;
	u32 clus = get_entry_start_cluster(node->mdata.node_data.entry.data);
	if (count == 0 || clus == 0)
		return 0;
	vfs_node* mount_point = node->tag;
	pages = count >> 12;
	if ((count & 4095) > 0) pages = pages + 1;
	for (i = 0; i < pages; i++)
	{
		if (fat_fs_write_by_page(mount_point, node, clus, buf + (i << 12), count) == 0)
		{
			log(LOG_FAIL, "write_page error");
			while (1);
		}
		get_fat_entry_value(clus, &next_clus);
		if (next_clus > FAT_EOF)
			if (fat_fs_alloc(&next_clus, node->tag) != 0)
				return -1;
		if (fs_modify_fat(clus, next_clus) == 1)
			return -1;
		clus = next_clus;
	}
	node->mdata.node_data.entry.attr.size = count;
	index = fs_read_4k(PAGE_BUFFER.page, clus2sec(node->mdata.node_data.metadata_cluster, &mount_data->fat_info), &PAGE_BUFFER.clock_head, 10);
	entry = (struct dir_entry_attr *)PAGE_BUFFER.page[index].buf;
	entry[node->mdata.node_data.metadata_index].size = count;
	PAGE_BUFFER.page[index].state = 3;
	return count;
}

u8 fat_fs_write_by_page(vfs_node * mount_point, vfs_node * file, u32 clus, const u8* buf, u32 count)
{
	u32 index;
	u32 i, cc = 0;
	struct fat_mount_data * mount_data = &mount_point->mdata.mount_data;

	index = fs_read_4k(PAGE_BUFFER.page, clus2sec(clus, &mount_data->fat_info), &PAGE_BUFFER.clock_head, 10);
	PAGE_BUFFER.page[index].state = 3;
	for (i = 0; i < (mount_data->fat_info.BPB.attr.sectors_per_cluster << 9); ++i)
	{

		if (i < count)
			PAGE_BUFFER.page[index].buf[i] = buf[cc++];
		else
			PAGE_BUFFER.page[index].buf[i] = 0;
	}
	return 1;
}


/* Modified */
unsigned long fat_fs_read(u32 fd, vfs_node* file, u32 start, u32 count, void * iobuf)
{
	u32 i;
	u32 pages;
	u32 rem;
	u32 next_clus;
	u8* page_buffer;
	u32 clus = get_entry_start_cluster(file->mdata.node_data.entry.data);
	if (count == 0)
		return 0; // the file is empty
	vfs_node * mount_point = file->tag;
	//iobuf = charkmalloc(count * sizeof(char));
	pages = count >> 12;
	if ((count & 4095) > 0) pages = pages + 1;     // ceil division
//	kernel_printf("pages = %d", pages);
	page_buffer = (u8 *)kmalloc(pages * 4096);	// Allocate pagesizes * page_numbers
	for (i = 0; i < pages; i++)
	{
		//kernel_printf("pages = %d", i);
		if (fat_fs_read_page(mount_point, file, clus, page_buffer + (i << 12)) == 0)
		{
			log(LOG_FAIL, "read_page error");
		}
		get_fat_entry_value(clus, &next_clus);
		clus = next_clus;
	}

	kernel_memcpy(iobuf, page_buffer + start, count);
	kfree(page_buffer);
	return count;
}




/*read 4k from cluster 'clus_start' save in 'iobuf' 'iobuf' may not be like the BUF512, BUF4k, it may be i/o buffer, only characters */
u8 fat_fs_read_page(vfs_node * mount_point, vfs_node * file, u32 clus, void * buf)
{
	u32 index;
	u32 i, cc = 0;
	u8 * byte_buf = (unsigned char *)buf;
	struct fat_mount_data * mount_data = &mount_point->mdata.mount_data;
	index = fs_read_4k(PAGE_BUFFER.page, clus2sec(clus, &mount_data->fat_info), \
		&PAGE_BUFFER.clock_head, 10);
//	kernel_printf("index = %d", index);
	for (i = 0; i < (mount_data->fat_info.BPB.attr.sectors_per_cluster << 9); i++)
	{
		byte_buf[cc++] = PAGE_BUFFER.page[index].buf[i];
	}
	return 1;
}


vfs_node * fat_fs_mount(char * mount_name, vfs_node* dev_node, vfs_node* root)
{
	/* 创建一个挂载点的vfs_node */
	vfs_node* mount_point = vfs_create_node(mount_name, 0, NODE_MOUNT | NODE_DIRECTORY, VFS_READ | VFS_WRITE, \
		0, NULL, dev_node, root, &fat_ops);

	// 获取了挂载点的mount data
	// 
	fat_mount_data* mount_data = &(mount_point->mdata.mount_data);

	// 将mount data 中的 fat info 读取出来 将sd卡上的信息->fat_info这个struct 结构中
	//  
	u32 succ = init_fat_info(&mount_data->fat_info);

	if (0 != succ)
		goto fs_init_err;

	/* Base addr DBR表所在位置 */
	mount_data->partition_offset = mount_data->fat_info.base_addr;

	/* fat lba所在目录 */
	mount_data->fat_lba = mount_data->fat_info.BPB.attr.reserved_sectors \
		+ mount_data->partition_offset;

	mount_data->cluster_lba = mount_data->fat_lba + \
		mount_data->fat_info.BPB.attr.number_of_copies_of_fat * \
		mount_data->fat_info.BPB.attr.num_of_sectors_per_fat;

	/* 根目录的第一个蔟 */
	mount_data->root_dir_first_cluster = mount_data->fat_info.BPB.attr.cluster_number_of_root_dir;

	u32 root_dir_first_cluster = mount_data->root_dir_first_cluster;


	init_fat_buf();
	init_dir_buf();
	//exit(1);
	//mount_point->children = 
	fat_fs_read_directory(mount_point, root_dir_first_cluster, mount_point);

	return mount_point;

fs_init_err:
	log(LOG_FAIL, "File system init fail.");
	return 0;

}
u8 vfs_to_fat_attributes(u32 vfs_attrs)
{
	u32 attrs = 0;

	switch (vfs_attrs & 7)
	{
	case NODE_FILE:		attrs |= FAT_ARCHIVE;	break; //FAT_ARCHIVE is used for backup utilities to track changes in files. 
	case NODE_DIRECTORY: attrs |= FAT_DIRECTORY; break;
	case NODE_LINK:		attrs |= FAT_DIRECTORY; break;
	default:			attrs = 0;				break;
	}

	//if ((vfs_attrs & NODE_WRITE) != NODE_WRITE)
	//	attrs |= FAT_READONLY;

	if ((vfs_attrs & NODE_HIDDEN) == NODE_HIDDEN)
		attrs |= FAT_HIDDEN;

	return (u8)attrs;
}



/* Find a free data cluster */
u32 fat_fs_next_free(u32 start, u32 *next_free, vfs_node * mount_point) {
	u32 clus;
	u32 ClusEntryVal;

	*next_free = 0xFFFFFFFF;

	for (clus = start; clus <= mount_point->mdata.mount_data.fat_info.total_data_clusters + 1; clus++) {
		if (get_fat_entry_value(clus, &ClusEntryVal) == 1)
			goto fs_next_free_err;
		// == 0 is currently available
		if (ClusEntryVal == 0) {
			*next_free = clus;
			break;
		}
	}

	return 0;
fs_next_free_err:
	return 1;
}


/* Alloc a new free data cluster */
// Modified 2017 12.5
u32 fat_fs_alloc(u32 *new_alloc, vfs_node * mount_point) {
	u32 clus;
	u32 next_free;
	// No need to +1 fixed
	clus = get_u32(mount_point->mdata.mount_data.fat_info.fat_fs_info + 492);

	/* If FSI_Nxt_Free is illegal (> FSI_Free_Count), find a free data cluster
	* from beginning  fixed +1 */
	if (clus > get_u32(mount_point->mdata.mount_data.fat_info.fat_fs_info + 488)) {
		// search for next free cluster from the beginning ( cluster 2 )
		if (fat_fs_next_free(2, &clus, mount_point) == 1)
			goto fs_alloc_err;

		//        if (fs_modify_fat(clus, 0xFFFFFFFF) == 1)
		//            goto fs_alloc_err;
		// Redundant
	}

	/* FAT allocated and update FSI_Nxt_Free */
	if (fs_modify_fat(clus, 0xFFFFFFFF) == 1)
		goto fs_alloc_err;

	// find next free, update it
	if (fat_fs_next_free(clus + 1, &next_free, mount_point) == 1)
		goto fs_alloc_err;

	/* no available free cluster */
	if (next_free > mount_point->mdata.mount_data.fat_info.total_data_clusters)
		goto fs_alloc_err;
	// Modified no need to -1
	set_u32(mount_point->mdata.mount_data.fat_info.fat_fs_info + 492, next_free);

	*new_alloc = clus;
	// new_alloc is the new allocate

	/* Erase new allocated cluster */
	// Slightly modified
	if (write_block(new_alloc_empty, clus2sec(clus, &mount_point->mdata.mount_data.fat_info), mount_point->mdata.mount_data.fat_info.BPB.attr.sectors_per_cluster) == 1)
		goto fs_alloc_err;

	return 0;
fs_alloc_err:
	return 1;
}

void get_filename(u8 *entry, u8 *buf) {
	u32 i;
	u32 l1 = 0, l2 = 8;

	for (i = 0; i < 11; i++)
		buf[i] = entry[i];

	if (buf[0] == '.') {
		if (buf[1] == '.')
			buf[2] = 0;
		else
			buf[1] = 0;
	}
	else {
		for (i = 0; i < 8; i++)
			if (buf[i] == 0x20) {
				buf[i] = '.';
				l1 = i;
				break;
			}

		if (i == 8) {
			for (i = 11; i > 8; i--)
				buf[i] = buf[i - 1];

			buf[8] = '.';
			l1 = 8;
			l2 = 9;
		}

		for (i = l1 + 1; i < l1 + 4; i++) {
			if (buf[l2 + i - l1 - 1] != 0x20)
				buf[i] = buf[l2 + i - l1 - 1];
			else
				break;
		}

		buf[i] = 0;

		if (buf[i - 1] == '.')
			buf[i - 1] = 0;
	}
}

// recursively read directories and files. 
// if directory, then recursively read these things
struct list_head fat_fs_read_directory(vfs_node* mount_point, u32 current_cluster, vfs_node* parent)
{
	struct list_head l;
	INIT_LIST_HEAD(&l);
	BUF_4K m_buffer;
	u8 i;
	u8 j;
	u32 thisoffset;
	// 从根目录所开始的第一个蔟开始（若是根目录），否则是从当前目录的簇开始。
	u32 offset = current_cluster;
	/* 若多于1个簇 */
	while (offset < FAT_EOF)
	{
		fs_clear_clus(&m_buffer, &(mount_point->mdata.mount_data.fat_info), 1);
		if (fat_fs_read_by_data_cluster(mount_point, offset, &m_buffer) == 0)
		{
			log(LOG_FAIL, "Read data cluster error");
			while (1);
		}
// Directory
		struct dir_entry_attr * entry = (struct dir_entry_attr *)m_buffer.buf;
		char * longfile = 0;
		char lfn[130] = { 0 };
		for (i = 0; i < 128; i++)
		{
			if (entry[i].name[0] == 0)  // empty and no entry afterward
				break;
			if (entry[i].name[0] == 0xe5)	// empty but maybe entries afterward
				continue;
			if ((entry[i].attr & FAT_VOLUME_ID) == FAT_VOLUME_ID)	// long filename 
			{
				longfile = (char*)kmalloc(13);
				longfile[0] = *((char*)&entry[i] + 1);
				longfile[1] = *((char*)&entry[i] + 3);
				longfile[2] = *((char*)&entry[i] + 5);
				longfile[3] = *((char*)&entry[i] + 7);
				longfile[4] = *((char*)&entry[i] + 9);
				longfile[5] = *((char*)&entry[i] + 14);
				longfile[6] = *((char*)&entry[i] + 16);
				longfile[7] = *((char*)&entry[i] + 18);
				longfile[8] = *((char*)&entry[i] + 20);
				longfile[9] = *((char*)&entry[i] + 22);
				longfile[10] = *((char*)&entry[i] + 24);
				longfile[11] = *((char*)&entry[i] + 28);
				longfile[12] = *((char*)&entry[i] + 30);
				kernel_memcpy(lfn + 13, lfn, 13);
				kernel_memcpy(lfn, longfile, 13);

				continue;
			}

			// get its short name.
			char filename[13] = { 0 };
			fat_fs_get_short_name(entry + i, filename);

			struct list_head children;
			INIT_LIST_HEAD(&children);

			u32 attribute = fat_to_vfs_attr(entry[i].attr);
			vfs_node *node;
			if (lfn[0] == 0)
			{
				node = vfs_create_node(filename, 1, attribute, 0, entry[i].size, \
					NULL, mount_point, parent, &fat_ops);
			}
			else
			{
				//				exit(0);
				node = vfs_create_node(lfn, 1, attribute, 0, entry[i].size, \
					NULL, mount_point, parent, &fat_ops);
				lfn[0] = 0;
			}

			node->mdata.node_data.metadata_cluster = offset;
			node->mdata.node_data.metadata_index = i;
			//			node->mdata.node_data.layout_loaded = 0;
			for (j = 0; j < 32; j++)
			{
				node->mdata.node_data.entry.data[j] = *((u8 *)(entry + i) + j);
			}


			if (filename[0] != '.' && ((entry[i].attr & FAT_DIRECTORY) == FAT_DIRECTORY))
			{
				u32 clus = fat_fs_get_entry_data_cluster(entry + i);
				children = fat_fs_read_directory(mount_point, clus, node);
			}

			//			node->children = children;
			vfs_add_child(parent, node);
			//			list_add(&node->thisnode, &node->children);
		}
		thisoffset = offset;
		get_fat_entry_value(thisoffset, &offset);
		/* offset -> next offset */
	}
	return l;
}

u8 fat_fs_read_by_data_cluster(vfs_node * mount_point, u32 offset, BUF_4K * ptr_buffer)
{
	u32 index;
	// offset 第几个蔟 （根目录开始）
	u32 dummy = 0;
	index = fs_read_4k(ptr_buffer, clus2sec(offset, &mount_point->mdata.mount_data.fat_info), &(dummy), 1);
	return 1;
}


/* since the fat32 supports 8.3 */
void fat_fs_get_short_name(struct dir_entry_attr* entry, char buffer[13])
{
	buffer[12] = 0;
	u8 name_index = 0;
	u8 i;
	for (i = 0; i < 8; ++i)
		if (entry->name[i] != ' ')
			buffer[name_index++] = entry->name[i];
	buffer[name_index++] = '.';
	u8 prev_index = name_index;
	for (i = 0; i < 3; ++i)
		if (entry->ext[i] != ' ')
			buffer[name_index++] = entry->ext[i];
	if (name_index == prev_index) buffer[--name_index] = 0; // this may be a directory
}

// Convert fat attributes to VFS node's attribute
u32 fat_to_vfs_attr(u32 fat_attr)
{
	u32 attr = NODE_READ;

	if ((fat_attr & FAT_READONLY) != FAT_READONLY)
		attr |= NODE_WRITE;
	if ((fat_attr & FAT_HIDDEN) == FAT_HIDDEN)
		attr |= NODE_HIDDEN;
	if ((fat_attr & FAT_DIRECTORY) == FAT_DIRECTORY)
		attr |= NODE_DIRECTORY;
	else
		attr |= NODE_FILE;
	return attr;
}


u32 fat_fs_get_entry_data_cluster(struct dir_entry_attr* e)
{
	return e->startlow + ((u32)e->starthi & 0x0FFFFFFF);
}


//unsigned long  fat_fs_open(struct vfs_node *node, u32 open_flags){}
unsigned long  fat_fs_close(struct vfs_node *node) {}

u8 fat_fs_validate_83_name(char* name, u32 length)
{
	log(LOG_OK, "length = %d", length);
	if (length > 11)
		return 0;

	/*0x2E, (this is the '.')*/
	u8 bad_values[] = { 0x22, 0x2A, 0x2B, 0x2C,  0x2F, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, 0x7C };

	for (u32 i = 0; i < length; i++)
	{
		if (i != 0 && name[i] < 0x20)
			return 0;

		if (i == 0 && name[i] < 0x20 && name[i] != 0x05)
			return 0;

		for (u8 j = 0; j < 16; j++)
			if (name[i] == bad_values[j])
				return 0;
	}

	return 1;
}

u8 fat_fs_find_empty_entry(vfs_node* mount_point, vfs_node* directory, u32 * cluster, u32 * index)
{
	u32 offset, thisoffset;
	BUF_4K m_buffer;
	fs_clear_clus(&m_buffer, &(mount_point->mdata.mount_data.fat_info), 1);
	*cluster = 0;
	*index = 129;		// index ranges [0, 127], so set this error value

						//	auto layout = LAYOUT(directory);	// this must have been loaded...

						// foreach metadata cluster in the chain loaded
	if (directory == mount_point)
		offset = mount_point->mdata.mount_data.root_dir_first_cluster;
	else
		offset = directory->mdata.node_data.metadata_cluster;
	log(LOG_OK, "offset = %d", offset);

	while (offset < FAT_EOF)
	{
	check_offset:
		if (fat_fs_read_by_data_cluster(mount_point, offset, &m_buffer) == 0)
		{
			log(LOG_FAIL, "READ DATA FAILED");
			return 0;
		}

		struct dir_entry_attr* file_entry = (struct dir_entry_attr*)m_buffer.buf;

		// loop through the metadata cluster's 128 entries
		for (u32 j = 0; j < 127; j++)
		{
			if (file_entry[j].name[0] == 0xE5 || file_entry[j].name[0] == 0)		// this is a free entry
			{
				*cluster = offset;
				*index = j;
				return 1;
			}
		}

		thisoffset = offset;
		get_fat_entry_value(thisoffset, &offset);

	}
	// Till the end of the cluster need to alloc a new one.
	if (fat_fs_alloc(&offset, mount_point) == 1)
	{
		log(LOG_FAIL, "No more space");
		return 0xff;
	}
	if (fs_modify_fat(thisoffset + 1, offset) == 1)
		return 0xff;
	goto check_offset;

	return 0;
}


void fat_fs_generate_short_name(vfs_node* node, char name[12])
{
	if (node->name_length > 12)
		return;

	for (u8 j = 0; j < 11; j++)
		name[j] = ' ';

	u8 i = 0;
	u8 dot_index = 0;
	for (; i < 8 && i < node->name_length; i++)
	{
		if (node->name[i] == '.')
		{
			dot_index = i;
			while (i < 8)
				name[i++] = ' ';

			break;
		}

		name[i] = node->name[i];
	}

	if (dot_index == 0)
		return;

	for (u8 j = 0; j < 3 && dot_index + j + 1 < node->name_length; j++, i++)
		name[i] = node->name[dot_index + j + 1];
}

u8 fat_fs_create_short_entry_from_node(struct dir_entry_attr* entry, vfs_node* node)
{

	//uint32 first_cluster = vector_at(LAYOUT(node), 0);
	u32 first_cluster = node->mdata.node_data.metadata_cluster;
	u32 next_clus;
	log(LOG_FAIL, "attr = %x", vfs_to_fat_attributes(node->attributes));
	entry->attr = vfs_to_fat_attributes(node->attributes);
	fat_fs_generate_short_name(node, (char*)entry->name);
	entry->size = 0;

	// TODO: Fix dates
	entry->cdate = 0x2c0d;
	entry->ctime = 0x2082;
	entry->ctime_cs = 0;

	entry->adate = 0x2c0d;
	entry->date = 0x2c0d;
	entry->time = 0x2082;

	fat_fs_alloc(&next_clus, node->tag);
	entry->startlow = (u16)next_clus;
	entry->starthi = (u16)((next_clus >> 16) & 0xFF);

	entry->lcase = 0;
	return 1;
}


//unsigned long  fat_fs_read(u32 fd, struct vfs_node *node, u32 start, u32 count, void *iob){}

vfs_node* fat_fs_create_node(vfs_node* mount_point, vfs_node* directory, char* name, u32 vfs_attributes)
{
	struct fat_mount_data * mount_data = &mount_point->mdata.mount_data;
	if (mount_point == 0 || directory == 0 ||
		(mount_point->attributes & NODE_MOUNT) != NODE_MOUNT || (directory->attributes & NODE_DIRECTORY) != NODE_DIRECTORY)
	{
		log(LOG_FAIL, "attributes = %d", directory->attributes);
		log(LOG_FAIL, "Create file below %s failed", directory->name);
		return 0;
	}

	if (fat_fs_validate_83_name(name, kernel_strlen(name)) == 0)
	{
		log(LOG_FAIL, "Not a valid 8-3 name");
		return 0;
	}
	BUF_4K m_buffer;

	u32 metadata_cluster, metadata_index, index;

	// try to find an empty entry under the directory clusters.
	if (fat_fs_find_empty_entry(mount_point, directory, &metadata_cluster, &metadata_index))
	{
	file_creation:
		kernel_printf("found empty entry at: %u %u\n", metadata_cluster, metadata_index);
		// when found, the cluster is already loaded into the cache

		index = fs_read_4k(PAGE_BUFFER.page, clus2sec(metadata_cluster, &mount_data->fat_info), &PAGE_BUFFER.clock_head, 10);
		struct dir_entry_attr* file_entry = (struct dir_entry_attr*)PAGE_BUFFER.page[index].buf + metadata_index;

		PAGE_BUFFER.page[index].state = 3;

		// create the new node for the vfs tree

		vfs_node* new_node = vfs_create_node(name, 1, vfs_attributes, 0, 0, 0, mount_point, directory, &fat_ops);
		new_node->mdata.node_data.metadata_cluster = metadata_cluster;
		new_node->mdata.node_data.metadata_cluster = metadata_index;
		vfs_add_child(directory, new_node);

		if (fat_fs_create_short_entry_from_node(file_entry, new_node) != 1)
		{
			return 0;
		}
		////////////////////////////////////////////

		/* The file's metadata stuff have been created. Now we deal with the file's first cluster */

		// save the created entry for further use as the cache is re-used

		return new_node;
	}
	else
	{


		//		metadata_cluster = free_cluster;
		metadata_index = 0;

		goto file_creation;
	}

	log(LOG_FAIL, "No empty cluster found!");
	return 0;
}




unsigned long  fat_fs_sync(u32 fd, struct vfs_node *node, u32 start_page, u32 end_page) {}
unsigned long  fat_fs_lookup(struct vfs_node *node, char * path, struct vfs_node ** result) {}
unsigned long  fat_fs_flush(struct vfs_node * mount_point)
{
	int i = 0;
	int j;
	for (i = 0; i < 10; ++i)
	{
		if (fat_fs_write_4k(PAGE_BUFFER.page + i, &mount_point->mdata.mount_data.fat_info) == 1)
		{ 
			//log(LOG_FAIL, "%d", i);
			//log(LOG_FAIL, "FLUSH DISK FAILED");
			j = 0;
			}
	}
	return 1;
}



// initializes a directory with the '.' and '..' entries as the first entries in the buffer.
void fat_fs_initialize_directory(u32 parent_cluster, struct dir_entry_attr* new_dir, char* buffer)
{
	struct dir_entry_attr* dot = (struct dir_entry_attr*)buffer;

	kernel_memcpy(dot, new_dir, sizeof(struct dir_entry_attr));		// greedy copy of new_dir data over dot
	kernel_memcpy((char*)dot->name, ".          ", 11);			// fix dot name

	log(LOG_OK, "new_dir attributes: %h", new_dir->attr);
	log(LOG_OK, "dot attributes: %h", dot->attr);

	struct dir_entry_attr* dotdot = dot + 1;

	kernel_memcpy(dotdot, new_dir, sizeof(struct dir_entry_attr));
	kernel_memcpy((char*)dotdot->name, "..         ", 11);

	dotdot->startlow = (u16)parent_cluster;
	dotdot->starthi = (u16)((parent_cluster >> 16) & 0x00FF);
}





