/*
 *  linux/kernel/vserver/namespace.c
 *
 *  Virtual Server: Context Namespace Support
 *
 *  Copyright (C) 2003-2006  Herbert Pötzl
 *
 *  V0.01  broken out from context.c 0.07
 *  V0.02  added task locking for namespace
 *  V0.03  broken out vx_enter_namespace
 *
 */

#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/vs_context.h>
#include <linux/vserver/namespace.h>
#include <linux/vserver/namespace_cmd.h>
#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/fs.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


/* namespace functions */

#include <linux/namespace.h>

int vx_enter_namespace(struct vx_info *vxi)
{
	struct fs_struct *old_fs, *fs;
	struct namespace *old_ns;

	if (vx_info_flags(vxi, VXF_INFO_LOCK, 0))
		return -EACCES;
	if (!vxi->vx_namespace)
		return -EINVAL;

	fs = copy_fs_struct(vxi->vx_fs);
	if (!fs)
		return -ENOMEM;

	task_lock(current);
	old_ns = current->namespace;
	old_fs = current->fs;
	get_namespace(vxi->vx_namespace);
	current->namespace = vxi->vx_namespace;
	current->fs = fs;
	task_unlock(current);

	put_namespace(old_ns);
	put_fs_struct(old_fs);
	return 0;
}

int vx_set_namespace(struct vx_info *vxi, struct namespace *ns, struct fs_struct *fs)
{
	struct fs_struct *fs_copy;

	if (vxi->vx_namespace)
		return -EPERM;
	if (!ns || !fs)
		return -EINVAL;

	fs_copy = copy_fs_struct(fs);
	if (!fs_copy)
		return -ENOMEM;

	get_namespace(ns);
	vxi->vx_namespace = ns;
	vxi->vx_fs = fs_copy;
	return 0;
}

int vc_enter_namespace(struct vx_info *vxi, void __user *data)
{
	return vx_enter_namespace(vxi);
}

int vc_set_namespace(struct vx_info *vxi, void __user *data)
{
	struct fs_struct *fs;
	struct namespace *ns;
	int ret;

	task_lock(current);
	fs = current->fs;
	atomic_inc(&fs->count);
	ns = current->namespace;
	get_namespace(current->namespace);
	task_unlock(current);

	ret = vx_set_namespace(vxi, ns, fs);

	put_namespace(ns);
	put_fs_struct(fs);
	return ret;
}

