#include "vfs.h"
#include <zjunix/log.h>
#include <zjunix/utils.h>


//extern void vfs_devlist_init(void);


static inode_ops default_fs_operations =
{
	vfs_default_open,
	NULL,
	vfs_default_read,
	vfs_default_write,
	vfs_default_sync,
	vfs_default_lookup,
	NULL
};

static inode_ops stdin_fs_operations =
{
	NULL,
	NULL,
	vfs_stdin_read,
	NULL,
	NULL,
	NULL,
	NULL
};

static inode_ops stdout_fs_operations =
{
	NULL,
	NULL,
	NULL,
	vfs_stdout_write,
	NULL,
	NULL,
	NULL
};

static inode_ops stderr_fs_operations =
{
	NULL,
	NULL,
	NULL,
	vfs_stderr_write,
	NULL,
	NULL,
	NULL
};


vfs_node* root;	// the root of the whole file system.

// vfs_init -  vfs initialize
void vfs_init() {
//    vfs_devlist_init();
	
    root = vfs_create_node("root", 0, NODE_DIRECTORY, VFS_READ, 0, NULL, NULL, NULL, &default_fs_operations);

    /* Add the device on it */
	vfs_node* devnode = vfs_create_node("dev", 0, NODE_DIRECTORY, VFS_READ, 0, NULL, root, root, &default_fs_operations);
    vfs_add_child(root, devnode);

	/* Add device on it */
	vfs_node* node_sda = vfs_create_node("sda", 0, NODE_DEVICE, VFS_READ, 0, NULL, devnode, devnode, &default_fs_operations);
	vfs_add_child(devnode, node_sda);
	vfs_node* node_stdin = vfs_create_node("stdin", 0, NODE_DEVICE, VFS_READ, 0, NULL, devnode, devnode, &stdin_fs_operations);
	vfs_add_child(devnode, node_stdin);
	vfs_node* node_stdout = vfs_create_node("stdout", 0, NODE_DEVICE, VFS_WRITE, 0, NULL, devnode, devnode, &stdout_fs_operations);
	vfs_add_child(devnode, node_stdout);
	vfs_node* node_stderr = vfs_create_node("stderr", 0, NODE_DEVICE, VFS_WRITE, 0, NULL, devnode, devnode, &stderr_fs_operations);
	vfs_add_child(devnode, node_stderr);


	log(LOG_OK, "devnode pointer %p", (char *)(devnode));
	log(LOG_OK, "devnode thisnode %p", (char *)(&devnode->thisnode));
	/*
	vfs_node* devnode2 = vfs_create_node("dev2", 0, 0, VFS_READ, 0, NULL, root, NULL, &default_fs_operations);
	vfs_add_child(root, devnode2);
	log(LOG_OK, "devnode2 pointer %p", (char *)(devnode2));
	log(LOG_OK, "devnode2 thisnode %p", (char *)(&devnode2->thisnode));
	vfs_node* devnode3 = vfs_create_node("dev3", 0, 0, VFS_READ, 0, NULL, root, NULL, &default_fs_operations);
	vfs_add_child(root, devnode3);
	log(LOG_OK, "devnode3 pointer %p", (char *)(devnode2));
	log(LOG_OK, "devnode3 thisnode %p", (char *)(&devnode2->thisnode));
	*/


	// ÈÕºóÅ²×ß
	vfs_node * mount = fat_fs_mount("user", devnode, root);
	vfs_add_child(root, mount);
	/* Add Mount the devnode */
}

/* create a new node at the VFS layer*/
vfs_node* vfs_create_node(char *filename, bool copy_name, u32 attributes, u32 capabilities, \
							u32 file_length, void * m_data, vfs_node * tag, vfs_node * parent,  \
							inode_ops* fileops)
{

	vfs_node * new_node = (vfs_node * ) kmalloc(sizeof(vfs_node));	// to be modified
	if (!new_node)
	{
		log(LOG_OK, "ALLOC NODE FAILED");
		while (1);
	}

	INIT_LIST_HEAD(&new_node->children);

	new_node->name_length = kernel_strlen(filename); // modified later
	new_node->tag = tag;
	new_node->parent = parent;

	INIT_LIST_HEAD(&new_node->thisnode);

	if (fileops != NULL)
		new_node->in_ops = fileops;
	else
	{
		log(LOG_OK, "FILE_OP NOT EXIST");
		while (1);
	}

/*	if (m_data != NULL)
		new_node->metadata.mount_data = m_data; */
	new_node->name = (char*)kmalloc(kernel_strlen(filename) + 1);


	kernel_memcpy(new_node->name, filename, kernel_strlen(filename) + 1 );
	new_node->attributes = attributes;
	new_node->capabilities = capabilities;
	new_node->file_length = file_length;
	log(LOG_OK, "Node created: %s", new_node->name);
	return new_node;
}

/* from the root look up a vfs node */
u32 vfs_root_lookup(char * path, vfs_node ** result)
{
	return vfs_lookup(vfs_get_root(), path, result);
}

/*use specific lookup method to find vfs node*/
u32 vfs_lookup(vfs_node* parent, char* path, vfs_node ** result) 
{
	return parent->in_ops->vop_lookup(parent, path, result);
}

/* return the root */
vfs_node* vfs_get_root()
{
	return root;
}




//
// @ usage vfs_node vfs_node use to add child to parents' child list
//
void vfs_add_child(vfs_node* parent, vfs_node* child)
{

	list_add(&child->thisnode, &parent->children);

	child->parent = parent;
}




unsigned long vfs_default_read(u32 fd, vfs_node* node, u32 start, u32 count, void *iob)
{

}
unsigned long vfs_default_write(u32 fd, vfs_node* node, u32 start, u32 count, void *iob)
{

}
unsigned long vfs_default_sync(u32 fd, vfs_node* node, u32 start_page, u32 end_page)
{

}
unsigned long vfs_default_open(vfs_node* node, u32 open_flags)
{

}
unsigned long vfs_default_lookup(vfs_node* parent, char* path, vfs_node** result)
{

}

void test_vfs()
{
	vfs_node * test = vfs_get_root();

	log(LOG_OK, "pointer root %p", (char *)(root));
	log(LOG_OK, "pointer root this node %p", (char *)(&test->thisnode));
	vfs_node * ele = to_struct(&test->thisnode, vfs_node, thisnode);
	log(LOG_OK, "pointer calculated %p", (char *)(ele));
	log(LOG_OK, "this pointer %p", (char *)(&((vfs_node *)0)->thisnode));
	log(LOG_OK, "this member offset %d", offsetof(vfs_node, thisnode));
	vfs_node * elements;
	vfs_node * b;
	vfs_node * c;
	u8 *buf;
	u32 file_size;
	u32 start;
	struct list_head * p = &test->children;
	char * mypath;
	vfs_node * tet = to_struct(p, vfs_node, children);
	do {
		p = list_next(p);
		if (p == &test->children) goto VFS_DEVEND;
		elements = to_struct(p, vfs_node, thisnode);
		log(LOG_OK, "pointer children %p", (char *)(p));
		log(LOG_OK, "pointer calculated %p", (char *)(elements));
		log(LOG_OK, "Element within root: %s", elements->name);
	} while (1);
VFS_DEVEND:
	log(LOG_OK, "DEV node check successful");

	vfs_node *pp = vfs_find_child(root, "user");
	vfs_node *ps = vfs_find_child(pp, "DIR");
	vfs_node *pk = vfs_find_child(ps, "B.TXT");
	log(LOG_OK, "/user/DIR/b.txt' s name = %s", pk->name);

	struct list_head  * pch = &pp->children;
	do
	{
		pch = list_next(pch);
		if (pch == &pp->children) goto VFS_END;
		elements = to_struct(pch, vfs_node, thisnode);
		log(LOG_OK, "Element within user: %s", elements->name);
		
	} while (1);

VFS_END:

	mypath = (char *)kmalloc(sizeof("user/DIR/B.TXT"));
	kernel_memcpy(mypath, "user/DIR/B.TXT", sizeof("user/DIR/B.TXT") - 1);

	b = vfs_find_relative_node(vfs_get_root(), mypath);
	log(LOG_OK, "Find relative node root/user/DIR/B.TXT name = %s", b->name);


		mypath = (char *)kmalloc(sizeof("user/Goodbye.txt.txt"));
	kernel_memcpy(mypath, "user/Goodbye.txt.txt", sizeof("user/Goodbye.txt.txt") - 1);

	c = vfs_find_relative_node(vfs_get_root(), mypath);
	log(LOG_OK, "Find relative node root/user/Goodbye.txt.txt name = %s", c->name);
//
//	file_size = get_entry_filesize(b->mdata.node_data.entry.data);
//	start = get_entry_start_cluster(b->mdata.node_data.entry.data);
//	buf = (u8 *)kmalloc(file_size);
//	log(LOG_OK, "filesize = %d", file_size);
//	b->in_ops->vop_read(0, b, 0, file_size, buf);
//	buf[file_size] = 0;
//	log(LOG_OK, "B.txt content:");
//	kernel_printf("%s\n", buf);
//	kfree(buf);
//	buf = (u8 *)kmalloc(sizeof("Successful Writing!!!"));
//	log(LOG_OK, "Writing string \"Writing\" into B.TXT");
//	kernel_memcpy(buf, "Successful Writing!!!", sizeof("Successful Writing!!!") - 1);
//	b->in_ops->vop_write(0, b, 0, sizeof("Successful Writing!!!") - 1, buf);
//	file_size = get_entry_filesize(b->mdata.node_data.entry.data);
//	buf = (u8 *)kmalloc(file_size);
//	log(LOG_OK, "filesize = %d", file_size);
//	b->in_ops->vop_read(0, b, 0, file_size, buf);
//	buf[file_size] = 0;
//	log(LOG_OK, "B.txt content:");
//	kernel_printf("%s\n", buf);
//	kfree(buf);
//	if (vfs_get_dev() != NULL) log(LOG_OK, "VFS init successful");
//
//	log(LOG_OK, "Starting create node test");
	//fat_fs_create_node(pp, pp, "Hello", NODE_FILE );

	//mypath = (char *)kmalloc(sizeof("user/Hello"));
	//kernel_memcpy(mypath, "user/Hello", sizeof("user/Hello") - 1);
	//b = vfs_find_relative_node(vfs_get_root(), mypath);
	//buf = (u8 *)kmalloc(sizeof("Successful Writing!!!"));
	//kernel_memcpy(buf, "Successful Writing!!!", sizeof("Successful Writing!!!") - 1);
	//b->in_ops->vop_write(0, b, 0, sizeof("Successful Writing!!!") - 1, buf);
	//b->in_ops->vop_flush(b->tag);
//


	//c = vfs_find_relative_node(vfs_get_root(), mypath);
	//log(LOG_OK, "Find relative node root/user/Hello name = %s", c->name);


	//buf = (u8 *)kmalloc(sizeof("Successful Writing!!!"));
	//log(LOG_OK, "Writing string \"Writing\" into Hello");
	//kernel_memcpy(buf, "Successful Writing!!!", sizeof("Successful Writing!!!") - 1);
	//c->in_ops->vop_write(0, c, start, sizeof("Successful Writing!!!") - 1, buf);
	//file_size = get_entry_filesize(c->mdata.node_data.entry.data);
	//buf = (u8 *)kmalloc(file_size);
	//log(LOG_OK, "filesize = %d", file_size);
	//c->in_ops->vop_read(0, c, start, file_size, buf);
	//buf[file_size] = 0;
	//log(LOG_OK, "Hello content:");
	//kernel_printf("%s\n", buf);
	return;

}

/*get the dev node*/
vfs_node* vfs_get_dev()
{
	return vfs_find_child(root, "dev");
}


/*find the node with specific name*/
vfs_node* vfs_find_child(vfs_node * node, char * name)
{
	struct list_head * head = &node->children;
	struct list_head * find = head;
	do
	{
	//	log(LOG_OK, "%p", to_struct(find, vfs_node, thisnode));
		find = list_next(find);
		if (kernel_strcmp((to_struct(find, vfs_node, thisnode))->name, name) == 0)
			return (to_struct(find, vfs_node, thisnode));
	} while (find != head);
	return NULL;
}


vfs_node* vfs_create_device(char* name, u32 capabilities, vfs_node* tag, inode_ops* dev_ops)
{
	vfs_node* node = vfs_create_node(name, 1, NODE_DEVICE | NODE_READ, capabilities, 0, 0, tag, vfs_get_dev(), dev_ops);

	if (node == 0)
		return 0;
	vfs_add_child(vfs_get_dev(), node);
	return node;
}


vfs_node* vfs_find_relative_node(vfs_node* start, char* path)
{
	if (path == 0)
	{
		log(LOG_FAIL, "path not correct");
		while (1);
	}

	vfs_node* next = start;
	char* slash;
	int i;

	while (1)
	{
		for (i = 0; (*(path + i) != 0) && (*(path + i) != '/'); i++)
			;

		if (*(path + i) == 0) slash = 0;
		else
			slash = path + i;

		if (slash == 0)		// remaining path contains no slashes
		{
			// just find the last-on-path node and return
			next = vfs_find_child(next, path);
			return next;
		}
		else
		{
			//exit(1);
			*slash = 0;							// null terminate the path substring to make it a temp string
			
			next = vfs_find_child(next, path);

			*slash = '/';						// restore the substring
			path = slash + 1;					// continue to the next substring
		}

		if (next == 0)							// path could not be constructed. Child not found
		{
			log(LOG_FAIL, "this path is invalid");
			return 0;
		}
	}
	

	// we should never reach here
	return 0;
}

vfs_node* vfs_get_parent(vfs_node* node)
{
	if (node == 0)
		return 0;
	return node->parent;
}

vfs_node* vfs_find_node(char* path)
{
	//something/folder/another_folder/file.txt
	return vfs_find_relative_node(vfs_get_root(), path);
}



unsigned long vfs_stdin_read(u32 fd, vfs_node* node, u32 start, u32 count, void *iob)
{
	//kernel_scanf();
}

unsigned long vfs_stdout_write(u32 fd, vfs_node* node, u32 start, u32 count, void *iob)
{

	for (int i = start; i < start + count; ++i)
	{
		kernel_printf("%c", ((char*)iob)[i]);
	}
}

unsigned long vfs_stderr_write(u32 fd, vfs_node* node, u32 start, u32 count, void *iob)
{
	char * logerr;
	logerr = (char *)kmalloc(count + 1);
	kernel_memcpy(logerr, iob + start, count);
	logerr[count] = 0;
	log(LOG_FAIL, "Standard Error: %s", logerr);
}
