/*
 *  linux/kernel/vserver/device.c
 *
 *  Linux-VServer: Device Support
 *
 *  Copyright (C) 2006  Herbert Pötzl
 *  Copyright (C) 2007  Daniel Hokka Zakrisson
 *
 *  V0.01  device mapping basics
 *  V0.02  added defaults
 *
 */

#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/hash.h>

#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/vserver/base.h>
#include <linux/vserver/debug.h>
#include <linux/vserver/context.h>
#include <linux/vserver/device.h>
#include <linux/vserver/device_cmd.h>


#define DMAP_HASH_BITS	4


struct vs_mapping {
	union {
		struct hlist_node hlist;
		struct list_head list;
	} u;
#define dm_hlist	u.hlist
#define dm_list		u.list
	vxid_t xid;
	dev_t device;
	struct vx_dmap_target target;
};


static struct hlist_head dmap_main_hash[1 << DMAP_HASH_BITS];

static DEFINE_SPINLOCK(dmap_main_hash_lock);

static struct vx_dmap_target dmap_defaults[2] = {
	{ .flags = DATTR_OPEN },
	{ .flags = DATTR_OPEN },
};


struct kmem_cache *dmap_cachep __read_mostly;

int __init dmap_cache_init(void)
{
	dmap_cachep = kmem_cache_create("dmap_cache",
		sizeof(struct vs_mapping), 0,
		SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL);
	return 0;
}

__initcall(dmap_cache_init);


static inline unsigned int __hashval(dev_t dev, int bits)
{
	return hash_long((unsigned long)dev, bits);
}


/*	__hash_mapping()
 *	add the mapping to the hash table
 */
static inline void __hash_mapping(struct vx_info *vxi, struct vs_mapping *vdm)
{
	spinlock_t *hash_lock = &dmap_main_hash_lock;
	struct hlist_head *head, *hash = dmap_main_hash;
	int device = vdm->device;

	spin_lock(hash_lock);
	vxdprintk(VXD_CBIT(misc, 8), "__hash_mapping: %p[#%d] %08x:%08x",
		vxi, vxi ? vxi->vx_id : 0, device, vdm->target.target);

	head = &hash[__hashval(device, DMAP_HASH_BITS)];
	hlist_add_head(&vdm->dm_hlist, head);
	spin_unlock(hash_lock);
}


static inline int __mode_to_default(umode_t mode)
{
	switch (mode) {
	case S_IFBLK:
		return 0;
	case S_IFCHR:
		return 1;
	default:
		BUG();
	}
}


/*	__set_default()
 *	set a default
 */
static inline void __set_default(struct vx_info *vxi, umode_t mode,
	struct vx_dmap_target *vdmt)
{
	spinlock_t *hash_lock = &dmap_main_hash_lock;
	spin_lock(hash_lock);

	if (vxi)
		vxi->dmap.targets[__mode_to_default(mode)] = *vdmt;
	else
		dmap_defaults[__mode_to_default(mode)] = *vdmt;


	spin_unlock(hash_lock);

	vxdprintk(VXD_CBIT(misc, 8), "__set_default: %p[#%u] %08x %04x",
		  vxi, vxi ? vxi->vx_id : 0, vdmt->target, vdmt->flags);
}


/*	__remove_default()
 *	remove a default
 */
static inline int __remove_default(struct vx_info *vxi, umode_t mode)
{
	spinlock_t *hash_lock = &dmap_main_hash_lock;
	spin_lock(hash_lock);

	if (vxi)
		vxi->dmap.targets[__mode_to_default(mode)].flags = 0;
	else	/* remove == reset */
		dmap_defaults[__mode_to_default(mode)].flags = DATTR_OPEN | mode;

	spin_unlock(hash_lock);
	return 0;
}


/*	__find_mapping()
 *	find a mapping in the hash table
 *
 *	caller must hold hash_lock
 */
static inline int __find_mapping(vxid_t xid, dev_t device, umode_t mode,
	struct vs_mapping **local, struct vs_mapping **global)
{
	struct hlist_head *hash = dmap_main_hash;
	struct hlist_head *head = &hash[__hashval(device, DMAP_HASH_BITS)];
	struct hlist_node *pos;
	struct vs_mapping *vdm;

	*local = NULL;
	if (global)
		*global = NULL;

	hlist_for_each(pos, head) {
		vdm = hlist_entry(pos, struct vs_mapping, dm_hlist);

		if ((vdm->device == device) &&
			!((vdm->target.flags ^ mode) & S_IFMT)) {
			if (vdm->xid == xid) {
				*local = vdm;
				return 1;
			} else if (global && vdm->xid == 0)
				*global = vdm;
		}
	}

	if (global && *global)
		return 0;
	else
		return -ENOENT;
}


/*	__lookup_mapping()
 *	find a mapping and store the result in target and flags
 */
static inline int __lookup_mapping(struct vx_info *vxi,
	dev_t device, dev_t *target, int *flags, umode_t mode)
{
	spinlock_t *hash_lock = &dmap_main_hash_lock;
	struct vs_mapping *vdm, *global;
	struct vx_dmap_target *vdmt;
	int ret = 0;
	vxid_t xid = vxi->vx_id;
	int index;

	spin_lock(hash_lock);
	if (__find_mapping(xid, device, mode, &vdm, &global) > 0) {
		ret = 1;
		vdmt = &vdm->target;
		goto found;
	}

	index = __mode_to_default(mode);
	if (vxi && vxi->dmap.targets[index].flags) {
		ret = 2;
		vdmt = &vxi->dmap.targets[index];
	} else if (global) {
		ret = 3;
		vdmt = &global->target;
		goto found;
	} else {
		ret = 4;
		vdmt = &dmap_defaults[index];
	}

found:
	if (target && (vdmt->flags & DATTR_REMAP))
		*target = vdmt->target;
	else if (target)
		*target = device;
	if (flags)
		*flags = vdmt->flags;

	spin_unlock(hash_lock);

	return ret;
}


/*	__remove_mapping()
 *	remove a mapping from the hash table
 */
static inline int __remove_mapping(struct vx_info *vxi, dev_t device,
	umode_t mode)
{
	spinlock_t *hash_lock = &dmap_main_hash_lock;
	struct vs_mapping *vdm = NULL;
	int ret = 0;

	spin_lock(hash_lock);

	ret = __find_mapping((vxi ? vxi->vx_id : 0), device, mode, &vdm,
		NULL);
	vxdprintk(VXD_CBIT(misc, 8), "__remove_mapping: %p[#%d] %08x %04x",
		vxi, vxi ? vxi->vx_id : 0, device, mode);
	if (ret < 0)
		goto out;
	hlist_del(&vdm->dm_hlist);

out:
	spin_unlock(hash_lock);
	if (vdm)
		kmem_cache_free(dmap_cachep, vdm);
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
	vxdprintk(VXD_CBIT(misc, 8), "vs_map_device: %08x target: %08x flags: %04x mode: %04x mapped=%d",
		device, target ? *target : 0, flags, mode, ret);
out:
	return (flags & DATTR_MASK);
}



static int do_set_mapping(struct vx_info *vxi,
	dev_t device, dev_t target, int flags, umode_t mode)
{
	if (device) {
		struct vs_mapping *new;

		new = kmem_cache_alloc(dmap_cachep, GFP_KERNEL);
		if (!new)
			return -ENOMEM;

		INIT_HLIST_NODE(&new->dm_hlist);
		new->device = device;
		new->target.target = target;
		new->target.flags = flags | mode;
		new->xid = (vxi ? vxi->vx_id : 0);

		vxdprintk(VXD_CBIT(misc, 8), "do_set_mapping: %08x target: %08x flags: %04x", device, target, flags);
		__hash_mapping(vxi, new);
	} else {
		struct vx_dmap_target new = {
			.target = target,
			.flags = flags | mode,
		};
		__set_default(vxi, mode, &new);
	}
	return 0;
}


static int do_unset_mapping(struct vx_info *vxi,
	dev_t device, dev_t target, int flags, umode_t mode)
{
	int ret = -EINVAL;

	if (device) {
		ret = __remove_mapping(vxi, device, mode);
		if (ret < 0)
			goto out;
	} else {
		ret = __remove_default(vxi, mode);
		if (ret < 0)
			goto out;
	}

out:
	return ret;
}


static inline int __user_device(const char __user *name, dev_t *dev,
	umode_t *mode)
{
	struct nameidata nd;
	int ret;

	if (!name) {
		*dev = 0;
		return 0;
	}
	ret = user_lpath(name, &nd.path);
	if (ret)
		return ret;
	if (nd.path.dentry->d_inode) {
		*dev = nd.path.dentry->d_inode->i_rdev;
		*mode = nd.path.dentry->d_inode->i_mode;
	}
	path_put(&nd.path);
	return 0;
}

static inline int __mapping_mode(dev_t device, dev_t target,
	umode_t device_mode, umode_t target_mode, umode_t *mode)
{
	if (device)
		*mode = device_mode & S_IFMT;
	else if (target)
		*mode = target_mode & S_IFMT;
	else
		return -EINVAL;

	/* if both given, device and target mode have to match */
	if (device && target &&
		((device_mode ^ target_mode) & S_IFMT))
		return -EINVAL;
	return 0;
}


static inline int do_mapping(struct vx_info *vxi, const char __user *device_path,
	const char __user *target_path, int flags, int set)
{
	dev_t device = ~0, target = ~0;
	umode_t device_mode = 0, target_mode = 0, mode;
	int ret;

	ret = __user_device(device_path, &device, &device_mode);
	if (ret)
		return ret;
	ret = __user_device(target_path, &target, &target_mode);
	if (ret)
		return ret;

	ret = __mapping_mode(device, target,
		device_mode, target_mode, &mode);
	if (ret)
		return ret;

	if (set)
		return do_set_mapping(vxi, device, target,
			flags, mode);
	else
		return do_unset_mapping(vxi, device, target,
			flags, mode);
}


int vc_set_mapping(struct vx_info *vxi, void __user *data)
{
	struct vcmd_set_mapping_v0 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_mapping(vxi, vc_data.device, vc_data.target,
		vc_data.flags, 1);
}

int vc_unset_mapping(struct vx_info *vxi, void __user *data)
{
	struct vcmd_set_mapping_v0 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_mapping(vxi, vc_data.device, vc_data.target,
		vc_data.flags, 0);
}


#ifdef	CONFIG_COMPAT

int vc_set_mapping_x32(struct vx_info *vxi, void __user *data)
{
	struct vcmd_set_mapping_v0_x32 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_mapping(vxi, compat_ptr(vc_data.device_ptr),
		compat_ptr(vc_data.target_ptr), vc_data.flags, 1);
}

int vc_unset_mapping_x32(struct vx_info *vxi, void __user *data)
{
	struct vcmd_set_mapping_v0_x32 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_mapping(vxi, compat_ptr(vc_data.device_ptr),
		compat_ptr(vc_data.target_ptr), vc_data.flags, 0);
}

#endif	/* CONFIG_COMPAT */


