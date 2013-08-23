/*
 *  linux/drivers/block/vroot.c
 *
 *  written by Herbert Pötzl, 9/11/2002
 *  ported to 2.6.10 by Herbert Pötzl, 30/12/2004
 *
 *  based on the loop.c code by Theodore Ts'o.
 *
 * Copyright (C) 2002-2007 by Herbert Pötzl.
 * Redistribution of this file is permitted under the
 * GNU General Public License.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/file.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/slab.h>

#include <linux/vroot.h>
#include <linux/vs_context.h>


static int max_vroot = 8;

static struct vroot_device *vroot_dev;
static struct gendisk **disks;


static int vroot_set_dev(
	struct vroot_device *vr,
	struct block_device *bdev,
	unsigned int arg)
{
	struct block_device *real_bdev;
	struct file *file;
	struct inode *inode;
	int error;

	error = -EBUSY;
	if (vr->vr_state != Vr_unbound)
		goto out;

	error = -EBADF;
	file = fget(arg);
	if (!file)
		goto out;

	error = -EINVAL;
	inode = file->f_dentry->d_inode;


	if (S_ISBLK(inode->i_mode)) {
		real_bdev = inode->i_bdev;
		vr->vr_device = real_bdev;
		__iget(real_bdev->bd_inode);
	} else
		goto out_fput;

	vxdprintk(VXD_CBIT(misc, 0),
		"vroot[%d]_set_dev: dev=" VXF_DEV,
		vr->vr_number, VXD_DEV(real_bdev));

	vr->vr_state = Vr_bound;
	error = 0;

 out_fput:
	fput(file);
 out:
	return error;
}

static int vroot_clr_dev(
	struct vroot_device *vr,
	struct block_device *bdev)
{
	struct block_device *real_bdev;

	if (vr->vr_state != Vr_bound)
		return -ENXIO;
	if (vr->vr_refcnt > 1)	/* we needed one fd for the ioctl */
		return -EBUSY;

	real_bdev = vr->vr_device;

	vxdprintk(VXD_CBIT(misc, 0),
		"vroot[%d]_clr_dev: dev=" VXF_DEV,
		vr->vr_number, VXD_DEV(real_bdev));

	bdput(real_bdev);
	vr->vr_state = Vr_unbound;
	vr->vr_device = NULL;
	return 0;
}


static int vr_ioctl(struct block_device *bdev, fmode_t mode,
	unsigned int cmd, unsigned long arg)
{
	struct vroot_device *vr = bdev->bd_disk->private_data;
	int err;

	down(&vr->vr_ctl_mutex);
	switch (cmd) {
	case VROOT_SET_DEV:
		err = vroot_set_dev(vr, bdev, arg);
		break;
	case VROOT_CLR_DEV:
		err = vroot_clr_dev(vr, bdev);
		break;
	default:
		err = -EINVAL;
		break;
	}
	up(&vr->vr_ctl_mutex);
	return err;
}

static int vr_open(struct block_device *bdev, fmode_t mode)
{
	struct vroot_device *vr = bdev->bd_disk->private_data;

	down(&vr->vr_ctl_mutex);
	vr->vr_refcnt++;
	up(&vr->vr_ctl_mutex);
	return 0;
}

static void vr_release(struct gendisk *disk, fmode_t mode)
{
	struct vroot_device *vr = disk->private_data;

	down(&vr->vr_ctl_mutex);
	--vr->vr_refcnt;
	up(&vr->vr_ctl_mutex);
}

static struct block_device_operations vr_fops = {
	.owner =	THIS_MODULE,
	.open =		vr_open,
	.release =	vr_release,
	.ioctl =	vr_ioctl,
};

static void vroot_make_request(struct request_queue *q, struct bio *bio)
{
	printk("vroot_make_request %p, %p\n", q, bio);
	bio_io_error(bio);
}

struct block_device *__vroot_get_real_bdev(struct block_device *bdev)
{
	struct inode *inode = bdev->bd_inode;
	struct vroot_device *vr;
	struct block_device *real_bdev;
	int minor = iminor(inode);

	vr = &vroot_dev[minor];
	real_bdev = vr->vr_device;

	vxdprintk(VXD_CBIT(misc, 0),
		"vroot[%d]_get_real_bdev: dev=" VXF_DEV,
		vr->vr_number, VXD_DEV(real_bdev));

	if (vr->vr_state != Vr_bound)
		return ERR_PTR(-ENXIO);

	__iget(real_bdev->bd_inode);
	return real_bdev;
}



/*
 * And now the modules code and kernel interface.
 */

module_param(max_vroot, int, 0);

MODULE_PARM_DESC(max_vroot, "Maximum number of vroot devices (1-256)");
MODULE_LICENSE("GPL");
MODULE_ALIAS_BLOCKDEV_MAJOR(VROOT_MAJOR);

MODULE_AUTHOR ("Herbert Pötzl");
MODULE_DESCRIPTION ("Virtual Root Device Mapper");


int __init vroot_init(void)
{
	int err, i;

	if (max_vroot < 1 || max_vroot > 256) {
		max_vroot = MAX_VROOT_DEFAULT;
		printk(KERN_WARNING "vroot: invalid max_vroot "
			"(must be between 1 and 256), "
			"using default (%d)\n", max_vroot);
	}

	if (register_blkdev(VROOT_MAJOR, "vroot"))
		return -EIO;

	err = -ENOMEM;
	vroot_dev = kmalloc(max_vroot * sizeof(struct vroot_device), GFP_KERNEL);
	if (!vroot_dev)
		goto out_mem1;
	memset(vroot_dev, 0, max_vroot * sizeof(struct vroot_device));

	disks = kmalloc(max_vroot * sizeof(struct gendisk *), GFP_KERNEL);
	if (!disks)
		goto out_mem2;

	for (i = 0; i < max_vroot; i++) {
		disks[i] = alloc_disk(1);
		if (!disks[i])
			goto out_mem3;
		disks[i]->queue = blk_alloc_queue(GFP_KERNEL);
		if (!disks[i]->queue)
			goto out_mem3;
		blk_queue_make_request(disks[i]->queue, vroot_make_request);
	}

	for (i = 0; i < max_vroot; i++) {
		struct vroot_device *vr = &vroot_dev[i];
		struct gendisk *disk = disks[i];

		memset(vr, 0, sizeof(*vr));
		sema_init(&vr->vr_ctl_mutex, 1);
		vr->vr_number = i;
		disk->major = VROOT_MAJOR;
		disk->first_minor = i;
		disk->fops = &vr_fops;
		sprintf(disk->disk_name, "vroot%d", i);
		disk->private_data = vr;
	}

	err = register_vroot_grb(&__vroot_get_real_bdev);
	if (err)
		goto out_mem3;

	for (i = 0; i < max_vroot; i++)
		add_disk(disks[i]);
	printk(KERN_INFO "vroot: loaded (max %d devices)\n", max_vroot);
	return 0;

out_mem3:
	while (i--)
		put_disk(disks[i]);
	kfree(disks);
out_mem2:
	kfree(vroot_dev);
out_mem1:
	unregister_blkdev(VROOT_MAJOR, "vroot");
	printk(KERN_ERR "vroot: ran out of memory\n");
	return err;
}

void vroot_exit(void)
{
	int i;

	if (unregister_vroot_grb(&__vroot_get_real_bdev))
		printk(KERN_WARNING "vroot: cannot unregister grb\n");

	for (i = 0; i < max_vroot; i++) {
		del_gendisk(disks[i]);
		put_disk(disks[i]);
	}
	unregister_blkdev(VROOT_MAJOR, "vroot");

	kfree(disks);
	kfree(vroot_dev);
}

module_init(vroot_init);
module_exit(vroot_exit);

#ifndef MODULE

static int __init max_vroot_setup(char *str)
{
	max_vroot = simple_strtol(str, NULL, 0);
	return 1;
}

__setup("max_vroot=", max_vroot_setup);

#endif

