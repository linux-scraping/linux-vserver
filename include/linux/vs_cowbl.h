#ifndef _VX_VS_COWBL_H
#define _VX_VS_COWBL_H

#include <linux/fs.h>
#include <linux/dcache.h>

extern struct dentry *cow_break_link(const char *pathname);

static inline int cow_break_link_vfsmount(struct dentry **dentry, struct vfsmount *mnt)
{
	struct inode *inode = (*dentry)->d_inode;
	int error = 0;
	if (IS_COW(inode)) {
		if (IS_COW_LINK(inode)) {
			struct dentry *new_dentry, *old_dentry = *dentry;
			char *path, *buf;

			buf = kmalloc(PATH_MAX, GFP_KERNEL);
			if (!buf) {
				return -ENOMEM;
			}
			path = d_path(*dentry, mnt, buf, PATH_MAX);
			new_dentry = cow_break_link(path);
			kfree(buf);
			if (!IS_ERR(new_dentry)) {
				*dentry = new_dentry;
				dput(old_dentry);
			} else
				error = PTR_ERR(new_dentry);
		} else {
			inode->i_flags &= ~(S_IUNLINK|S_IMMUTABLE);
			mark_inode_dirty(inode);
		}
	}
	return error;
}

#else
#warning duplicate inclusion
#endif
