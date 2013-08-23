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
#include <linux/namei.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <linux/vserver/inode.h>
#include <linux/vserver/inode_cmd.h>
#include <linux/vs_base.h>
#include <linux/vs_tag.h>

#include <asm/uaccess.h>
#include <../../fs/proc/internal.h>


static int __vc_get_iattr(struct inode *in, uint32_t *tag, uint32_t *flags, uint32_t *mask)
{
	struct proc_dir_entry *entry;

	if (!in || !in->i_sb)
		return -ESRCH;

	*flags = IATTR_TAG
		| (IS_IMMUTABLE(in) ? IATTR_IMMUTABLE : 0)
		| (IS_IXUNLINK(in) ? IATTR_IXUNLINK : 0)
		| (IS_BARRIER(in) ? IATTR_BARRIER : 0)
		| (IS_COW(in) ? IATTR_COW : 0);
	*mask = IATTR_IXUNLINK | IATTR_IMMUTABLE | IATTR_COW;

	if (S_ISDIR(in->i_mode))
		*mask |= IATTR_BARRIER;

	if (IS_TAGGED(in)) {
		*tag = i_tag_read(in);
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
		*tag = i_tag_read(in);
		*mask |= IATTR_TAG;
		break;

	default:
		break;
	}
	return 0;
}

int vc_get_iattr(void __user *data)
{
	struct path path;
	struct vcmd_ctx_iattr_v1 vc_data = { .tag = -1 };
	int ret;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_lpath(vc_data.name, &path);
	if (!ret) {
		ret = __vc_get_iattr(path.dentry->d_inode,
			&vc_data.tag, &vc_data.flags, &vc_data.mask);
		path_put(&path);
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
	struct path path;
	struct vcmd_ctx_iattr_v1_x32 vc_data = { .tag = -1 };
	int ret;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_lpath(compat_ptr(vc_data.name_ptr), &path);
	if (!ret) {
		ret = __vc_get_iattr(path.dentry->d_inode,
			&vc_data.tag, &vc_data.flags, &vc_data.mask);
		path_put(&path);
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
		attr.ia_tag = make_ktag(&init_user_ns, *tag);
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

	if (*mask & (IATTR_IMMUTABLE | IATTR_IXUNLINK |
		IATTR_BARRIER | IATTR_COW)) {
		int iflags = in->i_flags;
		int vflags = in->i_vflags;

		if (*mask & IATTR_IMMUTABLE) {
			if (*flags & IATTR_IMMUTABLE)
				iflags |= S_IMMUTABLE;
			else
				iflags &= ~S_IMMUTABLE;
		}
		if (*mask & IATTR_IXUNLINK) {
			if (*flags & IATTR_IXUNLINK)
				iflags |= S_IXUNLINK;
			else
				iflags &= ~S_IXUNLINK;
		}
		if (S_ISDIR(in->i_mode) && (*mask & IATTR_BARRIER)) {
			if (*flags & IATTR_BARRIER)
				vflags |= V_BARRIER;
			else
				vflags &= ~V_BARRIER;
		}
		if (S_ISREG(in->i_mode) && (*mask & IATTR_COW)) {
			if (*flags & IATTR_COW)
				vflags |= V_COW;
			else
				vflags &= ~V_COW;
		}
		if (in->i_op && in->i_op->sync_flags) {
			error = in->i_op->sync_flags(in, iflags, vflags);
			if (error)
				goto out;
		}
	}

	if (attr.ia_valid) {
		if (in->i_op && in->i_op->setattr)
			error = in->i_op->setattr(de, &attr);
		else {
			error = inode_change_ok(in, &attr);
			if (!error) {
				setattr_copy(in, &attr);
				mark_inode_dirty(in);
			}
		}
	}

out:
	mutex_unlock(&in->i_mutex);
	return error;
}

int vc_set_iattr(void __user *data)
{
	struct path path;
	struct vcmd_ctx_iattr_v1 vc_data;
	int ret;

	if (!capable(CAP_LINUX_IMMUTABLE))
		return -EPERM;
	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_lpath(vc_data.name, &path);
	if (!ret) {
		ret = __vc_set_iattr(path.dentry,
			&vc_data.tag, &vc_data.flags, &vc_data.mask);
		path_put(&path);
	}

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		ret = -EFAULT;
	return ret;
}

#ifdef	CONFIG_COMPAT

int vc_set_iattr_x32(void __user *data)
{
	struct path path;
	struct vcmd_ctx_iattr_v1_x32 vc_data;
	int ret;

	if (!capable(CAP_LINUX_IMMUTABLE))
		return -EPERM;
	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = user_lpath(compat_ptr(vc_data.name_ptr), &path);
	if (!ret) {
		ret = __vc_set_iattr(path.dentry,
			&vc_data.tag, &vc_data.flags, &vc_data.mask);
		path_put(&path);
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

int dx_parse_tag(char *string, vtag_t *tag, int remove, int *mnt_flags,
		 unsigned long *flags)
{
	int set = 0;
	substring_t args[MAX_OPT_ARGS];
	int token;
	char *s, *p, *opts;
#if defined(CONFIG_PROPAGATE) || defined(CONFIG_VSERVER_DEBUG)
	int option = 0;
#endif

	if (!string)
		return 0;
	s = kstrdup(string, GFP_KERNEL | GFP_ATOMIC);
	if (!s)
		return 0;

	opts = s;
	while ((p = strsep(&opts, ",")) != NULL) {
		token = match_token(p, tokens, args);

		switch (token) {
#ifdef CONFIG_PROPAGATE
		case Opt_tag:
			if (tag)
				*tag = 0;
			if (remove)
				__dx_parse_remove(s, "tag");
			*mnt_flags |= MNT_TAGID;
			set |= MNT_TAGID;
			break;
		case Opt_notag:
			if (remove)
				__dx_parse_remove(s, "notag");
			*mnt_flags |= MNT_NOTAG;
			set |= MNT_NOTAG;
			break;
		case Opt_tagid:
			if (tag && !match_int(args, &option))
				*tag = option;
			if (remove)
				__dx_parse_remove(s, "tagid");
			*mnt_flags |= MNT_TAGID;
			set |= MNT_TAGID;
			break;
#endif	/* CONFIG_PROPAGATE */
		case Opt_notagcheck:
			if (remove)
				__dx_parse_remove(s, "notagcheck");
			*flags |= MS_NOTAGCHECK;
			set |= MS_NOTAGCHECK;
			break;
		}
		vxdprintk(VXD_CBIT(tag, 7),
			"dx_parse_tag(" VS_Q("%s") "): %d:#%d",
			p, token, option);
	}
	if (set)
		strcpy(string, s);
	kfree(s);
	return set;
}

#ifdef	CONFIG_PROPAGATE

void __dx_propagate_tag(struct nameidata *nd, struct inode *inode)
{
	vtag_t new_tag = 0;
	struct vfsmount *mnt;
	int propagate;

	if (!nd)
		return;
	mnt = nd->path.mnt;
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
		i_tag_write(inode, new_tag);
}

#include <linux/module.h>

EXPORT_SYMBOL_GPL(__dx_propagate_tag);

#endif	/* CONFIG_PROPAGATE */

