#include <vfs.h>
#include <inode.h>


extern void vfs_devlist_init(void);


vfs_node* root;	// the root of the whole file system.

// vfs_init -  vfs initialize
void vfs_init(void) {
    vfs_devlist_init();
    root = vfs_create_node("root", false, NODE_DIRECTORY, VFS_READ, 0, NULL, NULL, NULL, NULL);

    /* Add the device on it */
    vfs_add_child(root, vfs_create_node("dev", false, 0, VFS_READ, 0, NULL, root, NULL, NULL));
}


vfs_node* vfs_create_node(char *filename, bool copy_name, uint32_t attributes, uint32_t capabilities, \
							uint32_t file_length, void * md, vfs_node * tag, vfs_node * parent,  \
							struct inode_ops* fileops)
{
	vfs_node * new_node = kmalloc(sizeof(vfs_node));

	if (!new_node)
	{
		log(LOG_FAIL, "ALLOC NODE FAILED");
		while (1);
	}
	INIT_LIST_HEAD(new_node->children);
	new_node->name_length = sizeof(filename); // modified later
	new_node->tag = tag;
	new_node->parent = parent;

	if (fileops != NULL)
		new_node->in_ops = fileops;
	else
	{
		log(LOG_FAIL, "FILE_OP NOT EXIST");
		while (1);
	}
	if (m_data != NULL)
		new_node->mount_data = m_data;
	new_node->name = filename;
	new_node->attributes = attributes;
	new_node->capabilities = capabilities;
	new_node->file_length = file_length;

	return new_node;
}

uint32_t vfs_root_lookup(char * path, vfs_node ** result)
{
	return vfs_lookup(vfs_get_root(), path, result);
}

uint32_t vfs_lookup(vfs_node* parent, char* path, vfs_node ** result) 
{
	return parent->in_ops->fs_lookup(parent, path, result);
}


vfs_node* vfs_get_root()
{
	return root;
}

vfs_node* vfs_get_dev()
{
	return vfs_find_child(root, "dev");
}

vfs_node* vfs_find_child(vfs_node * node, char * name)
{
	list_head * head = node->children;
	list_head * find = head;
	do 
	{
		if (kernel_strcmp((to_struct(find, vfs_node, children))->filename, name) == 0)
			return (to_struct(find, vfs_node, children));
		find = list_next(find);
	}
		while (find != head);
	return NULL;
}

uint32_t vfs_read_file(uint32_t fd, vfs_node* node, size_t count)
{
	return node->in_ops->vop_read(fd, node, count);
}

uint32_t vfs_write_file(uint32_t fd, vfs_node* node, size_t count)
{
	return node->in_ops->vop_write(fd, node, count);
}



struct fs * __alloc_fs(int type) {
    struct fs *fs;
    if ((fs = kmalloc(sizeof(struct fs))) != NULL) {
        fs->fs_type = type;
    }
    return fs;
}
