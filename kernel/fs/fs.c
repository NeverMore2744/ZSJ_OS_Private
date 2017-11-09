#include <fat.h>
#include <vfs.h>
#include <inode.h>



void fs_init(void) {
    vfs_init();	// initial the device list 
    dev_init();
    fat_init();
}