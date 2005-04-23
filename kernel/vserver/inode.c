/*
 *  linux/kernel/vserver/inode.c
 *
 *  Virtual Server: File System Support
 *
 *  Copyright (C) 2004-2005  Herbert Pötzl
 *
 *  V0.01  separated from vcontext V0.05
 *
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/vs_context.h>
#include <linux/proc_fs.h>
#include <linux/devpts_fs.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/parser.h>
#include <linux/vserver/inode.h>
#include <linux/vserver/inode_cmd.h>
#include <linux/vserver/xid.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


static int __vc_get_iattr(struct inode *in, uint32_t *xid, uint32_t *flags, uint32_t *mask)
{
	struct proc_dir_entry *entry;

	if (!in || !in->i_sb)
		return -ESRCH;

	*flags = IATTR_XID
		| (IS_BARRIER(in) ? IATTR_BARRIER : 0)
		| (IS_IUNLINK(in) ? IATTR_IUNLINK : 0)
		| (IS_IMMUTABLE(in) ? IATTR_IMMUTABLE : 0);
	*mask = IATTR_IUNLINK | IATTR_IMMUTABLE;

	if (S_ISDIR(in->i_mode))
		*mask |= IATTR_BARRIER;

	if (in->i_sb->s_flags & MS_TAGXID) {
		*xid = in->i_xid;
		*mask |= IATTR_XID;
	}

	switch (in->i_sb->s_magic) {
	case PROC_SUPER_MAGIC:
		entry = PROC_I(in)->pde;

		// check for specific inodes ?
		if (entry)
			*mask |= IATTR_FLAGS;
		if (entry)
			*flags |= (entry->vx_flags & IATTR_FLAGS);
		else
			*flags |= (PROC_I(in)->vx_flags & IATTR_FLAGS);
		break;

	case DEVPTS_SUPER_MAGIC:
		*xid = in->i_xid;
		*mask |= IATTR_XID;
		break;

	default:
		break;
	}
	return 0;
}

int vc_get_iattr(uint32_t id, void __user *data)
{
	struct nameidata nd;
	struct vcmd_ctx_iattr_v1 vc_data = { .xid = -1 };
	int ret;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_path_walk_link(vc_data.name, &nd);
	if (!ret) {
		ret = __vc_get_iattr(nd.dentry->d_inode,
			&vc_data.xid, &vc_data.flags, &vc_data.mask);
		path_release(&nd);
	}

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		ret = -EFAULT;
	return ret;
}

static int __vc_set_iattr(struct dentry *de, uint32_t *xid, uint32_t *flags, uint32_t *mask)
{
	struct inode *in = de->d_inode;
	int error = 0, is_proc = 0, has_xid = 0;

	if (!in || !in->i_sb)
		return -ESRCH;

	is_proc = (in->i_sb->s_magic == PROC_SUPER_MAGIC);
	if ((*mask & IATTR_FLAGS) && !is_proc)
		return -EINVAL;

	has_xid = (in->i_sb->s_flags & MS_TAGXID) ||
		(in->i_sb->s_magic == DEVPTS_SUPER_MAGIC);
	if ((*mask & IATTR_XID) && !has_xid)
		return -EINVAL;

	down(&in->i_sem);
	if (*mask & IATTR_XID)
		in->i_xid = *xid;

	if (*mask & IATTR_FLAGS) {
		struct proc_dir_entry *entry = PROC_I(in)->pde;
		unsigned int iflags = PROC_I(in)->vx_flags;

		iflags = (iflags & ~(*mask & IATTR_FLAGS))
			| (*flags & IATTR_FLAGS);
		PROC_I(in)->vx_flags = iflags;
		if (entry)
			entry->vx_flags = iflags;
	}

	if (*mask & (IATTR_BARRIER | IATTR_IUNLINK | IATTR_IMMUTABLE)) {
		struct iattr attr;

		attr.ia_valid = ATTR_ATTR_FLAG;
		attr.ia_attr_flags =
			(IS_IMMUTABLE(in) ? ATTR_FLAG_IMMUTABLE : 0) |
			(IS_IUNLINK(in) ? ATTR_FLAG_IUNLINK : 0) |
			(IS_BARRIER(in) ? ATTR_FLAG_BARRIER : 0);

		if (*mask & IATTR_IMMUTABLE) {
			if (*flags & IATTR_IMMUTABLE)
				attr.ia_attr_flags |= ATTR_FLAG_IMMUTABLE;
			else
				attr.ia_attr_flags &= ~ATTR_FLAG_IMMUTABLE;
		}
		if (*mask & IATTR_IUNLINK) {
			if (*flags & IATTR_IUNLINK)
				attr.ia_attr_flags |= ATTR_FLAG_IUNLINK;
			else
				attr.ia_attr_flags &= ~ATTR_FLAG_IUNLINK;
		}
		if (S_ISDIR(in->i_mode) && (*mask & IATTR_BARRIER)) {
			if (*flags & IATTR_BARRIER)
				attr.ia_attr_flags |= ATTR_FLAG_BARRIER;
			else
				attr.ia_attr_flags &= ~ATTR_FLAG_BARRIER;
		}
		if (in->i_op && in->i_op->setattr)
			error = in->i_op->setattr(de, &attr);
		else {
			error = inode_change_ok(in, &attr);
			if (!error)
				error = inode_setattr(in, &attr);
		}
	}

	mark_inode_dirty(in);
	up(&in->i_sem);
	return 0;
}

int vc_set_iattr(uint32_t id, void __user *data)
{
	struct nameidata nd;
	struct vcmd_ctx_iattr_v1 vc_data;
	int ret;

	if (!capable(CAP_SYS_ADMIN) || !capable(CAP_LINUX_IMMUTABLE))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_path_walk_link(vc_data.name, &nd);
	if (!ret) {
		ret = __vc_set_iattr(nd.dentry,
			&vc_data.xid, &vc_data.flags, &vc_data.mask);
		path_release(&nd);
	}

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		ret = -EFAULT;
	return ret;
}


#ifdef	CONFIG_VSERVER_LEGACY

#define PROC_DYNAMIC_FIRST 0xF0000000UL

int vx_proc_ioctl(struct inode * inode, struct file * filp,
	unsigned int cmd, unsigned long arg)
{
	struct proc_dir_entry *entry;
	int error = 0;
	int flags;

	if (inode->i_ino < PROC_DYNAMIC_FIRST)
		return -ENOTTY;

	entry = PROC_I(inode)->pde;
	if (!entry)
		return -ENOTTY;

	switch(cmd) {
	case FIOC_GETXFLG: {
		/* fixme: if stealth, return -ENOTTY */
		error = -EPERM;
		flags = entry->vx_flags;
		if (capable(CAP_CONTEXT))
			error = put_user(flags, (int *) arg);
		break;
	}
	case FIOC_SETXFLG: {
		/* fixme: if stealth, return -ENOTTY */
		error = -EPERM;
		if (!capable(CAP_CONTEXT))
			break;
		error = -EROFS;
		if (IS_RDONLY(inode))
			break;
		error = -EFAULT;
		if (get_user(flags, (int *) arg))
			break;
		error = 0;
		entry->vx_flags = flags;
		break;
	}
	default:
		return -ENOTTY;
	}
	return error;
}
#endif


int vx_parse_xid(char *string, xid_t *xid, int remove)
{
	static match_table_t tokens = {
		{1, "xid=%u"},
		{0, NULL}
	};
	substring_t args[MAX_OPT_ARGS];
	int token, option = 0;

	if (!string)
		return 0;

	token = match_token(string, tokens, args);
	if (token && xid && !match_int(args, &option))
		*xid = option;

	vxdprintk(VXD_CBIT(xid, 7),
		"vx_parse_xid(»%s«): %d:#%d",
		string, token, option);

	if (token && remove) {
		char *p = strstr(string, "xid=");
		char *q = p;

		if (p) {
			while (*q != '\0' && *q != ',')
				q++;
			while (*q)
				*p++ = *q++;
			while (*p)
				*p++ = '\0';
		}
	}
	return token;
}

void vx_propagate_xid(struct nameidata *nd, struct inode *inode)
{
	xid_t new_xid = 0;
	struct vfsmount *mnt;
	int propagate;

	if (!nd)
		return;
	mnt = nd->mnt;
	if (!mnt)
		return;

	propagate = (mnt->mnt_flags & MNT_XID);
	if (propagate)
		new_xid = mnt->mnt_xid;

	vxdprintk(VXD_CBIT(xid, 7),
		"vx_propagate_xid(%p[#%lu.%d]): %d,%d",
		inode, inode->i_ino, inode->i_xid,
		new_xid, (propagate)?1:0);

	if (propagate)
		inode->i_xid = new_xid;
}

#include <linux/module.h>

EXPORT_SYMBOL_GPL(vx_propagate_xid);

