#ifndef _VS_COWBL_H
#define _VS_COWBL_H

#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/slab.h>

extern struct dentry *cow_break_link(const char *pathname);

static inline int cow_check_and_break(struct path *path)
{
	struct inode *inode = path->dentry->d_inode;
	int error = 0;

	/* do we need this check? */
	if (IS_RDONLY(inode))
		return -EROFS;

	if (IS_COW(inode)) {
		if (IS_COW_LINK(inode)) {
			struct dentry *new_dentry, *old_dentry = path->dentry;
			char *pp, *buf;

			buf = kmalloc(PATH_MAX, GFP_KERNEL);
			if (!buf) {
				return -ENOMEM;
			}
			pp = d_path(path, buf, PATH_MAX);
			new_dentry = cow_break_link(pp);
			kfree(buf);
			if (!IS_ERR(new_dentry)) {
				path->dentry = new_dentry;
				dput(old_dentry);
			} else
				error = PTR_ERR(new_dentry);
		} else {
			inode->i_flags &= ~(S_IXUNLINK | S_IMMUTABLE);
			inode->i_ctime = CURRENT_TIME;
			mark_inode_dirty(inode);
		}
	}
	return error;
}

#else
#warning duplicate inclusion
#endif
