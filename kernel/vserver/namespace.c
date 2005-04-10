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

#include <linux/config.h>
#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/vs_context.h>
#include <linux/vserver/namespace.h>
#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/fs.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


/* virtual host info names */

static char * vx_vhi_name(struct vx_info *vxi, int id)
{
	switch (id) {
		case VHIN_CONTEXT:
			return vxi->vx_name;
		case VHIN_SYSNAME:
			return vxi->cvirt.utsname.sysname;
		case VHIN_NODENAME:
			return vxi->cvirt.utsname.nodename;
		case VHIN_RELEASE:
			return vxi->cvirt.utsname.release;
		case VHIN_VERSION:
			return vxi->cvirt.utsname.version;
		case VHIN_MACHINE:
			return vxi->cvirt.utsname.machine;
		case VHIN_DOMAINNAME:
			return vxi->cvirt.utsname.domainname;
		default:
			return NULL;
	}
	return NULL;
}

int vc_set_vhi_name(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_vx_vhi_name_v0 vc_data;
	char *name;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi = locate_vx_info(id);
	if (!vxi)
		return -ESRCH;

	name = vx_vhi_name(vxi, vc_data.field);
	if (name)
		memcpy(name, vc_data.name, 65);
	put_vx_info(vxi);
	return (name ? 0 : -EFAULT);
}

int vc_get_vhi_name(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_vx_vhi_name_v0 vc_data;
	char *name;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi = locate_vx_info(id);
	if (!vxi)
		return -ESRCH;

	name = vx_vhi_name(vxi, vc_data.field);
	if (!name)
		goto out_put;

	memcpy(vc_data.name, name, 65);
	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
out_put:
	put_vx_info(vxi);
	return (name ? 0 : -EFAULT);
}

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

int vc_enter_namespace(uint32_t id, void *data)
{
	struct vx_info *vxi;
	struct fs_struct *old_fs, *fs;
	struct namespace *old_ns;
	int ret = 0;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;

	vxi = locate_vx_info(id);
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

int vc_cleanup_namespace(uint32_t id, void *data)
{
	down_write(&current->namespace->sem);
	spin_lock(&vfsmount_lock);
	umount_unused(current->namespace->root, current->fs);
	spin_unlock(&vfsmount_lock);
	up_write(&current->namespace->sem);
	return 0;
}

int vc_set_namespace(uint32_t id, void __user *data)
{
	struct fs_struct *fs;
	struct namespace *ns;
	struct vx_info *vxi;
	int ret;

	if (vx_check(0, VX_ADMIN|VX_WATCH))
		return -ENOSYS;

	task_lock(current);
	vxi = get_vx_info(current->vx_info);
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

