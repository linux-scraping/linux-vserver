/*
 *  linux/kernel/vserver/dlimit.c
 *
 *  Virtual Server: Context Disk Limits
 *
 *  Copyright (C) 2004-2005  Herbert P�tzl
 *
 *  V0.01  initial version
 *
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/namespace.h>
#include <linux/namei.h>
#include <linux/statfs.h>
#include <linux/vserver/switch.h>
#include <linux/vs_context.h>
#include <linux/vs_dlimit.h>

#include <asm/errno.h>
#include <asm/uaccess.h>

/*	__alloc_dl_info()

	* allocate an initialized dl_info struct
	* doesn't make it visible (hash)			*/

static struct dl_info *__alloc_dl_info(struct super_block *sb, xid_t xid)
{
	struct dl_info *new = NULL;

	vxdprintk(VXD_CBIT(dlim, 5),
		"alloc_dl_info(%p,%d)*", sb, xid);

	/* would this benefit from a slab cache? */
	new = kmalloc(sizeof(struct dl_info), GFP_KERNEL);
	if (!new)
		return 0;

	memset (new, 0, sizeof(struct dl_info));
	new->dl_xid = xid;
	new->dl_sb = sb;
	INIT_RCU_HEAD(&new->dl_rcu);
	INIT_HLIST_NODE(&new->dl_hlist);
	spin_lock_init(&new->dl_lock);
	atomic_set(&new->dl_refcnt, 0);
	atomic_set(&new->dl_usecnt, 0);

	/* rest of init goes here */

	vxdprintk(VXD_CBIT(dlim, 4),
		"alloc_dl_info(%p,%d) = %p", sb, xid, new);
	return new;
}

/*	__dealloc_dl_info()

	* final disposal of dl_info				*/

static void __dealloc_dl_info(struct dl_info *dli)
{
	vxdprintk(VXD_CBIT(dlim, 4),
		"dealloc_dl_info(%p)", dli);

	dli->dl_hlist.next = LIST_POISON1;
	dli->dl_xid = -1;
	dli->dl_sb = 0;

	BUG_ON(atomic_read(&dli->dl_usecnt));
	BUG_ON(atomic_read(&dli->dl_refcnt));

	kfree(dli);
}


/*	hash table for dl_info hash */

#define DL_HASH_SIZE	13

struct hlist_head dl_info_hash[DL_HASH_SIZE];

static spinlock_t dl_info_hash_lock = SPIN_LOCK_UNLOCKED;


static inline unsigned int __hashval(struct super_block *sb, xid_t xid)
{
	return ((xid ^ (unsigned long)sb) % DL_HASH_SIZE);
}



/*	__hash_dl_info()

	* add the dli to the global hash table
	* requires the hash_lock to be held			*/

static inline void __hash_dl_info(struct dl_info *dli)
{
	struct hlist_head *head;

	vxdprintk(VXD_CBIT(dlim, 6),
		"__hash_dl_info: %p[#%d]", dli, dli->dl_xid);
	get_dl_info(dli);
	head = &dl_info_hash[__hashval(dli->dl_sb, dli->dl_xid)];
	hlist_add_head_rcu(&dli->dl_hlist, head);
}

/*	__unhash_dl_info()

	* remove the dli from the global hash table
	* requires the hash_lock to be held			*/

static inline void __unhash_dl_info(struct dl_info *dli)
{
	vxdprintk(VXD_CBIT(dlim, 6),
		"__unhash_dl_info: %p[#%d]", dli, dli->dl_xid);
	hlist_del_rcu(&dli->dl_hlist);
	put_dl_info(dli);
}


/*	__lookup_dl_info()

	* requires the rcu_read_lock()
	* doesn't increment the dl_refcnt			*/

static inline struct dl_info *__lookup_dl_info(struct super_block *sb, xid_t xid)
{
	struct hlist_head *head = &dl_info_hash[__hashval(sb, xid)];
	struct hlist_node *pos;

	hlist_for_each_rcu(pos, head) {
		struct dl_info *dli =
			hlist_entry(pos, struct dl_info, dl_hlist);

		if (dli->dl_xid == xid && dli->dl_sb == sb) {
			return dli;
		}
	}
	return NULL;
}


struct dl_info *locate_dl_info(struct super_block *sb, xid_t xid)
{
	struct dl_info *dli;

	rcu_read_lock();
	dli = get_dl_info(__lookup_dl_info(sb, xid));
	vxdprintk(VXD_CBIT(dlim, 7),
		"locate_dl_info(%p,#%d) = %p", sb, xid, dli);
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




int vc_add_dlimit(uint32_t id, void __user *data)
{
	struct nameidata nd;
	struct vcmd_ctx_dlimit_base_v0 vc_data;
	int ret;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_path_walk_link(vc_data.name, &nd);
	if (!ret) {
		struct super_block *sb;
		struct dl_info *dli;

		ret = -EINVAL;
		if (!nd.dentry->d_inode)
			goto out_release;
		if (!(sb = nd.dentry->d_inode->i_sb))
			goto out_release;

		dli = __alloc_dl_info(sb, id);
		spin_lock(&dl_info_hash_lock);

		ret = -EEXIST;
		if (__lookup_dl_info(sb, id))
			goto out_unlock;
		__hash_dl_info(dli);
		dli = NULL;
		ret = 0;

	out_unlock:
		spin_unlock(&dl_info_hash_lock);
		if (dli)
			__dealloc_dl_info(dli);
	out_release:
		path_release(&nd);
	}
	return ret;
}


int vc_rem_dlimit(uint32_t id, void __user *data)
{
	struct nameidata nd;
	struct vcmd_ctx_dlimit_base_v0 vc_data;
	int ret;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_path_walk_link(vc_data.name, &nd);
	if (!ret) {
		struct super_block *sb;
		struct dl_info *dli;

		ret = -EINVAL;
		if (!nd.dentry->d_inode)
			goto out_release;
		if (!(sb = nd.dentry->d_inode->i_sb))
			goto out_release;

		spin_lock(&dl_info_hash_lock);
		dli = __lookup_dl_info(sb, id);

		ret = -ESRCH;
		if (!dli)
			goto out_unlock;

		__unhash_dl_info(dli);
		ret = 0;

	out_unlock:
		spin_unlock(&dl_info_hash_lock);
	out_release:
		path_release(&nd);
	}
	return ret;
}


int vc_set_dlimit(uint32_t id, void __user *data)
{
	struct nameidata nd;
	struct vcmd_ctx_dlimit_v0 vc_data;
	int ret;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_path_walk_link(vc_data.name, &nd);
	if (!ret) {
		struct super_block *sb;
		struct dl_info *dli;

		ret = -EINVAL;
		if (!nd.dentry->d_inode)
			goto out_release;
		if (!(sb = nd.dentry->d_inode->i_sb))
			goto out_release;
		if ((vc_data.reserved != (uint32_t)CDLIM_KEEP &&
			vc_data.reserved > 100) ||
			(vc_data.inodes_used != (uint32_t)CDLIM_KEEP &&
			vc_data.inodes_used > vc_data.inodes_total) ||
			(vc_data.space_used != (uint32_t)CDLIM_KEEP &&
			vc_data.space_used > vc_data.space_total))
			goto out_release;

		ret = -ESRCH;
		dli = locate_dl_info(sb, id);
		if (!dli)
			goto out_release;

		spin_lock(&dli->dl_lock);

		if (vc_data.inodes_used != (uint32_t)CDLIM_KEEP)
			dli->dl_inodes_used = vc_data.inodes_used;
		if (vc_data.inodes_total != (uint32_t)CDLIM_KEEP)
			dli->dl_inodes_total = vc_data.inodes_total;
		if (vc_data.space_used != (uint32_t)CDLIM_KEEP) {
			dli->dl_space_used = vc_data.space_used;
			dli->dl_space_used <<= 10;
		}
		if (vc_data.space_total == (uint32_t)CDLIM_INFINITY)
			dli->dl_space_total = (uint64_t)CDLIM_INFINITY;
		else if (vc_data.space_total != (uint32_t)CDLIM_KEEP) {
			dli->dl_space_total = vc_data.space_total;
			dli->dl_space_total <<= 10;
		}
		if (vc_data.reserved != (uint32_t)CDLIM_KEEP)
			dli->dl_nrlmult = (1 << 10) * (100 - vc_data.reserved) / 100;

		spin_unlock(&dli->dl_lock);

		put_dl_info(dli);
		ret = 0;

	out_release:
		path_release(&nd);
	}
	return ret;
}

int vc_get_dlimit(uint32_t id, void __user *data)
{
	struct nameidata nd;
	struct vcmd_ctx_dlimit_v0 vc_data;
	int ret;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_path_walk_link(vc_data.name, &nd);
	if (!ret) {
		struct super_block *sb;
		struct dl_info *dli;

		ret = -EINVAL;
		if (!nd.dentry->d_inode)
			goto out_release;
		if (!(sb = nd.dentry->d_inode->i_sb))
			goto out_release;
		if (vc_data.reserved > 100 ||
			vc_data.inodes_used > vc_data.inodes_total ||
			vc_data.space_used > vc_data.space_total)
			goto out_release;

		ret = -ESRCH;
		dli = locate_dl_info(sb, id);
		if (!dli)
			goto out_release;

		spin_lock(&dli->dl_lock);
		vc_data.inodes_used = dli->dl_inodes_used;
		vc_data.inodes_total = dli->dl_inodes_total;
		vc_data.space_used = dli->dl_space_used >> 10;
		if (dli->dl_space_total == (uint64_t)CDLIM_INFINITY)
			vc_data.space_total = (uint32_t)CDLIM_INFINITY;
		else
			vc_data.space_total = dli->dl_space_total >> 10;

		vc_data.reserved = 100 - ((dli->dl_nrlmult * 100 + 512) >> 10);
		spin_unlock(&dli->dl_lock);

		put_dl_info(dli);
		ret = -EFAULT;
		if (copy_to_user(data, &vc_data, sizeof(vc_data)))
			goto out_release;

		ret = 0;
	out_release:
		path_release(&nd);
	}
	return ret;
}


void vx_vsi_statfs(struct super_block *sb, struct kstatfs *buf)
{
	struct dl_info *dli;
	__u64 blimit, bfree, bavail;
	__u32 ifree;

	dli = locate_dl_info(sb, vx_current_xid());
	if (!dli)
		return;

	spin_lock(&dli->dl_lock);
	if (dli->dl_inodes_total == (uint32_t)CDLIM_INFINITY)
		goto no_ilim;

	/* reduce max inodes available to limit */
	if (buf->f_files > dli->dl_inodes_total)
		buf->f_files = dli->dl_inodes_total;

	ifree = dli->dl_inodes_total - dli->dl_inodes_used;
	/* reduce free inodes to min */
	if (ifree < buf->f_ffree)
		buf->f_ffree = ifree;

no_ilim:
	if (dli->dl_space_total == (uint64_t)CDLIM_INFINITY)
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

