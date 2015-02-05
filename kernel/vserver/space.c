/*
 *  linux/kernel/vserver/space.c
 *
 *  Virtual Server: Context Space Support
 *
 *  Copyright (C) 2003-2010  Herbert Pötzl
 *
 *  V0.01  broken out from context.c 0.07
 *  V0.02  added task locking for namespace
 *  V0.03  broken out vx_enter_namespace
 *  V0.04  added *space support and commands
 *  V0.05  added credential support
 *
 */

#include <linux/utsname.h>
#include <linux/nsproxy.h>
#include <linux/err.h>
#include <linux/fs_struct.h>
#include <linux/cred.h>
#include <asm/uaccess.h>

#include <linux/vs_context.h>
#include <linux/vserver/space.h>
#include <linux/vserver/space_cmd.h>

atomic_t vs_global_nsproxy	= ATOMIC_INIT(0);
atomic_t vs_global_fs		= ATOMIC_INIT(0);
atomic_t vs_global_mnt_ns	= ATOMIC_INIT(0);
atomic_t vs_global_uts_ns	= ATOMIC_INIT(0);
atomic_t vs_global_user_ns	= ATOMIC_INIT(0);
atomic_t vs_global_pid_ns	= ATOMIC_INIT(0);


/* namespace functions */

#include <linux/mnt_namespace.h>
#include <linux/user_namespace.h>
#include <linux/pid_namespace.h>
#include <linux/ipc_namespace.h>
#include <net/net_namespace.h>
#include "../fs/mount.h"


static const struct vcmd_space_mask_v1 space_mask_v0 = {
	.mask = CLONE_FS |
		CLONE_NEWNS |
#ifdef	CONFIG_UTS_NS
		CLONE_NEWUTS |
#endif
#ifdef	CONFIG_IPC_NS
		CLONE_NEWIPC |
#endif
#ifdef	CONFIG_USER_NS
		CLONE_NEWUSER |
#endif
		0
};

static const struct vcmd_space_mask_v1 space_mask = {
	.mask = CLONE_FS |
		CLONE_NEWNS |
#ifdef	CONFIG_UTS_NS
		CLONE_NEWUTS |
#endif
#ifdef	CONFIG_IPC_NS
		CLONE_NEWIPC |
#endif
#ifdef	CONFIG_USER_NS
		CLONE_NEWUSER |
#endif
#ifdef	CONFIG_PID_NS
		CLONE_NEWPID |
#endif
#ifdef	CONFIG_NET_NS
		CLONE_NEWNET |
#endif
		0
};

static const struct vcmd_space_mask_v1 default_space_mask = {
	.mask = CLONE_FS |
		CLONE_NEWNS |
#ifdef	CONFIG_UTS_NS
		CLONE_NEWUTS |
#endif
#ifdef	CONFIG_IPC_NS
		CLONE_NEWIPC |
#endif
#ifdef	CONFIG_USER_NS
//		CLONE_NEWUSER |
#endif
#ifdef	CONFIG_PID_NS
//		CLONE_NEWPID |
#endif
		0
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
#ifdef	CONFIG_PID_NS
	struct pid_namespace *old_pid;
#endif
#ifdef	CONFIG_NET_NS
	struct net *old_net;
#endif
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

#ifdef	CONFIG_PID_NS
	if (mask & CLONE_NEWPID) {
		old_pid = nsproxy->pid_ns_for_children;
		nsproxy->pid_ns_for_children = new_nsproxy->pid_ns_for_children;
		if (nsproxy->pid_ns_for_children)
			get_pid_ns(nsproxy->pid_ns_for_children);
	} else
		old_pid = NULL;
#endif
#ifdef	CONFIG_NET_NS
	if (mask & CLONE_NEWNET) {
		old_net = nsproxy->net_ns;
		nsproxy->net_ns = new_nsproxy->net_ns;
		if (nsproxy->net_ns)
			get_net(nsproxy->net_ns);
	} else
		old_net = NULL;
#endif
	if (old_ns)
		put_mnt_ns(old_ns);
	if (old_uts)
		put_uts_ns(old_uts);
	if (old_ipc)
		put_ipc_ns(old_ipc);
#ifdef	CONFIG_PID_NS
	if (old_pid)
		put_pid_ns(old_pid);
#endif
#ifdef	CONFIG_NET_NS
	if (old_net)
		put_net(old_net);
#endif
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


int vx_enter_space(struct vx_info *vxi, unsigned long mask, unsigned index)
{
	struct nsproxy *proxy, *proxy_cur, *proxy_new;
	struct fs_struct *fs_cur, *fs = NULL;
	struct _vx_space *space;
	int ret, kill = 0;

	vxdprintk(VXD_CBIT(space, 8), "vx_enter_space(%p[#%u],0x%08lx,%d)",
		vxi, vxi->vx_id, mask, index);

	if (vx_info_flags(vxi, VXF_INFO_PRIVATE, 0))
		return -EACCES;

	if (index >= VX_SPACES)
		return -EINVAL;

	space = &vxi->space[index];

	if (!mask)
		mask = space->vx_nsmask;

	if ((mask & space->vx_nsmask) != mask)
		return -EINVAL;

	if (mask & CLONE_FS) {
		fs = copy_fs_struct(space->vx_fs);
		if (!fs)
			return -ENOMEM;
	}
	proxy = space->vx_nsproxy;

	vxdprintk(VXD_CBIT(space, 9),
		"vx_enter_space(%p[#%u],0x%08lx,%d) -> (%p,%p)",
		vxi, vxi->vx_id, mask, index, proxy, fs);

	task_lock(current);
	fs_cur = current->fs;

	if (mask & CLONE_FS) {
		spin_lock(&fs_cur->lock);
		current->fs = fs;
		kill = !--fs_cur->users;
		spin_unlock(&fs_cur->lock);
	}

	proxy_cur = current->nsproxy;
	get_nsproxy(proxy_cur);
	task_unlock(current);

	if (kill)
		free_fs_struct(fs_cur);

	proxy_new = __vs_merge_nsproxy(proxy_cur, proxy, mask);
	if (IS_ERR(proxy_new)) {
		ret = PTR_ERR(proxy_new);
		goto out_put;
	}

	proxy_new = xchg(&current->nsproxy, proxy_new);

	if (mask & CLONE_NEWUSER) {
		struct cred *cred;

		vxdprintk(VXD_CBIT(space, 10),
			"vx_enter_space(%p[#%u],%p) cred (%p,%p)",
			vxi, vxi->vx_id, space->vx_cred,
			current->real_cred, current->cred);

		if (space->vx_cred) {
			cred = __prepare_creds(space->vx_cred);
			if (cred)
				commit_creds(cred);
		}
	}

	ret = 0;

	if (proxy_new)
		put_nsproxy(proxy_new);
out_put:
	if (proxy_cur)
		put_nsproxy(proxy_cur);
	return ret;
}


int vx_set_space(struct vx_info *vxi, unsigned long mask, unsigned index)
{
	struct nsproxy *proxy_vxi, *proxy_cur, *proxy_new;
	struct fs_struct *fs_vxi, *fs = NULL;
	struct _vx_space *space;
	int ret, kill = 0;

	vxdprintk(VXD_CBIT(space, 8), "vx_set_space(%p[#%u],0x%08lx,%d)",
		vxi, vxi->vx_id, mask, index);

	if ((mask & space_mask.mask) != mask)
		return -EINVAL;

	if (index >= VX_SPACES)
		return -EINVAL;

	space = &vxi->space[index];

	proxy_vxi = space->vx_nsproxy;
	fs_vxi = space->vx_fs;

	if (mask & CLONE_FS) {
		fs = copy_fs_struct(current->fs);
		if (!fs)
			return -ENOMEM;
	}

	task_lock(current);

	if (mask & CLONE_FS) {
		spin_lock(&fs_vxi->lock);
		space->vx_fs = fs;
		kill = !--fs_vxi->users;
		spin_unlock(&fs_vxi->lock);
	}

	proxy_cur = current->nsproxy;
	get_nsproxy(proxy_cur);
	task_unlock(current);

	if (kill)
		free_fs_struct(fs_vxi);

	proxy_new = __vs_merge_nsproxy(proxy_vxi, proxy_cur, mask);
	if (IS_ERR(proxy_new)) {
		ret = PTR_ERR(proxy_new);
		goto out_put;
	}

	proxy_new = xchg(&space->vx_nsproxy, proxy_new);
	space->vx_nsmask |= mask;

	if (mask & CLONE_NEWUSER) {
		struct cred *cred;

		vxdprintk(VXD_CBIT(space, 10),
			"vx_set_space(%p[#%u],%p) cred (%p,%p)",
			vxi, vxi->vx_id, space->vx_cred,
			current->real_cred, current->cred);

		cred = prepare_creds();
		cred = (struct cred *)xchg(&space->vx_cred, cred);
		if (cred)
			abort_creds(cred);
	}

	ret = 0;

	if (proxy_new)
		put_nsproxy(proxy_new);
out_put:
	if (proxy_cur)
		put_nsproxy(proxy_cur);
	return ret;
}


int vc_enter_space_v1(struct vx_info *vxi, void __user *data)
{
	struct vcmd_space_mask_v1 vc_data = { .mask = 0 };

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return vx_enter_space(vxi, vc_data.mask, 0);
}

int vc_enter_space(struct vx_info *vxi, void __user *data)
{
	struct vcmd_space_mask_v2 vc_data = { .mask = 0 };

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	if (vc_data.index >= VX_SPACES)
		return -EINVAL;

	return vx_enter_space(vxi, vc_data.mask, vc_data.index);
}

int vc_set_space_v1(struct vx_info *vxi, void __user *data)
{
	struct vcmd_space_mask_v1 vc_data = { .mask = 0 };

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return vx_set_space(vxi, vc_data.mask, 0);
}

int vc_set_space(struct vx_info *vxi, void __user *data)
{
	struct vcmd_space_mask_v2 vc_data = { .mask = 0 };

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	if (vc_data.index >= VX_SPACES)
		return -EINVAL;

	return vx_set_space(vxi, vc_data.mask, vc_data.index);
}

int vc_get_space_mask(void __user *data, int type)
{
	const struct vcmd_space_mask_v1 *mask;

	if (type == 0)
		mask = &space_mask_v0;
	else if (type == 1)
		mask = &space_mask;
	else
		mask = &default_space_mask;

	vxdprintk(VXD_CBIT(space, 10),
		"vc_get_space_mask(%d) = %08llx", type, mask->mask);

	if (copy_to_user(data, mask, sizeof(*mask)))
		return -EFAULT;
	return 0;
}

