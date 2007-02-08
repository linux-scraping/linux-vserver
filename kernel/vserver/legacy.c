/*
 *  linux/kernel/vserver/legacy.c
 *
 *  Virtual Server: Legacy Funtions
 *
 *  Copyright (C) 2001-2003  Jacques Gelinas
 *  Copyright (C) 2003-2006  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext.c V0.05
 *  V0.02  updated to spaces *sigh*
 *
 */

#include <linux/sched.h>
#include <linux/vs_context.h>
#include <linux/vs_network.h>
#include <linux/vserver/legacy.h>
#include <linux/vserver/space.h>
// #include <linux/mnt_namespace.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


extern int vx_set_init(struct vx_info *, struct task_struct *);

static int vx_set_initpid(struct vx_info *vxi, int pid)
{
	struct task_struct *init;

	init = find_task_by_real_pid(pid);
	if (!init)
		return -ESRCH;
	return vx_set_init(vxi, init);
}

int vc_new_s_context(uint32_t ctx, void __user *data)
{
	int ret = -ENOMEM;
	struct vcmd_new_s_context_v1 vc_data;
	struct vx_info *new_vxi;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	/* legacy hack, will be removed soon */
	if (ctx == -2) {
		/* assign flags and initpid */
		if (!current->vx_info)
			return -EINVAL;
		ret = 0;
		if (vc_data.flags & VX_INFO_INIT)
			ret = vx_set_initpid(current->vx_info, current->tgid);
		if (ret == 0) {
			/* We keep the same vx_id, but lower the capabilities */
			current->vx_info->vx_bcaps &= (~vc_data.remove_cap);
			ret = vx_current_xid();
			current->vx_info->vx_flags |= vc_data.flags;
		}
		return ret;
	}

	if (!vx_check(0, VS_ADMIN) || !capable(CAP_SYS_ADMIN)
		/* might make sense in the future, or not ... */
		|| vx_flags(VX_INFO_PRIVATE, 0))
		return -EPERM;

	/* ugly hack for Spectator */
	if (ctx == 1) {
		current->xid = 1;
		return 0;
	}

	if (((ctx > MAX_S_CONTEXT) && (ctx != VX_DYNAMIC_ID)) ||
		(ctx == 0))
		return -EINVAL;

	if ((ctx == VX_DYNAMIC_ID) || (ctx < MIN_D_CONTEXT))
		new_vxi = lookup_or_create_vx_info(ctx);
	else
		new_vxi = lookup_vx_info(ctx);

	if (!new_vxi)
		return -EINVAL;

	ret = -EPERM;
	if (!vx_info_flags(new_vxi, VXF_STATE_SETUP, 0) &&
		vx_info_flags(new_vxi, VX_INFO_PRIVATE, 0))
		goto out_put;

	ret = vx_migrate_task(current, new_vxi,
		vx_info_flags(new_vxi, VXF_STATE_SETUP, 0));
	new_vxi->vx_flags &= ~VXF_STATE_SETUP;

	if (ret == 0) {
		current->vx_info->vx_bcaps &= (~vc_data.remove_cap);
		new_vxi->vx_flags |= vc_data.flags;
		if (vc_data.flags & VX_INFO_INIT)
			vx_set_initpid(new_vxi, current->tgid);
		/* FIXME: nsproxy
		if (vc_data.flags & VX_INFO_NAMESPACE)
			vx_set_namespace(new_vxi,
				current->namespace, current->fs); */
		if (vc_data.flags & VX_INFO_NPROC)
			__rlim_set(&new_vxi->limit, RLIMIT_NPROC,
				current->signal->rlim[RLIMIT_NPROC].rlim_max);

		/* tweak some defaults for legacy */
		new_vxi->vx_flags |= (VXF_HIDE_NETIF|VXF_INFO_INIT);
		ret = new_vxi->vx_id;
	}
out_put:
	put_vx_info(new_vxi);
	return ret;
}

