#ifndef _VFS_H
#define _VFS_H
#include "fat.h"
#include<driver/vga.h>
//#include <stdlib.h> // debug for windows

/* VFS NODE ATTRIBUTES */
#define NODE_UNDIFINED                          0x00000000	
#define NODE_FILE                               0x00000001	// this node is a normal file
#define NODE_DIRECTORY                          0x00000002  // this node is a directory
#define NODE_LINK                               0x00000003	// this node is a link
#define NODE_DEVICE                             0x00000004	// this node is a device
#define NODE_MOUNT                              0x00000005	// mount point in the VFS
#define NODE_PIPE                               0x00000006	// Pipe in the VFS
#define NODE_READ                               0x00000008	// this node can be read
#define NODE_WRITE                              0x00000010	// this node can be written
#define NODE_HIDDEN                             0x00000020	// this node is hidden

/* VFS CAPABILITIES */
#define VFS_NONE                                0x00000001
#define VFS_READ                                0x00000002
#define VFS_WRITE                               0x00000004


/*
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
*/
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
//void vfs_devlist_init(void);

//int vfs_mount(const char *devname, int (*mountfunc)(struct device *dev, struct fs **fs_store));
/*
#define __fsop_info(_fs, type) ({                                   \
            struct fs *__fs = (_fs);                                \
            assert(__fs != NULL && check_fs_type(__fs, type));      \
            &(__fs->fs_info.__##type##_info);                       \
        })

#define fsop_info(fs, type)                 __fsop_info(fs, type)

int vfs_add_dev(const char *devname, struct inode *devnode, bool mountable);
*/


/*
    vfs node (inode for the VFS system)
 */

struct vfs_node {
    char* name;
    u32 name_length;
    u32 attributes;
    u32 capabilities;
    u32 file_length;
	struct list_head thisnode;
    struct vfs_node* tag;  // tag node
    struct vfs_node* parent; // parent node
    u32 flags;     // file flags
    const struct inode_ops *in_ops;
    struct list_head children;
    union mdata {
        struct fat_mount_data mount_data; // mount data infomation
        struct fat_node_data node_data;   // node data for fat32   
    }   mdata;
};

typedef struct vfs_node vfs_node;


struct inode_ops {
    unsigned long (*vop_open)(struct vfs_node *node, u32 open_flags);
    unsigned long (*vop_close)(struct vfs_node *node);
    unsigned long (*vop_read)(u32 fd, struct vfs_node *node, u32 start, u32 count, void *iob);
    unsigned long (*vop_write)(u32 fd, struct vfs_node *node, u32 start, u32 count, const u8 *buf);
    unsigned long (*vop_sync)(u32 fd, struct vfs_node *node, u32 start_page, u32 end_page);
    unsigned long (*vop_lookup)(struct vfs_node *node, char * path, struct vfs_node ** result) ;                  
    unsigned long (*vop_flush)(struct vfs_node *mount_pount);
};
typedef struct inode_ops inode_ops;

inline u32 vfs_read_file(u32 fd, vfs_node* node, u32 start, size_t count, void *iob) 
    { 
            return node->in_ops->vop_read(fd, node, start, count, iob); 
    }

inline u32 vfs_write_file(u32 fd, vfs_node* node, u32 start, size_t count, void *iob) 
    { 
            return node->in_ops->vop_write( fd, node, start, count, iob); 
    }

vfs_node* vfs_create_node(char *filename, bool copy_name, u32 attributes, u32 capabilities, \
	u32 file_length, void * md, vfs_node * tag, vfs_node * parent, \
	struct inode_ops* fileops);


void vfs_init();

u32 vfs_lookup(vfs_node* parent, char* path, vfs_node ** result);

vfs_node* vfs_get_root();

vfs_node* vfs_get_dev();


vfs_node* vfs_find_child(vfs_node * node, char * name);

void vfs_add_child(vfs_node* parent, vfs_node* child);

vfs_node * fat_fs_mount(char * mount_name, vfs_node* dev_node, vfs_node* root);

vfs_node* vfs_find_relative_node(vfs_node* start, char* path);

unsigned long vfs_default_read(u32 fd, vfs_node* node, u32 start, u32 count, void *iob);
unsigned long vfs_default_write(u32 fd, vfs_node* node, u32 start, u32 count, void *iob);
unsigned long vfs_default_sync(u32 fd, vfs_node* node, u32 start_page, u32 end_page);
unsigned long vfs_default_open(vfs_node* node, u32 open_flags);
unsigned long vfs_default_lookup(vfs_node* parent, char* path, vfs_node** result);

unsigned long vfs_stdin_read(u32 fd, vfs_node* node, u32 start, u32 count, void *iob);
unsigned long vfs_stdout_write(u32 fd, vfs_node* node, u32 start, u32 count, void *iob);
unsigned long vfs_stderr_write(u32 fd, vfs_node* node, u32 start, u32 count, void *iob);

#endif
