/*
 *  linux/kernel/vserver/namespace.c
 *
 *  Virtual Server: Context Namespace Support
 *
 *  Copyright (C) 2003-2005  Herbert Pötzl
 *
 *  V0.01  broken out from context.c 0.07
 *  V0.02  added task locking for namespace
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

int vc_enter_namespace(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct fs_struct *old_fs, *fs;
	struct namespace *old_ns;
	int ret = 0;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;

	vxi = lookup_vx_info(id);
	if (!vxi)
		return -ESRCH;

	ret = -EINVAL;
	if (!vxi->vx_namespace)
		goto out_put;

	ret = -ENOMEM;
	fs = copy_fs_struct(vxi->vx_fs);
	if (!fs)
		goto out_put;

	ret = 0;
	task_lock(current);
	old_ns = current->namespace;
	old_fs = current->fs;
	get_namespace(vxi->vx_namespace);
	current->namespace = vxi->vx_namespace;
	current->fs = fs;
	task_unlock(current);

	put_namespace(old_ns);
	put_fs_struct(old_fs);
out_put:
	put_vx_info(vxi);
	return ret;
}

int vc_set_namespace(uint32_t id, void __user *data)
{
	struct fs_struct *fs;
	struct namespace *ns;
	struct vx_info *vxi;
	int ret;

	vxi = lookup_vx_info(id);
	if (!vxi)
		return -ESRCH;

	task_lock(current);
	fs = current->fs;
	atomic_inc(&fs->count);
	ns = current->namespace;
	get_namespace(current->namespace);
	task_unlock(current);

	ret = vx_set_namespace(vxi, ns, fs);

	put_namespace(ns);
	put_fs_struct(fs);
	put_vx_info(vxi);
	return ret;
}

