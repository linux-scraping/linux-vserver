/*
 *  linux/kernel/vserver/inode.c
 *
 *  Virtual Server: File System Support
 *
 *  Copyright (C) 2004-2007  Herbert Pötzl
 *
 *  V0.01  separated from vcontext V0.05
 *  V0.02  moved to tag (instead of xid)
 *
 */

#include <linux/tty.h>
#include <linux/proc_fs.h>
#include <linux/devpts_fs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mount.h>
#include <linux/parser.h>
#include <linux/vserver/inode.h>
#include <linux/vserver/inode_cmd.h>
#include <linux/vs_base.h>
#include <linux/vs_tag.h>

#include <asm/uaccess.h>


static int __vc_get_iattr(struct inode *in, uint32_t *tag, uint32_t *flags, uint32_t *mask)
{
	struct proc_dir_entry *entry;

	if (!in || !in->i_sb)
		return -ESRCH;

	*flags = IATTR_TAG
		| (IS_BARRIER(in) ? IATTR_BARRIER : 0)
		| (IS_IUNLINK(in) ? IATTR_IUNLINK : 0)
		| (IS_IMMUTABLE(in) ? IATTR_IMMUTABLE : 0);
	*mask = IATTR_IUNLINK | IATTR_IMMUTABLE;

	if (S_ISDIR(in->i_mode))
		*mask |= IATTR_BARRIER;

	if (IS_TAGGED(in)) {
		*tag = in->i_tag;
		*mask |= IATTR_TAG;
	}

	switch (in->i_sb->s_magic) {
	case PROC_SUPER_MAGIC:
		entry = PROC_I(in)->pde;

		/* check for specific inodes? */
		if (entry)
			*mask |= IATTR_FLAGS;
		if (entry)
			*flags |= (entry->vx_flags & IATTR_FLAGS);
		else
			*flags |= (PROC_I(in)->vx_flags & IATTR_FLAGS);
		break;

	case DEVPTS_SUPER_MAGIC:
		*tag = in->i_tag;
		*mask |= IATTR_TAG;
		break;

	default:
		break;
	}
	return 0;
}

int vc_get_iattr(void __user *data)
{
	struct nameidata nd;
	struct vcmd_ctx_iattr_v1 vc_data = { .tag = -1 };
	int ret;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_path_walk_link(vc_data.name, &nd);
	if (!ret) {
		ret = __vc_get_iattr(nd.dentry->d_inode,
			&vc_data.tag, &vc_data.flags, &vc_data.mask);
		path_release(&nd);
	}
	if (ret)
		return ret;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		ret = -EFAULT;
	return ret;
}

#ifdef	CONFIG_COMPAT

int vc_get_iattr_x32(void __user *data)
{
	struct nameidata nd;
	struct vcmd_ctx_iattr_v1_x32 vc_data = { .tag = -1 };
	int ret;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_path_walk_link(compat_ptr(vc_data.name_ptr), &nd);
	if (!ret) {
		ret = __vc_get_iattr(nd.dentry->d_inode,
			&vc_data.tag, &vc_data.flags, &vc_data.mask);
		path_release(&nd);
	}
	if (ret)
		return ret;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		ret = -EFAULT;
	return ret;
}

#endif	/* CONFIG_COMPAT */


int vc_fget_iattr(uint32_t fd, void __user *data)
{
	struct file *filp;
	struct vcmd_ctx_fiattr_v0 vc_data = { .tag = -1 };
	int ret;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	filp = fget(fd);
	if (!filp || !filp->f_dentry || !filp->f_dentry->d_inode)
		return -EBADF;

	ret = __vc_get_iattr(filp->f_dentry->d_inode,
		&vc_data.tag, &vc_data.flags, &vc_data.mask);

	fput(filp);

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		ret = -EFAULT;
	return ret;
}


static int __vc_set_iattr(struct dentry *de, uint32_t *tag, uint32_t *flags, uint32_t *mask)
{
	struct inode *in = de->d_inode;
	int error = 0, is_proc = 0, has_tag = 0;
	struct iattr attr = { 0 };

	if (!in || !in->i_sb)
		return -ESRCH;

	is_proc = (in->i_sb->s_magic == PROC_SUPER_MAGIC);
	if ((*mask & IATTR_FLAGS) && !is_proc)
		return -EINVAL;

	has_tag = IS_TAGGED(in) ||
		(in->i_sb->s_magic == DEVPTS_SUPER_MAGIC);
	if ((*mask & IATTR_TAG) && !has_tag)
		return -EINVAL;

	mutex_lock(&in->i_mutex);
	if (*mask & IATTR_TAG) {
		attr.ia_tag = *tag;
		attr.ia_valid |= ATTR_TAG;
	}

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
		if (*mask & IATTR_IMMUTABLE) {
			if (*flags & IATTR_IMMUTABLE)
				in->i_flags |= S_IMMUTABLE;
			else
				in->i_flags &= ~S_IMMUTABLE;
		}
		if (*mask & IATTR_IUNLINK) {
			if (*flags & IATTR_IUNLINK)
				in->i_flags |= S_IUNLINK;
			else
				in->i_flags &= ~S_IUNLINK;
		}
		if (S_ISDIR(in->i_mode) && (*mask & IATTR_BARRIER)) {
			if (*flags & IATTR_BARRIER)
				in->i_flags |= S_BARRIER;
			else
				in->i_flags &= ~S_BARRIER;
		}
		if (in->i_op && in->i_op->sync_flags) {
			error = in->i_op->sync_flags(in);
			if (error)
				goto out;
		}
	}

	if (attr.ia_valid) {
		if (in->i_op && in->i_op->setattr)
			error = in->i_op->setattr(de, &attr);
		else {
			error = inode_change_ok(in, &attr);
			if (!error)
				error = inode_setattr(in, &attr);
		}
	}

out:
	mutex_unlock(&in->i_mutex);
	return error;
}

int vc_set_iattr(void __user *data)
{
	struct nameidata nd;
	struct vcmd_ctx_iattr_v1 vc_data;
	int ret;

	if (!capable(CAP_LINUX_IMMUTABLE))
		return -EPERM;
	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_path_walk_link(vc_data.name, &nd);
	if (!ret) {
		ret = __vc_set_iattr(nd.dentry,
			&vc_data.tag, &vc_data.flags, &vc_data.mask);
		path_release(&nd);
	}

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		ret = -EFAULT;
	return ret;
}

#ifdef	CONFIG_COMPAT

int vc_set_iattr_x32(void __user *data)
{
	struct nameidata nd;
	struct vcmd_ctx_iattr_v1_x32 vc_data;
	int ret;

	if (!capable(CAP_LINUX_IMMUTABLE))
		return -EPERM;
	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_path_walk_link(compat_ptr(vc_data.name_ptr), &nd);
	if (!ret) {
		ret = __vc_set_iattr(nd.dentry,
			&vc_data.tag, &vc_data.flags, &vc_data.mask);
		path_release(&nd);
	}

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		ret = -EFAULT;
	return ret;
}

#endif	/* CONFIG_COMPAT */

int vc_fset_iattr(uint32_t fd, void __user *data)
{
	struct file *filp;
	struct vcmd_ctx_fiattr_v0 vc_data;
	int ret;

	if (!capable(CAP_LINUX_IMMUTABLE))
		return -EPERM;
	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	filp = fget(fd);
	if (!filp || !filp->f_dentry || !filp->f_dentry->d_inode)
		return -EBADF;

	ret = __vc_set_iattr(filp->f_dentry, &vc_data.tag,
		&vc_data.flags, &vc_data.mask);

	fput(filp);

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return ret;
}


enum { Opt_notagcheck, Opt_tag, Opt_notag, Opt_tagid, Opt_err };

static match_table_t tokens = {
	{Opt_notagcheck, "notagcheck"},
#ifdef	CONFIG_PROPAGATE
	{Opt_notag, "notag"},
	{Opt_tag, "tag"},
	{Opt_tagid, "tagid=%u"},
#endif
	{Opt_err, NULL}
};


static void __dx_parse_remove(char *string, char *opt)
{
	char *p = strstr(string, opt);
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

static inline
int __dx_parse_tag(char *string, tag_t *tag, int remove)
{
	substring_t args[MAX_OPT_ARGS];
	int token, option = 0;

	if (!string)
		return 0;

	token = match_token(string, tokens, args);

	vxdprintk(VXD_CBIT(tag, 7),
		"dx_parse_tag(»%s«): %d:#%d",
		string, token, option);

	switch (token) {
	case Opt_tag:
		if (tag)
			*tag = 0;
		if (remove)
			__dx_parse_remove(string, "tag");
		return MNT_TAGID;
	case Opt_notag:
		if (remove)
			__dx_parse_remove(string, "notag");
		return MNT_NOTAG;
	case Opt_notagcheck:
		if (remove)
			__dx_parse_remove(string, "notagcheck");
		return MNT_NOTAGCHECK;
	case Opt_tagid:
		if (tag && !match_int(args, &option))
			*tag = option;
		if (remove)
			__dx_parse_remove(string, "tagid");
		return MNT_TAGID;
	}
	return 0;
}

int dx_parse_tag(char *string, tag_t *tag, int remove)
{
	int retval, flags = 0;

	while ((retval = __dx_parse_tag(string, tag, remove)))
		flags |= retval;
	return flags;
}

#ifdef	CONFIG_PROPAGATE

void __dx_propagate_tag(struct nameidata *nd, struct inode *inode)
{
	tag_t new_tag = 0;
	struct vfsmount *mnt;
	int propagate;

	if (!nd)
		return;
	mnt = nd->mnt;
	if (!mnt)
		return;

	propagate = (mnt->mnt_flags & MNT_TAGID);
	if (propagate)
		new_tag = mnt->mnt_tag;

	vxdprintk(VXD_CBIT(tag, 7),
		"dx_propagate_tag(%p[#%lu.%d]): %d,%d",
		inode, inode->i_ino, inode->i_tag,
		new_tag, (propagate) ? 1 : 0);

	if (propagate)
		inode->i_tag = new_tag;
}

#include <linux/module.h>

EXPORT_SYMBOL_GPL(__dx_propagate_tag);

#endif	/* CONFIG_PROPAGATE */

