#include <dev.h>
#include <inode.h>
#include <auxillary/defs.h>


#define DISK0_BLKSIZE       4096
#define DISK0_BUFSIZE       (4096 * 4)
#define DISK0_BLK_NSECT     (4096 / 512)

#define init_device(x)                                  \
    do {                                                \
        dev_init_##x();                                 \
    } while (0)


BUF_512 fat_buf[FAT_BUF_NUM];   // 512 byte 的缓冲区

/* dev_init - Initialization functions for builtin vfs-level devices. */
void
dev_init(void) {
   // init_device(null);
   // init_device(stdin);
   // init_device(stdout);
    init_device(disk0);
}

/* function pointers */
static int
disk0_open(struct device *dev, uint32_t open_flags) {
    return 0;
}

static int
disk0_close(struct device *dev) {
    return 0;

}


// static int
// disk0_io(struct device *dev, BUF_512 iob, bool write) {
//     //off_t offset = iob->io_offset;
//     //size_t resid = iob->io_resid;
//     //uint32_t blkno = offset / DISK0_BLKSIZE;
//     uint32_t nblks = resid / DISK0_BLKSIZE;

//     /* don't allow I/O that isn't block-aligned */
//     if ((offset % DISK0_BLKSIZE) != 0 || (resid % DISK0_BLKSIZE) != 0) {
//         return -E_INVAL;
//     }

//     /* don't allow I/O past the end of disk0 */
//     if (blkno + nblks > dev->d_blocks) {
//         return -E_INVAL;
//     }

//     /* read/write nothing ? */
//     if (nblks == 0) {
//         return 0;
//     }

// //    lock_disk0();
//     while (resid != 0) {
//         size_t copied, alen = DISK0_BUFSIZE;
//         if (write) {
//             iobuf_move(iob, disk0_buffer, alen, 0, &copied);
//             assert(copied != 0 && copied <= resid && copied % DISK0_BLKSIZE == 0);
//             nblks = copied / DISK0_BLKSIZE;
//             disk0_write_blks_nolock(blkno, nblks);
//         }
//         else {
//             if (alen > resid) {
//                 alen = resid;
//             }
//             nblks = alen / DISK0_BLKSIZE;
//             disk0_read_blks_nolock(blkno, nblks);
//             iobuf_move(iob, disk0_buffer, alen, 1, &copied);
//             assert(copied == alen && copied % DISK0_BLKSIZE == 0);
//         }
//         resid -= copied, blkno += nblks;
//     }
// //    unlock_disk0();
//     return 0;
// }




void dev_init_disk0(void) {
    struct inode *node;
    if ((node = dev_create_inode()) == NULL) {
        panic("disk0: dev_create_node.\n");
    }
    disk0_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("disk0", node, 1)) != 0) {
        panic("disk0: vfs_add_dev: %e.\n", ret);
    }
}


struct inode * dev_create_inode(void) {
    struct inode *node;

    if ((node = __alloc_inode(inode_type_device_info)) != NULL) {
        vop_init(node, &dev_node_ops, NULL);
    }
    return node;
}

static void disk0_device_init(struct device *dev) {
//    static_assert(DISK0_BLKSIZE % SECTSIZE == 0);
/*    if (!ide_device_valid(DISK0_DEV_NO)) {
        panic("disk0 device isn't available.\n");
    }   */
//    dev->d_blocks = ide_device_size(DISK0_DEV_NO) / DISK0_BLK_NSECT;
//    dev->d_blocksize = DISK0_BLKSIZE;
    dev->d_open = disk0_open;
    dev->d_close = disk0_close;
//    dev->d_io = disk0_io;
    //dev->d_ioctl = disk0_ioctl;
//   sem_init(&(disk0_sem), 1);

/* Buffer 以 数组形式存在 */
//    static_assert(DISK0_BUFSIZE % DISK0_BLKSIZE == 0);
//    if ((disk0_buffer = kmalloc(DISK0_BUFSIZE)) == NULL) {
//        panic("disk0 alloc buffer failed.\n");
//    }
}
