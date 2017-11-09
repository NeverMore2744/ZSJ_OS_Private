#include <vfs.h>
#include <list.h>


// device infomation




/* in our implementation, we only have one device */
typedef struct {
	const char *devname;
	struct inode *devnode;
	struct fs *fs;
	bool mountable;
	list_head *vdev_link;
} vfs_dev_t;


static list_head vdev_list;     // device info list in vfs layer

void
vfs_devlist_init(void) {
    INIT_LIST_HEAD(&vdev_list); //链表初始化
}

/*
 * vfs_mount - Mount a filesystem. Once we've found the device, call MOUNTFUNC to
 *             set up the filesystem and hand back a struct fs.
 *
 * The DATA argument is passed through unchanged to MOUNTFUNC.
 */

// int
// vfs_mount(const char *devname, int (*mountfunc)(struct device *dev, struct fs **fs_store)) {
//     int ret;

//     vfs_dev_t *vdev;
//     if ((ret = find_mount(devname, &vdev)) != 0) {
//         goto out;
//     }
//     if (vdev->fs != NULL) {
//         ret = -E_BUSY;
//         goto out;
//     }
//     assert(vdev->devname != NULL && vdev->mountable);

//     struct device *dev = vop_info(vdev->devnode, device);
//     if ((ret = mountfunc(dev, &(vdev->fs))) == 0) {
//         assert(vdev->fs != NULL);
//         //cprintf("vfs: mount %s.\n", vdev->devname);
//     }

// out:

//     return ret;
// }


// /*
//  * find_mount - Look for a mountable device named DEVNAME.
//  *              Should already hold vdev_list lock.
//  */
// static int
// find_mount(const char *devname, vfs_dev_t **vdev_store) {

//     assert(devname != NULL);
//     list_head *list = &vdev_list, *le = list;
//     while ((le = list_next(le)) != list) {
//         vfs_dev_t *vdev = to_struct((le), vfs_dev_t, vdev_link);
//         if (vdev->mountable && strcmp(vdev->devname, devname) == 0) {
//             *vdev_store = vdev;
//             return 0;
//         }
//     }
//     return -E_NO_DEV;
// }

/*
 * vfs_add_dev - Add a new device, by name. See  vfs_do_add information for the description of
 *               mountable.
 */
int
vfs_add_dev(const char *devname, struct inode *devnode, bool mountable) {
    return vfs_do_add(devname, devnode, NULL, mountable);
}


/*
* vfs_do_add - Add a new device to the VFS layer's device table.
*
* If "mountable" is set, the device will be treated as one that expects
* to have a filesystem mounted on it, and a raw device will be created
* for direct access.
*/
static int
vfs_do_add(const char *devname, struct inode *devnode, struct fs *fs, bool mountable) {
    assert(devname != NULL);
//    assert((devnode == NULL && !mountable) || (devnode != NULL && check_inode_type(devnode, device)));
/*    if (strlen(devname) > FS_MAX_DNAME_LEN) {	// check devname exceed?
        return -E_TOO_BIG;
    }*/ 
    //later

 /*   int ret = -E_NO_MEM;
    char *s_devname;
    if ((s_devname = strdup(devname)) == NULL) {
        return ret;
    }*/



    vfs_dev_t *vdev;
    if ((vdev = kmalloc(sizeof(vfs_dev_t))) == NULL) {
        goto failed_cleanup_name;
    }

    ret = -E_EXISTS;
//    lock_vdev_list();
/*    if (!check_devname_conflict(s_devname)) {
        unlock_vdev_list();
        goto failed_cleanup_vdev;
    }*/
    vdev->devname = devname;
    vdev->devnode = devnode;
    vdev->mountable = mountable;
    vdev->fs = fs;

    list_add(&vdev_list, &(vdev->vdev_link));
//    unlock_vdev_list();
    return 0;

failed_cleanup_vdev:
    kfree(vdev);
failed_cleanup_name:
    kfree(s_devname);

    return ret;
}

