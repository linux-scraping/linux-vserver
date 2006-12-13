/*
 *  linux/kernel/vserver/device.c
 *
 *  Virtual Server: Device Support
 *
 *  Copyright (C) 2006  Herbert Pötzl
 *
 *  V0.01  device mapping basics
 *
 */

#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/hash.h>
// #include <linux/mount.h>

#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/vserver/base.h>
#include <linux/vserver/debug.h>
#include <linux/vserver/context.h>
#include <linux/vserver/device.h>
#include <linux/vserver/device_cmd.h>


#define DMAP_HASH_BITS	4

struct hlist_head dmap_main_hash[1 << DMAP_HASH_BITS];

static spinlock_t dmap_main_hash_lock = SPIN_LOCK_UNLOCKED;


struct vs_mapping {
	struct hlist_node dm_hlist;
	dev_t device;
	dev_t target;
	int flags;
};


kmem_cache_t *dmap_cachep __read_mostly;

int __init dmap_cache_init(void)
{
	dmap_cachep = kmem_cache_create("dmap_cache",
		sizeof(struct vs_mapping), 0,
		SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL, NULL);
	return 0;
}

__initcall(dmap_cache_init);


static inline unsigned int __hashval(dev_t dev, int bits)
{
	return hash_long((unsigned long)dev, bits);
}


/*	__hash_mapping()

	* add the mapping to the hash table			 */

static inline void __hash_mapping(struct vx_info *vxi, struct vs_mapping *vdm)
{
	spinlock_t *hash_lock = &dmap_main_hash_lock;
	struct hlist_head *head, *hash = dmap_main_hash;
	int device = vdm->device;

	spin_lock(hash_lock);
	vxdprintk(VXD_CBIT(misc, 8), "__hash_mapping: %p[#%d] %08x:%08x",
		vxi, vxi ? vxi->vx_id : 0, device, vdm->target);

	head = &hash[__hashval(device, DMAP_HASH_BITS)];
	hlist_add_head(&vdm->dm_hlist, head);
	spin_unlock(hash_lock);
}

/*	__unhash_mapping()

	* remove a mapping from the hash table			 */

static inline void __unhash_mapping(struct vx_info *vxi, struct vs_mapping *vdm)
{
	spinlock_t *hash_lock = &dmap_main_hash_lock;
	int device = vdm->device;

	spin_lock(hash_lock);
	vxdprintk(VXD_CBIT(misc, 8), "__unhash_mapping: %p[#%d] %08x:%08x",
		vxi, vxi ? vxi->vx_id : 0, device, vdm->target);

	hlist_del(&vdm->dm_hlist);
	spin_unlock(hash_lock);
}



static inline int __lookup_mapping(struct vx_info *vxi,
	dev_t device, dev_t *target, int *flags, umode_t mode)
{
	spinlock_t *hash_lock = &dmap_main_hash_lock;
	struct hlist_head *hash = dmap_main_hash;
	struct hlist_head *head = &hash[__hashval(device, DMAP_HASH_BITS)];
	struct hlist_node *pos;
	struct vs_mapping *vdm;
	int ret;

	spin_lock(hash_lock);
	hlist_for_each(pos, head) {
		vdm = hlist_entry(pos, struct vs_mapping, dm_hlist);

		if ((vdm->device == device) &&
			!((vdm->flags ^ mode) & S_IFMT))
			goto found;
	}

	/* compatible default for now */
	if (target)
		*target = device;
	if (flags)
		*flags = DATTR_OPEN;
	ret = 0;
	goto out;
found:
	if (target)
		*target = vdm->target;
	if (flags)
		*flags = vdm->flags;
	ret = 1;
out:
	spin_unlock(hash_lock);
	return ret;
}



int vs_map_device(struct vx_info *vxi,
	dev_t device, dev_t *target, umode_t mode)
{
	int ret, flags = DATTR_MASK;

	if (!vxi) {
		if (target)
			*target = device;
		goto out;
	}
	ret = __lookup_mapping(vxi, device, target, &flags, mode);
	printk("··· mapping device: %08x target: %08x flags: %04x mode: %04x mapped=%d\n",
		device, target ? *target : 0, flags, mode, ret);
out:
	return (flags & DATTR_MASK);
}



int do_set_mapping(struct vx_info *vxi,
	dev_t device, dev_t target, int flags, umode_t mode)
{
	struct vs_mapping *new;

	new = kmem_cache_alloc(dmap_cachep, SLAB_KERNEL);
	if (!new)
		return -ENOMEM;

	INIT_HLIST_NODE(&new->dm_hlist);
	new->device = device;
	new->target = target;
	new->flags = flags | mode;

	printk("··· device: %08x target: %08x\n", device, target);
	__hash_mapping(vxi, new);
	return 0;
}


static inline
int __user_device(const char __user *name, dev_t *dev, umode_t *mode)
{
	struct nameidata nd;
	int ret;

	if (!name) {
		*dev = 0;
		return 0;
	}
	ret = user_path_walk_link(name, &nd);
	if (ret)
		return ret;
	if (nd.dentry->d_inode) {
		*dev = nd.dentry->d_inode->i_rdev;
		*mode = nd.dentry->d_inode->i_mode;
	}
	path_release(&nd);
	return 0;
}

static inline
int __mapping_mode(dev_t device, dev_t target,
	umode_t device_mode, umode_t target_mode, umode_t *mode)
{
	if (device)
		*mode = device_mode & S_IFMT;
	else if (target)
		*mode = target_mode & S_IFMT;
	else
		*mode = 0;

	/* if both given, device and target mode have to match */
	if (device && target &&
		((device_mode ^ target_mode) & S_IFMT))
		return -EINVAL;
	return 0;
}


int vc_set_mapping(struct vx_info *vxi, void __user *data)
{
	struct vcmd_set_mapping_v0 vc_data;
	dev_t device = ~0, target = ~0;
	umode_t device_mode, target_mode, mode;
	int ret;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = __user_device(vc_data.device, &device, &device_mode);
	if (ret)
		return ret;
	ret = __user_device(vc_data.target, &target, &target_mode);
	if (ret)
		return ret;

	ret = __mapping_mode(device, target,
		device_mode, target_mode, &mode);
	if (ret)
		return ret;

	return do_set_mapping(vxi, device, target, vc_data.flags, mode);
}

#ifdef	CONFIG_COMPAT

int vc_set_mapping_x32(struct vx_info *vxi, void __user *data)
{
	struct vcmd_set_mapping_v0_x32 vc_data;
	dev_t device = ~0, target = ~0;
	umode_t device_mode, target_mode, mode;
	int ret;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = __user_device(compat_ptr(vc_data.device_ptr),
		&device, &device_mode);
	if (ret)
		return ret;
	ret = __user_device(compat_ptr(vc_data.target_ptr),
		&target, &target_mode);
	if (ret)
		return ret;

	ret = __mapping_mode(device, target,
		device_mode, target_mode, &mode);
	if (ret)
		return ret;

	return do_set_mapping(vxi, device, target, vc_data.flags, mode);
}

#endif	/* CONFIG_COMPAT */


