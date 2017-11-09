#include <fat.h>


/*
 * A struct inode is an abstract representation of a file.
 *
 * It is an interface that allows the kernel's filesystem-independent 
 * code to interact usefully with multiple sets of filesystem code.
 * vfs层的inode可能有不同的文件系统的底层
 * 
 * 
 */


#define __in_type(type)                                             inode_type_##type##_info

#define check_inode_type(node, type)                                ((node)->in_type == __in_type(type))

#define __vop_info(node, type)                                      \
    ({                                                              \
        struct inode *__node = (node);                              \
        assert(__node != NULL && check_inode_type(__node, type));   \
        &(__node->in_info.__##type##_info);                         \
     })

#define vop_info(node, type)                                        __vop_info(node, type)

struct inode *__alloc_inode(int type);

struct inode {

	char* name;
	uint32_t name_length;
	uint32_t attributes;
	uint32_t capabilities;
	uint32_t file_length;
	struct inode* tag;	// tag node
	struct inode* parent; // parent node

	uint32_t flags; 	// file flags

    struct fs *in_fs;
    const struct inode_ops *in_ops;
    list_head * children;
};

struct inode_ops {
    //unsigned long vop_magic;
    unsigned long (*vop_open)(struct inode *node, uint32_t open_flags);
    unsigned long (*vop_close)(struct inode *node);
    unsigned long (*vop_read)(struct inode *node, void *iob);
    unsigned long (*vop_write)(struct inode *node, void *iob);
    unsigned long (*vop_flush)();
};


