#include <fat.h>

/* VFS NODE ATTRIBUTES */
#define NODE_UNDIFINED                          0x00000000
#define NODE_FILE                               0x00000001
#define NODE_DIRECTORY                          0x00000002
#define NODE_LINK                               0x00000003
#define NODE_DEVICE                             0x00000004
#define NODE_MOUNT                              0x00000005
#define NODE_PIPE                               0x00000006
#define NODE_READ                               0x00000008
#define NODE_WRITE                              0x00000016

/* VFS CAPABILITIES */
#define VFS_NONE                                0x00000001
#define VFS_READ                                0x00000002
#define VFS_WRITE                               0x00000004



/* VFS flags */
#define O_RDONLY                                0            // open for reading only
#define O_WRONLY                                0x00000001   // open for writing only
#define O_RDWR                                  0x00000002   // open for reading and writing 
#define O_CREAT                                 0x00000004   // create file if it doesn't exist
#define O_EXCL                                  0x00000008   // error if O_CREAT and the file exist
#define O_TRUNC                                 0x00000010   // truncate file upon open
#define O_APPEND                                0x00000020   // append on each write
#define FS_MAX_DNAME_LEN                        31  


struct fs {
    union {
        struct fat32_fs __fat32_info;                   
    } fs_info;                                     // filesystem-specific data (only fat32 now) 
    enum {
        fs_type_fat32_info,
    } fs_type;                                     // filesystem type (currently only fat32)

    struct inode *(*fs_get_root)(struct fs *fs);   // Return root inode of filesystem.
    int (*fs_unmount)(struct fs *fs);              // Attempt unmount of filesystem.
    void (*fs_cleanup)(struct fs *fs);             // Cleanup of filesystem.???
};

// no super block, they are all in the fs structure.


#define alloc_fs(type)                                              __alloc_fs(__fs_type(type))

#define __fs_type(type)                                             fs_type_##type##_info

#define check_fs_type(fs, type)                                     ((fs)->fs_type == __fs_type(type))

// calling the filesystem operations
#define fsop_sync(fs)                               ((fs)->fs_sync(fs))
#define fsop_get_root(fs)                           ((fs)->fs_get_root(fs))
#define fsop_unmount(fs)                            ((fs)->fs_unmount(fs))
#define fsop_cleanup(fs)                            ((fs)->fs_cleanup(fs))
void vfs_init(void);
void vfs_cleanup(void);
void vfs_devlist_init(void);

int vfs_mount(const char *devname, int (*mountfunc)(struct device *dev, struct fs **fs_store));

#define __fsop_info(_fs, type) ({                                   \
            struct fs *__fs = (_fs);                                \
            assert(__fs != NULL && check_fs_type(__fs, type));      \
            &(__fs->fs_info.__##type##_info);                       \
        })

#define fsop_info(fs, type)                 __fsop_info(fs, type)

int vfs_add_dev(const char *devname, struct inode *devnode, bool mountable);



/*
    vfs node (inode for the VFS system)
 */

struct vfs_node {

    char* name;
    uint32_t name_length;
    uint32_t attributes;
    uint32_t capabilities;
    uint32_t file_length;
    struct vfs_node* tag;  // tag node
    struct vfs_node* parent; // parent node

    uint32_t flags;     // file flags
    
    //    struct fs *in_fs;
    const struct inode_ops *in_ops;
    list_head * children;
    union{
        struct fat_mount_data mount_data; // mount data infomation
        struct fat_node_data node_data;   // node data for fat32   
    }   metadata;
};




struct inode_ops {
    //unsigned long vop_magic;
    unsigned long (*vop_open)(struct vfs_node *node, uint32_t open_flags);
    unsigned long (*vop_close)(struct vfs_node *node);
    unsigned long (*vop_read)(struct vfs_node *node, void *iob);
    unsigned long (*vop_write)(struct vfs_node *node, void *iob);
    unsigned long (*vop_sync)(struct vfs_node *node);                 
    unsigned long (*vop_flush)();
};

