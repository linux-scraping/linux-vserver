/*
 *  linux/kernel/vserver/space.c
 *
 *  Virtual Server: Context Space Support
 *
 *  Copyright (C) 2003-2006  Herbert Pötzl
 *
 *  V0.01  broken out from context.c 0.07
 *  V0.02  added task locking for namespace
 *  V0.03  broken out vx_enter_namespace
 *  V0.04  added *space support and commands
 *
 */

#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/vs_context.h>
#include <linux/vserver/space.h>
#include <linux/vserver/space_cmd.h>
#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/fs.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


/* namespace functions */

#include <linux/namespace.h>

const struct vcmd_space_mask space_mask = {
	.mask = CLONE_NEWNS |
		CLONE_NEWUTS |
		CLONE_NEWIPC |
		CLONE_FS
};


/*
 *	build a new nsproxy mix
 *      assumes that both proxies are 'const'
 *	does not touch nsproxy refcounts
 */

struct nsproxy *vs_mix_nsproxy(struct nsproxy *old_nsproxy,
	struct nsproxy *new_nsproxy, unsigned long mask)
{
	struct namespace *old_ns;
	struct uts_namespace *old_uts;
	struct ipc_namespace *old_ipc;
	struct nsproxy *nsproxy;

	old_ns = old_nsproxy->namespace;
	old_uts = old_nsproxy->uts_ns;
	old_ipc = old_nsproxy->ipc_ns;

	nsproxy = dup_namespaces(old_nsproxy);
	if (!nsproxy)
		goto out;

	if (mask & CLONE_NEWNS) {
		nsproxy->namespace = new_nsproxy->namespace;
		if (nsproxy->namespace)
			get_namespace(nsproxy->namespace);
	} else
		old_ns = NULL;

	if (mask & CLONE_NEWUTS) {
		nsproxy->uts_ns = new_nsproxy->uts_ns;
		if (nsproxy->uts_ns)
			get_uts_ns(nsproxy->uts_ns);
	} else
		old_uts = NULL;

	if (mask & CLONE_NEWIPC) {
		nsproxy->ipc_ns = new_nsproxy->ipc_ns;
		if (nsproxy->ipc_ns)
			get_ipc_ns(nsproxy->ipc_ns);
	} else
		old_ipc = NULL;

	if (old_ns)
		put_namespace(old_ns);
	if (old_uts)
		put_uts_ns(old_uts);
	if (old_ipc)
		put_ipc_ns(old_ipc);
out:
	return nsproxy;
}

static inline
void __vs_merge_nsproxy(struct nsproxy **ptr,
	struct nsproxy *nsproxy, unsigned long mask)
{
	struct nsproxy *old = *ptr;
	struct nsproxy null_proxy = { .namespace = NULL };

	BUG_ON(!nsproxy);

	if (mask)
		*ptr = vs_mix_nsproxy(old ? old : &null_proxy,
			nsproxy, mask);
	else {
		*ptr = nsproxy;
		get_nsproxy(nsproxy);
	}
	if (old)
		put_nsproxy(old);
}

static inline
void __vs_merge_fs(struct fs_struct **ptr, struct fs_struct *fs)
{
	struct fs_struct *old = *ptr;

	*ptr = fs;
	atomic_inc(&fs->count);
	if (old)
		put_fs_struct(old);
}


int vx_enter_space(struct vx_info *vxi, unsigned long mask)
{
	struct fs_struct *fs = NULL;
	struct nsproxy *nsproxy;

	if (vx_info_flags(vxi, VXF_INFO_PRIVATE, 0))
		return -EACCES;

	if (!mask)
		mask = vxi->vx_nsmask;

	if ((mask & vxi->vx_nsmask) != mask)
		return -EINVAL;

	nsproxy = vxi->vx_nsproxy;
	if ((mask & CLONE_FS)) {
		BUG_ON(!vxi->vx_fs);
		fs = copy_fs_struct(vxi->vx_fs);
		if (!fs)
			return -ENOMEM;
	}

	task_lock(current);
	if (nsproxy)
		__vs_merge_nsproxy(&current->nsproxy, nsproxy, mask);
	if (fs)
		__vs_merge_fs(&current->fs, fs);
	task_unlock(current);
	return 0;
}


int vx_set_space(struct vx_info *vxi, unsigned long mask)
{
	struct fs_struct *fs, *fs_copy = NULL;
	struct nsproxy *nsproxy;
	int ret;

	if (!mask)
		mask = space_mask.mask;

	if ((mask & space_mask.mask) != mask)
		return -EINVAL;

	task_lock(current);
	fs = current->fs;
	atomic_inc(&fs->count);
	nsproxy = current->nsproxy;
	get_nsproxy(nsproxy);
	task_unlock(current);

	ret = -ENOMEM;
	if ((mask & CLONE_FS)) {
		fs_copy = copy_fs_struct(fs);
		if (!fs_copy)
			goto out_put;
	}

	if (nsproxy)
		__vs_merge_nsproxy(&vxi->vx_nsproxy, nsproxy, mask);
	if (fs_copy)
		__vs_merge_fs(&vxi->vx_fs, fs_copy);
	vxi->vx_nsmask |= mask;

	ret = 0;
out_put:
	put_fs_struct(fs);
	put_nsproxy(nsproxy);
	return ret;
}


int vc_enter_space(struct vx_info *vxi, void __user *data)
{
	struct vcmd_space_mask vc_data = { .mask = ~0 };

	if (data && copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return vx_enter_space(vxi, vc_data.mask);
}

int vc_set_space(struct vx_info *vxi, void __user *data)
{
	struct vcmd_space_mask vc_data = { .mask = 0 };

	if (data && copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return vx_set_space(vxi, vc_data.mask);
}

int vc_get_space_mask(struct vx_info *vxi, void __user *data)
{
	if (copy_to_user(data, &space_mask, sizeof(space_mask)))
		return -EFAULT;
	return 0;
}

