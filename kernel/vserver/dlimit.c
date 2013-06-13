/*
 *  linux/kernel/vserver/dlimit.c
 *
 *  Virtual Server: Context Disk Limits
 *
 *  Copyright (C) 2004-2009  Herbert Pötzl
 *
 *  V0.01  initial version
 *  V0.02  compat32 splitup
 *  V0.03  extended interface
 *
 */

#include <linux/statfs.h>
#include <linux/sched.h>
#include <linux/namei.h>
#include <linux/vs_tag.h>
#include <linux/vs_dlimit.h>
#include <linux/vserver/dlimit_cmd.h>
#include <linux/slab.h>
// #include <linux/gfp.h>

#include <asm/uaccess.h>

/*	__alloc_dl_info()

	* allocate an initialized dl_info struct
	* doesn't make it visible (hash)			*/

static struct dl_info *__alloc_dl_info(struct super_block *sb, vtag_t tag)
{
	struct dl_info *new = NULL;

	vxdprintk(VXD_CBIT(dlim, 5),
		"alloc_dl_info(%p,%d)*", sb, tag);

	/* would this benefit from a slab cache? */
	new = kmalloc(sizeof(struct dl_info), GFP_KERNEL);
	if (!new)
		return 0;

	memset(new, 0, sizeof(struct dl_info));
	new->dl_tag = tag;
	new->dl_sb = sb;
	// INIT_RCU_HEAD(&new->dl_rcu);
	INIT_HLIST_NODE(&new->dl_hlist);
	spin_lock_init(&new->dl_lock);
	atomic_set(&new->dl_refcnt, 0);
	atomic_set(&new->dl_usecnt, 0);

	/* rest of init goes here */

	vxdprintk(VXD_CBIT(dlim, 4),
		"alloc_dl_info(%p,%d) = %p", sb, tag, new);
	return new;
}

/*	__dealloc_dl_info()

	* final disposal of dl_info				*/

static void __dealloc_dl_info(struct dl_info *dli)
{
	vxdprintk(VXD_CBIT(dlim, 4),
		"dealloc_dl_info(%p)", dli);

	dli->dl_hlist.next = LIST_POISON1;
	dli->dl_tag = -1;
	dli->dl_sb = 0;

	BUG_ON(atomic_read(&dli->dl_usecnt));
	BUG_ON(atomic_read(&dli->dl_refcnt));

	kfree(dli);
}


/*	hash table for dl_info hash */

#define DL_HASH_SIZE	13

struct hlist_head dl_info_hash[DL_HASH_SIZE];

static DEFINE_SPINLOCK(dl_info_hash_lock);


static inline unsigned int __hashval(struct super_block *sb, vtag_t tag)
{
	return ((tag ^ (unsigned long)sb) % DL_HASH_SIZE);
}



/*	__hash_dl_info()

	* add the dli to the global hash table
	* requires the hash_lock to be held			*/

static inline void __hash_dl_info(struct dl_info *dli)
{
	struct hlist_head *head;

	vxdprintk(VXD_CBIT(dlim, 6),
		"__hash_dl_info: %p[#%d]", dli, dli->dl_tag);
	get_dl_info(dli);
	head = &dl_info_hash[__hashval(dli->dl_sb, dli->dl_tag)];
	hlist_add_head_rcu(&dli->dl_hlist, head);
}

/*	__unhash_dl_info()

	* remove the dli from the global hash table
	* requires the hash_lock to be held			*/

static inline void __unhash_dl_info(struct dl_info *dli)
{
	vxdprintk(VXD_CBIT(dlim, 6),
		"__unhash_dl_info: %p[#%d]", dli, dli->dl_tag);
	hlist_del_rcu(&dli->dl_hlist);
	put_dl_info(dli);
}


/*	__lookup_dl_info()

	* requires the rcu_read_lock()
	* doesn't increment the dl_refcnt			*/

static inline struct dl_info *__lookup_dl_info(struct super_block *sb, vtag_t tag)
{
	struct hlist_head *head = &dl_info_hash[__hashval(sb, tag)];
	struct dl_info *dli;

	hlist_for_each_entry_rcu(dli, head, dl_hlist) {
		if (dli->dl_tag == tag && dli->dl_sb == sb)
			return dli;
	}
	return NULL;
}


struct dl_info *locate_dl_info(struct super_block *sb, vtag_t tag)
{
	struct dl_info *dli;

	rcu_read_lock();
	dli = get_dl_info(__lookup_dl_info(sb, tag));
	vxdprintk(VXD_CBIT(dlim, 7),
		"locate_dl_info(%p,#%d) = %p", sb, tag, dli);
	rcu_read_unlock();
	return dli;
}

void rcu_free_dl_info(struct rcu_head *head)
{
	struct dl_info *dli = container_of(head, struct dl_info, dl_rcu);
	int usecnt, refcnt;

	BUG_ON(!dli || !head);

	usecnt = atomic_read(&dli->dl_usecnt);
	BUG_ON(usecnt < 0);

	refcnt = atomic_read(&dli->dl_refcnt);
	BUG_ON(refcnt < 0);

	vxdprintk(VXD_CBIT(dlim, 3),
		"rcu_free_dl_info(%p)", dli);
	if (!usecnt)
		__dealloc_dl_info(dli);
	else
		printk("!!! rcu didn't free\n");
}




static int do_addrem_dlimit(uint32_t id, const char __user *name,
	uint32_t flags, int add)
{
	struct path path;
	int ret;

	ret = user_lpath(name, &path);
	if (!ret) {
		struct super_block *sb;
		struct dl_info *dli;

		ret = -EINVAL;
		if (!path.dentry->d_inode)
			goto out_release;
		if (!(sb = path.dentry->d_inode->i_sb))
			goto out_release;

		if (add) {
			dli = __alloc_dl_info(sb, id);
			spin_lock(&dl_info_hash_lock);

			ret = -EEXIST;
			if (__lookup_dl_info(sb, id))
				goto out_unlock;
			__hash_dl_info(dli);
			dli = NULL;
		} else {
			spin_lock(&dl_info_hash_lock);
			dli = __lookup_dl_info(sb, id);

			ret = -ESRCH;
			if (!dli)
				goto out_unlock;
			__unhash_dl_info(dli);
		}
		ret = 0;
	out_unlock:
		spin_unlock(&dl_info_hash_lock);
		if (add && dli)
			__dealloc_dl_info(dli);
	out_release:
		path_put(&path);
	}
	return ret;
}

int vc_add_dlimit(uint32_t id, void __user *data)
{
	struct vcmd_ctx_dlimit_base_v0 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_addrem_dlimit(id, vc_data.name, vc_data.flags, 1);
}

int vc_rem_dlimit(uint32_t id, void __user *data)
{
	struct vcmd_ctx_dlimit_base_v0 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_addrem_dlimit(id, vc_data.name, vc_data.flags, 0);
}

#ifdef	CONFIG_COMPAT

int vc_add_dlimit_x32(uint32_t id, void __user *data)
{
	struct vcmd_ctx_dlimit_base_v0_x32 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_addrem_dlimit(id,
		compat_ptr(vc_data.name_ptr), vc_data.flags, 1);
}

int vc_rem_dlimit_x32(uint32_t id, void __user *data)
{
	struct vcmd_ctx_dlimit_base_v0_x32 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_addrem_dlimit(id,
		compat_ptr(vc_data.name_ptr), vc_data.flags, 0);
}

#endif	/* CONFIG_COMPAT */


static inline
int do_set_dlimit(uint32_t id, const char __user *name,
	uint32_t space_used, uint32_t space_total,
	uint32_t inodes_used, uint32_t inodes_total,
	uint32_t reserved, uint32_t flags)
{
	struct path path;
	int ret;

	ret = user_lpath(name, &path);
	if (!ret) {
		struct super_block *sb;
		struct dl_info *dli;

		ret = -EINVAL;
		if (!path.dentry->d_inode)
			goto out_release;
		if (!(sb = path.dentry->d_inode->i_sb))
			goto out_release;

		/* sanity checks */
		if ((reserved != CDLIM_KEEP &&
			reserved > 100) ||
			(inodes_used != CDLIM_KEEP &&
			inodes_used > inodes_total) ||
			(space_used != CDLIM_KEEP &&
			space_used > space_total))
			goto out_release;

		ret = -ESRCH;
		dli = locate_dl_info(sb, id);
		if (!dli)
			goto out_release;

		spin_lock(&dli->dl_lock);

		if (inodes_used != CDLIM_KEEP)
			dli->dl_inodes_used = inodes_used;
		if (inodes_total != CDLIM_KEEP)
			dli->dl_inodes_total = inodes_total;
		if (space_used != CDLIM_KEEP)
			dli->dl_space_used = dlimit_space_32to64(
				space_used, flags, DLIMS_USED);

		if (space_total == CDLIM_INFINITY)
			dli->dl_space_total = DLIM_INFINITY;
		else if (space_total != CDLIM_KEEP)
			dli->dl_space_total = dlimit_space_32to64(
				space_total, flags, DLIMS_TOTAL);

		if (reserved != CDLIM_KEEP)
			dli->dl_nrlmult = (1 << 10) * (100 - reserved) / 100;

		spin_unlock(&dli->dl_lock);

		put_dl_info(dli);
		ret = 0;

	out_release:
		path_put(&path);
	}
	return ret;
}

int vc_set_dlimit(uint32_t id, void __user *data)
{
	struct vcmd_ctx_dlimit_v0 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_dlimit(id, vc_data.name,
		vc_data.space_used, vc_data.space_total,
		vc_data.inodes_used, vc_data.inodes_total,
		vc_data.reserved, vc_data.flags);
}

#ifdef	CONFIG_COMPAT

int vc_set_dlimit_x32(uint32_t id, void __user *data)
{
	struct vcmd_ctx_dlimit_v0_x32 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_dlimit(id, compat_ptr(vc_data.name_ptr),
		vc_data.space_used, vc_data.space_total,
		vc_data.inodes_used, vc_data.inodes_total,
		vc_data.reserved, vc_data.flags);
}

#endif	/* CONFIG_COMPAT */


static inline
int do_get_dlimit(uint32_t id, const char __user *name,
	uint32_t *space_used, uint32_t *space_total,
	uint32_t *inodes_used, uint32_t *inodes_total,
	uint32_t *reserved, uint32_t *flags)
{
	struct path path;
	int ret;

	ret = user_lpath(name, &path);
	if (!ret) {
		struct super_block *sb;
		struct dl_info *dli;

		ret = -EINVAL;
		if (!path.dentry->d_inode)
			goto out_release;
		if (!(sb = path.dentry->d_inode->i_sb))
			goto out_release;

		ret = -ESRCH;
		dli = locate_dl_info(sb, id);
		if (!dli)
			goto out_release;

		spin_lock(&dli->dl_lock);
		*inodes_used = dli->dl_inodes_used;
		*inodes_total = dli->dl_inodes_total;

		*space_used = dlimit_space_64to32(
			dli->dl_space_used, flags, DLIMS_USED);

		if (dli->dl_space_total == DLIM_INFINITY)
			*space_total = CDLIM_INFINITY;
		else
			*space_total = dlimit_space_64to32(
				dli->dl_space_total, flags, DLIMS_TOTAL);

		*reserved = 100 - ((dli->dl_nrlmult * 100 + 512) >> 10);
		spin_unlock(&dli->dl_lock);

		put_dl_info(dli);
		ret = -EFAULT;

		ret = 0;
	out_release:
		path_put(&path);
	}
	return ret;
}


int vc_get_dlimit(uint32_t id, void __user *data)
{
	struct vcmd_ctx_dlimit_v0 vc_data;
	int ret;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = do_get_dlimit(id, vc_data.name,
		&vc_data.space_used, &vc_data.space_total,
		&vc_data.inodes_used, &vc_data.inodes_total,
		&vc_data.reserved, &vc_data.flags);
	if (ret)
		return ret;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

#ifdef	CONFIG_COMPAT

int vc_get_dlimit_x32(uint32_t id, void __user *data)
{
	struct vcmd_ctx_dlimit_v0_x32 vc_data;
	int ret;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = do_get_dlimit(id, compat_ptr(vc_data.name_ptr),
		&vc_data.space_used, &vc_data.space_total,
		&vc_data.inodes_used, &vc_data.inodes_total,
		&vc_data.reserved, &vc_data.flags);
	if (ret)
		return ret;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

#endif	/* CONFIG_COMPAT */


void vx_vsi_statfs(struct super_block *sb, struct kstatfs *buf)
{
	struct dl_info *dli;
	__u64 blimit, bfree, bavail;
	__u32 ifree;

	dli = locate_dl_info(sb, dx_current_tag());
	if (!dli)
		return;

	spin_lock(&dli->dl_lock);
	if (dli->dl_inodes_total == (unsigned long)DLIM_INFINITY)
		goto no_ilim;

	/* reduce max inodes available to limit */
	if (buf->f_files > dli->dl_inodes_total)
		buf->f_files = dli->dl_inodes_total;

	ifree = dli->dl_inodes_total - dli->dl_inodes_used;
	/* reduce free inodes to min */
	if (ifree < buf->f_ffree)
		buf->f_ffree = ifree;

no_ilim:
	if (dli->dl_space_total == DLIM_INFINITY)
		goto no_blim;

	blimit = dli->dl_space_total >> sb->s_blocksize_bits;

	if (dli->dl_space_total < dli->dl_space_used)
		bfree = 0;
	else
		bfree = (dli->dl_space_total - dli->dl_space_used)
			>> sb->s_blocksize_bits;

	bavail = ((dli->dl_space_total >> 10) * dli->dl_nrlmult);
	if (bavail < dli->dl_space_used)
		bavail = 0;
	else
		bavail = (bavail - dli->dl_space_used)
			>> sb->s_blocksize_bits;

	/* reduce max space available to limit */
	if (buf->f_blocks > blimit)
		buf->f_blocks = blimit;

	/* reduce free space to min */
	if (bfree < buf->f_bfree)
		buf->f_bfree = bfree;

	/* reduce avail space to min */
	if (bavail < buf->f_bavail)
		buf->f_bavail = bavail;

no_blim:
	spin_unlock(&dli->dl_lock);
	put_dl_info(dli);

	return;
}

#include <linux/module.h>

EXPORT_SYMBOL_GPL(locate_dl_info);
EXPORT_SYMBOL_GPL(rcu_free_dl_info);

