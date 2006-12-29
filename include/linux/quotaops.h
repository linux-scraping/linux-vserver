/*
 * Definitions for diskquota-operations. When diskquota is configured these
 * macros expand to the right source-code.
 *
 * Author:  Marco van Wieringen <mvw@planets.elm.net>
 *
 * Version: $Id: quotaops.h,v 1.2 1998/01/15 16:22:26 ecd Exp $
 *
 */
#ifndef _LINUX_QUOTAOPS_
#define _LINUX_QUOTAOPS_

#include <linux/smp_lock.h>

#include <linux/fs.h>

#if defined(CONFIG_QUOTA)

/*
 * declaration of quota_function calls in kernel.
 */
extern void sync_dquots(struct dqhash *hash, int type);

extern int dquot_initialize(struct inode *inode, int type);
extern int dquot_drop(struct inode *inode);

extern int dquot_alloc_space(struct inode *inode, qsize_t number, int prealloc);
extern int dquot_alloc_inode(const struct inode *inode, unsigned long number);

extern int dquot_free_space(struct inode *inode, qsize_t number);
extern int dquot_free_inode(const struct inode *inode, unsigned long number);

extern int dquot_transfer(struct inode *inode, struct iattr *iattr);
extern int dquot_commit(struct dquot *dquot);
extern int dquot_acquire(struct dquot *dquot);
extern int dquot_release(struct dquot *dquot);
extern int dquot_commit_info(struct dqhash *hash, int type);
extern int dquot_mark_dquot_dirty(struct dquot *dquot);

int remove_inode_dquot_ref(struct inode *inode, int type,
			   struct list_head *tofree_head);

extern int vfs_quota_on(struct dqhash *hash, int type, int format_id, char *path);
extern int vfs_quota_on_mount(struct dqhash *hash, char *qf_name,
		int format_id, int type);
extern int vfs_quota_off(struct dqhash *hash, int type);
#define vfs_quota_off_mount(dqh, type) vfs_quota_off(dqh, type)
extern int vfs_quota_sync(struct dqhash *hash, int type);
extern int vfs_get_dqinfo(struct dqhash *hash, int type, struct if_dqinfo *ii);
extern int vfs_set_dqinfo(struct dqhash *hash, int type, struct if_dqinfo *ii);
extern int vfs_get_dqblk(struct dqhash *hash, int type, qid_t id, struct if_dqblk *di);
extern int vfs_set_dqblk(struct dqhash *hash, int type, qid_t id, struct if_dqblk *di);

/*
 * Operations supported for diskquotas.
 */
extern struct dquot_operations dquot_operations;
extern struct quotactl_ops vfs_quotactl_ops;

#define sb_dquot_ops (&dquot_operations)
#define sb_quotactl_ops (&vfs_quotactl_ops)

/* It is better to call this function outside of any transaction as it might
 * need a lot of space in journal for dquot structure allocation. */
static __inline__ void DQUOT_INIT(struct inode *inode)
{
	if (!dqhash_valid(inode->i_dqh))
		return;
	BUG_ON(!inode->i_dqh);
	// printk("DQUOT_INIT(%p,%p,%d)\n", inode, inode->i_dqh, dqh_any_quota_enabled(inode->i_dqh));
	if (dqh_any_quota_enabled(inode->i_dqh) && !IS_NOQUOTA(inode))
		inode->i_dqh->dqh_qop->initialize(inode, -1);
}

/* The same as with DQUOT_INIT */
static __inline__ void DQUOT_DROP(struct inode *inode)
{
	/* Here we can get arbitrary inode from clear_inode() so we have
	 * to be careful. OTOH we don't need locking as quota operations
	 * are allowed to change only at mount time */
	if (!IS_NOQUOTA(inode) && inode->i_dqh && inode->i_dqh->dqh_qop
	    && inode->i_dqh->dqh_qop->drop) {
		int cnt;
		/* Test before calling to rule out calls from proc and such
                 * where we are not allowed to block. Note that this is
		 * actually reliable test even without the lock - the caller
		 * must assure that nobody can come after the DQUOT_DROP and
		 * add quota pointers back anyway */
		for (cnt = 0; cnt < MAXQUOTAS; cnt++)
			if (inode->i_dquot[cnt] != NODQUOT)
				break;
		if (cnt < MAXQUOTAS)
			inode->i_dqh->dqh_qop->drop(inode);
	}
}

/* The following allocation/freeing/transfer functions *must* be called inside
 * a transaction (deadlocks possible otherwise) */
static __inline__ int DQUOT_PREALLOC_SPACE_NODIRTY(struct inode *inode, qsize_t nr)
{
	if (dqh_any_quota_enabled(inode->i_dqh)) {
		/* Used space is updated in alloc_space() */
		if (inode->i_dqh->dqh_qop->alloc_space(inode, nr, 1) == NO_QUOTA)
			return 1;
	}
	else
		inode_add_bytes(inode, nr);
	return 0;
}

static __inline__ int DQUOT_PREALLOC_SPACE(struct inode *inode, qsize_t nr)
{
	int ret;
        if (!(ret =  DQUOT_PREALLOC_SPACE_NODIRTY(inode, nr)))
		mark_inode_dirty(inode);
	return ret;
}

static __inline__ int DQUOT_ALLOC_SPACE_NODIRTY(struct inode *inode, qsize_t nr)
{
	if (dqh_any_quota_enabled(inode->i_dqh)) {
		/* Used space is updated in alloc_space() */
		if (inode->i_dqh->dqh_qop->alloc_space(inode, nr, 0) == NO_QUOTA)
			return 1;
	}
	else
		inode_add_bytes(inode, nr);
	return 0;
}

static __inline__ int DQUOT_ALLOC_SPACE(struct inode *inode, qsize_t nr)
{
	int ret;
	if (!(ret = DQUOT_ALLOC_SPACE_NODIRTY(inode, nr)))
		mark_inode_dirty(inode);
	return ret;
}

static __inline__ int DQUOT_ALLOC_INODE(struct inode *inode)
{
	if (dqh_any_quota_enabled(inode->i_dqh)) {
		DQUOT_INIT(inode);
		if (inode->i_dqh->dqh_qop->alloc_inode(inode, 1) == NO_QUOTA)
			return 1;
	}
	return 0;
}

static __inline__ void DQUOT_FREE_SPACE_NODIRTY(struct inode *inode, qsize_t nr)
{
	if (dqh_any_quota_enabled(inode->i_dqh))
		inode->i_dqh->dqh_qop->free_space(inode, nr);
	else
		inode_sub_bytes(inode, nr);
}

static __inline__ void DQUOT_FREE_SPACE(struct inode *inode, qsize_t nr)
{
	DQUOT_FREE_SPACE_NODIRTY(inode, nr);
	mark_inode_dirty(inode);
}

static __inline__ void DQUOT_FREE_INODE(struct inode *inode)
{
	if (dqh_any_quota_enabled(inode->i_dqh))
		inode->i_dqh->dqh_qop->free_inode(inode, 1);
}

static __inline__ int DQUOT_TRANSFER(struct inode *inode, struct iattr *iattr)
{
	if (dqh_any_quota_enabled(inode->i_dqh) && !IS_NOQUOTA(inode)) {
		DQUOT_INIT(inode);
		if (inode->i_dqh->dqh_qop->transfer(inode, iattr) == NO_QUOTA)
			return 1;
	}
	return 0;
}

/* The following two functions cannot be called inside a transaction */
#define DQUOT_SYNC(hash)	sync_dquots(hash, -1)

static __inline__ int DQUOT_OFF(struct dqhash *hash)
{
	int ret = -ENOSYS;

	if (dqh_any_quota_enabled(hash) && hash->dqh_qcop &&
		hash->dqh_qcop->quota_off)
		ret = hash->dqh_qcop->quota_off(hash, -1);
	return ret;
}

#else

/*
 * NO-OP when quota not configured.
 */
#define sb_dquot_ops				(NULL)
#define sb_quotactl_ops				(NULL)
#define DQUOT_INIT(inode)			do { } while(0)
#define DQUOT_DROP(inode)			do { } while(0)
#define DQUOT_ALLOC_INODE(inode)		(0)
#define DQUOT_FREE_INODE(inode)			do { } while(0)
#define DQUOT_SYNC(hash)			do { } while(0)
#define DQUOT_OFF(hash)				do { } while(0)
#define DQUOT_TRANSFER(inode, iattr)		(0)
static inline int DQUOT_PREALLOC_SPACE_NODIRTY(struct inode *inode, qsize_t nr)
{
	inode_add_bytes(inode, nr);
	return 0;
}

static inline int DQUOT_PREALLOC_SPACE(struct inode *inode, qsize_t nr)
{
	DQUOT_PREALLOC_SPACE_NODIRTY(inode, nr);
	mark_inode_dirty(inode);
	return 0;
}

static inline int DQUOT_ALLOC_SPACE_NODIRTY(struct inode *inode, qsize_t nr)
{
	inode_add_bytes(inode, nr);
	return 0;
}

static inline int DQUOT_ALLOC_SPACE(struct inode *inode, qsize_t nr)
{
	DQUOT_ALLOC_SPACE_NODIRTY(inode, nr);
	mark_inode_dirty(inode);
	return 0;
}

static inline void DQUOT_FREE_SPACE_NODIRTY(struct inode *inode, qsize_t nr)
{
	inode_sub_bytes(inode, nr);
}

static inline void DQUOT_FREE_SPACE(struct inode *inode, qsize_t nr)
{
	DQUOT_FREE_SPACE_NODIRTY(inode, nr);
	mark_inode_dirty(inode);
}	

#endif /* CONFIG_QUOTA */

#define DQUOT_PREALLOC_BLOCK_NODIRTY(inode, nr)	DQUOT_PREALLOC_SPACE_NODIRTY(inode, ((qsize_t)(nr)) << (inode)->i_sb->s_blocksize_bits)
#define DQUOT_PREALLOC_BLOCK(inode, nr)	DQUOT_PREALLOC_SPACE(inode, ((qsize_t)(nr)) << (inode)->i_sb->s_blocksize_bits)
#define DQUOT_ALLOC_BLOCK_NODIRTY(inode, nr) DQUOT_ALLOC_SPACE_NODIRTY(inode, ((qsize_t)(nr)) << (inode)->i_sb->s_blocksize_bits)
#define DQUOT_ALLOC_BLOCK(inode, nr) DQUOT_ALLOC_SPACE(inode, ((qsize_t)(nr)) << (inode)->i_sb->s_blocksize_bits)
#define DQUOT_FREE_BLOCK_NODIRTY(inode, nr) DQUOT_FREE_SPACE_NODIRTY(inode, ((qsize_t)(nr)) << (inode)->i_sb->s_blocksize_bits)
#define DQUOT_FREE_BLOCK(inode, nr) DQUOT_FREE_SPACE(inode, ((qsize_t)(nr)) << (inode)->i_sb->s_blocksize_bits)

#endif /* _LINUX_QUOTAOPS_ */
