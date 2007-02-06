#ifndef _VS_COWBL_H
#define _VS_COWBL_H

#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/namei.h>

extern struct dentry *cow_break_link(const char *pathname);

static inline int cow_check_and_break(struct nameidata *nd)
{
	struct inode *inode = nd->dentry->d_inode;
	int error = 0;
	if (IS_RDONLY(inode) || MNT_IS_RDONLY(nd->mnt))
		return -EROFS;
	if (IS_COW(inode)) {
		if (IS_COW_LINK(inode)) {
			struct dentry *new_dentry, *old_dentry = nd->dentry;
			char *path, *buf;

			buf = kmalloc(PATH_MAX, GFP_KERNEL);
			if (!buf) {
				return -ENOMEM;
			}
			path = d_path(nd->dentry, nd->mnt, buf, PATH_MAX);
			new_dentry = cow_break_link(path);
			kfree(buf);
			if (!IS_ERR(new_dentry)) {
				nd->dentry = new_dentry;
				dput(old_dentry);
			} else
				error = PTR_ERR(new_dentry);
		} else {
			inode->i_flags &= ~(S_IUNLINK|S_IMMUTABLE);
			inode->i_ctime = CURRENT_TIME;
			mark_inode_dirty(inode);
		}
	}
	return error;
}

#else
#warning duplicate inclusion
#endif
