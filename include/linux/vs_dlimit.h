#ifndef _VX_VS_DLIMIT_H
#define _VX_VS_DLIMIT_H

#include "vserver/dlimit.h"
#include "vserver/debug.h"


#define get_dl_info(i)	__get_dl_info(i,__FILE__,__LINE__)

static inline struct dl_info *__get_dl_info(struct dl_info *dli,
	const char *_file, int _line)
{
	if (!dli)
		return NULL;
	vxlprintk(VXD_CBIT(dlim, 4), "get_dl_info(%p[#%d.%d])",
		dli, dli?dli->dl_xid:0, dli?atomic_read(&dli->dl_usecnt):0,
		_file, _line);
	atomic_inc(&dli->dl_usecnt);
	return dli;
}


#define free_dl_info(i) \
	call_rcu(&i->dl_rcu, rcu_free_dl_info);

#define put_dl_info(i)	__put_dl_info(i,__FILE__,__LINE__)

static inline void __put_dl_info(struct dl_info *dli,
	const char *_file, int _line)
{
	if (!dli)
		return;
	vxlprintk(VXD_CBIT(dlim, 4), "put_dl_info(%p[#%d.%d])",
		dli, dli?dli->dl_xid:0, dli?atomic_read(&dli->dl_usecnt):0,
		_file, _line);
	if (atomic_dec_and_test(&dli->dl_usecnt))
		free_dl_info(dli);
}


#define __dlimit_char(d)	((d)?'*':' ')

static inline int __dl_alloc_space(struct super_block *sb,
	xid_t xid, dlsize_t nr, const char *file, int line)
{
	struct dl_info *dli = NULL;
	int ret = 0;

	if (nr == 0)
		goto out;
	dli = locate_dl_info(sb, xid);
	if (!dli)
		goto out;

	spin_lock(&dli->dl_lock);
	ret = (dli->dl_space_used + nr > dli->dl_space_total);
	if (!ret)
		dli->dl_space_used += nr;
	spin_unlock(&dli->dl_lock);
	put_dl_info(dli);
out:
	vxlprintk(VXD_CBIT(dlim, 1),
		"ALLOC (%p,#%d)%c %lld bytes (%d)",
		sb, xid, __dlimit_char(dli), (long long)nr,
		ret, file, line);
	return ret;
}

static inline void __dl_free_space(struct super_block *sb,
	xid_t xid, dlsize_t nr, const char *_file, int _line)
{
	struct dl_info *dli = NULL;

	if (nr == 0)
		goto out;
	dli = locate_dl_info(sb, xid);
	if (!dli)
		goto out;

	spin_lock(&dli->dl_lock);
	if (dli->dl_space_used > nr)
		dli->dl_space_used -= nr;
	else
		dli->dl_space_used = 0;
	spin_unlock(&dli->dl_lock);
	put_dl_info(dli);
out:
	vxlprintk(VXD_CBIT(dlim, 1),
		"FREE  (%p,#%d)%c %lld bytes",
		sb, xid, __dlimit_char(dli), (long long)nr,
		_file, _line);
}

static inline int __dl_alloc_inode(struct super_block *sb,
	xid_t xid, const char *_file, int _line)
{
	struct dl_info *dli;
	int ret = 0;

	dli = locate_dl_info(sb, xid);
	if (!dli)
		goto out;

	spin_lock(&dli->dl_lock);
	ret = (dli->dl_inodes_used >= dli->dl_inodes_total);
	if (!ret)
		dli->dl_inodes_used++;
#if 0
	else
		vxwprintk("DLIMIT hit (%p,#%d), inode %d>=%d @ %s:%d",
			sb, xid,
			dli->dl_inodes_used, dli->dl_inodes_total,
			file, line);
#endif
	spin_unlock(&dli->dl_lock);
	put_dl_info(dli);
out:
	vxlprintk(VXD_CBIT(dlim, 0),
		"ALLOC (%p,#%d)%c inode (%d)",
		sb, xid, __dlimit_char(dli), ret, _file, _line);
	return ret;
}

static inline void __dl_free_inode(struct super_block *sb,
	xid_t xid, const char *_file, int _line)
{
	struct dl_info *dli;

	dli = locate_dl_info(sb, xid);
	if (!dli)
		goto out;

	spin_lock(&dli->dl_lock);
	if (dli->dl_inodes_used > 1)
		dli->dl_inodes_used--;
	else
		dli->dl_inodes_used = 0;
	spin_unlock(&dli->dl_lock);
	put_dl_info(dli);
out:
	vxlprintk(VXD_CBIT(dlim, 0),
		"FREE  (%p,#%d)%c inode",
		sb, xid, __dlimit_char(dli), _file, _line);
}

static inline void __dl_adjust_block(struct super_block *sb, xid_t xid,
	unsigned int *free_blocks, unsigned int *root_blocks,
	const char *_file, int _line)
{
	struct dl_info *dli;
	uint64_t broot, bfree;

	dli = locate_dl_info(sb, xid);
	if (!dli)
		return;

	spin_lock(&dli->dl_lock);
	broot = (dli->dl_space_total -
		(dli->dl_space_total >> 10) * dli->dl_nrlmult)
		>> sb->s_blocksize_bits;
	bfree = (dli->dl_space_total - dli->dl_space_used)
			>> sb->s_blocksize_bits;
	spin_unlock(&dli->dl_lock);

	vxlprintk(VXD_CBIT(dlim, 2),
		"ADJUST: %lld,%lld on %d,%d [mult=%d]",
		(long long)bfree, (long long)broot,
		*free_blocks, *root_blocks, dli->dl_nrlmult,
		_file, _line);
	if (free_blocks) {
		if (*free_blocks > bfree)
			*free_blocks = bfree;
	}
	if (root_blocks) {
		if (*root_blocks > broot)
			*root_blocks = broot;
	}
	put_dl_info(dli);
}

#define DLIMIT_ALLOC_SPACE(in, bytes) \
	__dl_alloc_space((in)->i_sb, (in)->i_xid, (dlsize_t)(bytes), \
		__FILE__, __LINE__ )

#define DLIMIT_FREE_SPACE(in, bytes) \
	__dl_free_space((in)->i_sb, (in)->i_xid, (dlsize_t)(bytes), \
		__FILE__, __LINE__ )

#define DLIMIT_ALLOC_BLOCK(in, nr) \
	__dl_alloc_space((in)->i_sb, (in)->i_xid, \
		((dlsize_t)(nr)) << (in)->i_sb->s_blocksize_bits, \
		__FILE__, __LINE__ )

#define DLIMIT_FREE_BLOCK(in, nr) \
	__dl_free_space((in)->i_sb, (in)->i_xid, \
		((dlsize_t)(nr)) << (in)->i_sb->s_blocksize_bits, \
		__FILE__, __LINE__ )


#define DLIMIT_ALLOC_INODE(in) \
	__dl_alloc_inode((in)->i_sb, (in)->i_xid, __FILE__, __LINE__ )

#define DLIMIT_FREE_INODE(in) \
	__dl_free_inode((in)->i_sb, (in)->i_xid, __FILE__, __LINE__ )


#define DLIMIT_ADJUST_BLOCK(sb, xid, fb, rb) \
	__dl_adjust_block(sb, xid, fb, rb, __FILE__, __LINE__ )


#else
#warning duplicate inclusion
#endif
