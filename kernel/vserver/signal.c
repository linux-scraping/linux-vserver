/*
 *  linux/kernel/vserver/signal.c
 *
 *  Virtual Server: Signal Support
 *
 *  Copyright (C) 2003-2005  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext V0.05
 *
 */

#include <linux/config.h>
#include <linux/sched.h>

#include <asm/errno.h>
#include <asm/uaccess.h>

#include <linux/vs_context.h>
#include <linux/vserver/signal_cmd.h>


int vc_ctx_kill(uint32_t id, void __user *data)
{
	int retval, count=0;
	struct vcmd_ctx_kill_v0 vc_data;
	struct task_struct *p;
	struct vx_info *vxi;
	unsigned long priv = 0;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi = locate_vx_info(id);
	if (!vxi)
		return -ESRCH;

	retval = -ESRCH;
	read_lock(&tasklist_lock);
	switch (vc_data.pid) {
	case  0:
		priv = 1;
	case -1:
		for_each_process(p) {
			int err = 0;

			if (vx_task_xid(p) != id || p->pid <= 1 ||
				(vc_data.pid && vxi->vx_initpid == p->pid))
				continue;

			err = group_send_sig_info(vc_data.sig, (void*)priv, p);
			++count;
			if (err != -EPERM)
				retval = err;
		}
		break;

	case 1:
		if (vxi->vx_initpid) {
			vc_data.pid = vxi->vx_initpid;
			priv = 1;
		}
		/* fallthrough */
	default:
		p = find_task_by_real_pid(vc_data.pid);
		if (p) {
			if ((id == -1) || (vx_task_xid(p) == id))
				retval = group_send_sig_info(vc_data.sig,
					(void*)priv, p);
		}
		break;
	}
	read_unlock(&tasklist_lock);
	put_vx_info(vxi);
	return retval;
}


static int __wait_exit(struct vx_info *vxi)
{
	DECLARE_WAITQUEUE(wait, current);
	int ret = 0;

	add_wait_queue(&vxi->vx_wait, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

wait:
	if (vx_info_state(vxi, VXS_SHUTDOWN|VXS_HASHED) == VXS_SHUTDOWN)
		goto out;
	if (signal_pending(current)) {
		ret = -ERESTARTSYS;
		goto out;
	}
	schedule();
	goto wait;

out:
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&vxi->vx_wait, &wait);
	return ret;
}



int vc_wait_exit(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	int ret;

	vxi = locate_vx_info(id);
	if (!vxi)
		return -ESRCH;

	ret = __wait_exit(vxi);
	put_vx_info(vxi);
	return ret;
}

