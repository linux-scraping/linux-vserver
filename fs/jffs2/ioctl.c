/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright © 2001-2007 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@infradead.org>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 */

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/mount.h>
#include <linux/jffs2.h>
#include <linux/uaccess.h>
#include <linux/vs_base.h>
#include "jffs2_fs_sb.h"
#include "jffs2_fs_i.h"
#include "acl.h"
#include "os-linux.h"

extern void jffs2_set_inode_flags(struct inode *);

int jffs2_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct jffs2_inode_info *j = JFFS2_INODE_INFO(inode);
	unsigned int flags, oldflags, newflags;

	switch (cmd) {
	case JFFS2_IOC_GETFLAGS:
		flags = j->flags & JFFS2_USER_VISIBLE;
		return put_user(flags, (int __user *) arg);

	case JFFS2_IOC_SETFLAGS:
		if (IS_RDONLY(inode) ||
			(filp && MNT_IS_RDONLY(filp->f_vfsmnt)))
			return -EROFS;

		if ((current->fsuid != inode->i_uid) && !capable(CAP_FOWNER))
			return -EACCES;

		if (get_user(flags, (int __user *) arg))
			return -EFAULT;

		oldflags = j->flags;
		newflags = flags & JFFS2_USER_MODIFIABLE;
		newflags |= oldflags & ~JFFS2_USER_MODIFIABLE;

		/*
		 * The IMMUTABLE flags can only be changed by
		 * the relevant capability.
		 */
		if (((oldflags ^ newflags) &
			(JFFS2_INO_FLAG_IMMUTABLE | JFFS2_INO_FLAG_IUNLINK)) ||
			(oldflags & JFFS2_INO_FLAG_IMMUTABLE)) {
			if (!capable(CAP_LINUX_IMMUTABLE))
				return -EPERM;
		}

		if (oldflags ^ newflags) {
			j->flags = newflags;
			inode->i_ctime = CURRENT_TIME;
			/* strange requirement, see jffs2_dirty_inode() */
			inode->i_state |= I_DIRTY_DATASYNC;
			mark_inode_dirty(inode);
			jffs2_set_inode_flags(inode);
		}
		return 0;

	default:
		return -ENOTTY;
	}
}

