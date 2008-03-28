/*
 *  linux/kernel/vserver/space.c
 *
 *  Virtual Server: Context Space Support
 *
 *  Copyright (C) 2003-2007  Herbert Pötzl
 *
 *  V0.01  broken out from context.c 0.07
 *  V0.02  added task locking for namespace
 *  V0.03  broken out vx_enter_namespace
 *  V0.04  added *space support and commands
 *
 */

#include <linux/utsname.h>
#include <linux/nsproxy.h>
#include <linux/err.h>
#include <asm/uaccess.h>

#include <linux/vs_context.h>
#include <linux/vserver/space.h>
#include <linux/vserver/space_cmd.h>

atomic_t vs_global_nsproxy	= ATOMIC_INIT(0);
atomic_t vs_global_fs		= ATOMIC_INIT(0);
atomic_t vs_global_mnt_ns	= ATOMIC_INIT(0);
atomic_t vs_global_uts_ns	= ATOMIC_INIT(0);
atomic_t vs_global_ipc_ns	= ATOMIC_INIT(0);
atomic_t vs_global_user_ns	= ATOMIC_INIT(0);
atomic_t vs_global_pid_ns	= ATOMIC_INIT(0);


/* namespace functions */

#include <linux/mnt_namespace.h>
#include <linux/user_namespace.h>
#include <linux/pid_namespace.h>

const struct vcmd_space_mask space_mask = {
	.mask = CLONE_NEWNS |
		CLONE_NEWUTS |
		CLONE_NEWIPC |
		CLONE_NEWUSER |
		CLONE_FS
};


/*
 *	build a new nsproxy mix
 *      assumes that both proxies are 'const'
 *	does not touch nsproxy refcounts
 *	will hold a reference on the result.
 */

struct nsproxy *vs_mix_nsproxy(struct nsproxy *old_nsproxy,
	struct nsproxy *new_nsproxy, unsigned long mask)
{
	struct mnt_namespace *old_ns;
	struct uts_namespace *old_uts;
	struct ipc_namespace *old_ipc;
	struct pid_namespace *old_pid;
	struct user_namespace *old_user;
	struct nsproxy *nsproxy;

	nsproxy = copy_nsproxy(old_nsproxy);
	if (!nsproxy)
		goto out;

	if (mask & CLONE_NEWNS) {
		old_ns = nsproxy->mnt_ns;
		nsproxy->mnt_ns = new_nsproxy->mnt_ns;
		if (nsproxy->mnt_ns)
			get_mnt_ns(nsproxy->mnt_ns);
	} else
		old_ns = NULL;

	if (mask & CLONE_NEWUTS) {
		old_uts = nsproxy->uts_ns;
		nsproxy->uts_ns = new_nsproxy->uts_ns;
		if (nsproxy->uts_ns)
			get_uts_ns(nsproxy->uts_ns);
	} else
		old_uts = NULL;

	if (mask & CLONE_NEWIPC) {
		old_ipc = nsproxy->ipc_ns;
		nsproxy->ipc_ns = new_nsproxy->ipc_ns;
		if (nsproxy->ipc_ns)
			get_ipc_ns(nsproxy->ipc_ns);
	} else
		old_ipc = NULL;

	if (mask & CLONE_NEWUSER) {
		old_user = nsproxy->user_ns;
		nsproxy->user_ns = new_nsproxy->user_ns;
		if (nsproxy->user_ns)
			get_user_ns(nsproxy->user_ns);
	} else
		old_user = NULL;

	if (mask & CLONE_NEWPID) {
		old_pid = nsproxy->pid_ns;
		nsproxy->pid_ns = new_nsproxy->pid_ns;
		if (nsproxy->pid_ns)
			get_pid_ns(nsproxy->pid_ns);
	} else
		old_pid = NULL;

	if (old_ns)
		put_mnt_ns(old_ns);
	if (old_uts)
		put_uts_ns(old_uts);
	if (old_ipc)
		put_ipc_ns(old_ipc);
	if (old_pid)
		put_pid_ns(old_pid);
	if (old_user)
		put_user_ns(old_user);
out:
	return nsproxy;
}


/*
 *	merge two nsproxy structs into a new one.
 *	will hold a reference on the result.
 */

static inline
struct nsproxy *__vs_merge_nsproxy(struct nsproxy *old,
	struct nsproxy *proxy, unsigned long mask)
{
	struct nsproxy null_proxy = { .mnt_ns = NULL };

	if (!proxy)
		return NULL;

	if (mask) {
		/* vs_mix_nsproxy returns with reference */
		return vs_mix_nsproxy(old ? old : &null_proxy,
			proxy, mask);
	}
	get_nsproxy(proxy);
	return proxy;
}

/*
 *	merge two fs structs into a new one.
 *	will take a reference on the result.
 */

static inline
struct fs_struct *__vs_merge_fs(struct fs_struct *old,
	struct fs_struct *fs, unsigned long mask)
{
	if (!(mask & CLONE_FS)) {
		if (old)
			atomic_inc(&old->count);
		return old;
	}

	if (!fs)
		return NULL;

	return copy_fs_struct(fs);
}


int vx_enter_space(struct vx_info *vxi, unsigned long mask)
{
	struct nsproxy *proxy, *proxy_cur, *proxy_new;
	struct fs_struct *fs, *fs_cur, *fs_new;
	int ret;

	if (vx_info_flags(vxi, VXF_INFO_PRIVATE, 0))
		return -EACCES;

	if (!mask)
		mask = vxi->vx_nsmask;

	if ((mask & vxi->vx_nsmask) != mask)
		return -EINVAL;

	proxy = vxi->vx_nsproxy;
	fs = vxi->vx_fs;

	task_lock(current);
	fs_cur = current->fs;
	atomic_inc(&fs_cur->count);
	proxy_cur = current->nsproxy;
	get_nsproxy(proxy_cur);
	task_unlock(current);

	fs_new = __vs_merge_fs(fs_cur, fs, mask);
	if (IS_ERR(fs_new)) {
		ret = PTR_ERR(fs_new);
		goto out_put;
	}

	proxy_new = __vs_merge_nsproxy(proxy_cur, proxy, mask);
	if (IS_ERR(proxy_new)) {
		ret = PTR_ERR(proxy_new);
		goto out_put_fs;
	}

	fs_new = xchg(&current->fs, fs_new);
	proxy_new = xchg(&current->nsproxy, proxy_new);
	ret = 0;

	if (proxy_new)
		put_nsproxy(proxy_new);
out_put_fs:
	if (fs_new)
		put_fs_struct(fs_new);
out_put:
	if (proxy_cur)
		put_nsproxy(proxy_cur);
	if (fs_cur)
		put_fs_struct(fs_cur);
	return ret;
}


int vx_set_space(struct vx_info *vxi, unsigned long mask)
{
	struct nsproxy *proxy_vxi, *proxy_cur, *proxy_new;
	struct fs_struct *fs_vxi, *fs_cur, *fs_new;
	int ret;

	if (!mask)
		mask = space_mask.mask;

	if ((mask & space_mask.mask) != mask)
		return -EINVAL;

	proxy_vxi = vxi->vx_nsproxy;
	fs_vxi = vxi->vx_fs;

	task_lock(current);
	fs_cur = current->fs;
	atomic_inc(&fs_cur->count);
	proxy_cur = current->nsproxy;
	get_nsproxy(proxy_cur);
	task_unlock(current);

	fs_new = __vs_merge_fs(fs_vxi, fs_cur, mask);
	if (IS_ERR(fs_new)) {
		ret = PTR_ERR(fs_new);
		goto out_put;
	}

	proxy_new = __vs_merge_nsproxy(proxy_vxi, proxy_cur, mask);
	if (IS_ERR(proxy_new)) {
		ret = PTR_ERR(proxy_new);
		goto out_put_fs;
	}

	fs_new = xchg(&vxi->vx_fs, fs_new);
	proxy_new = xchg(&vxi->vx_nsproxy, proxy_new);
	vxi->vx_nsmask |= mask;
	ret = 0;

	if (proxy_new)
		put_nsproxy(proxy_new);
out_put_fs:
	if (fs_new)
		put_fs_struct(fs_new);
out_put:
	if (proxy_cur)
		put_nsproxy(proxy_cur);
	if (fs_cur)
		put_fs_struct(fs_cur);
	return ret;
}


int vc_enter_space(struct vx_info *vxi, void __user *data)
{
	struct vcmd_space_mask vc_data = { .mask = 0 };

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return vx_enter_space(vxi, vc_data.mask);
}

int vc_set_space(struct vx_info *vxi, void __user *data)
{
	struct vcmd_space_mask vc_data = { .mask = 0 };

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return vx_set_space(vxi, vc_data.mask);
}

int vc_get_space_mask(struct vx_info *vxi, void __user *data)
{
	if (copy_to_user(data, &space_mask, sizeof(space_mask)))
		return -EFAULT;
	return 0;
}

