/*
 *  linux/kernel/vserver/signal.c
 *
 *  Virtual Server: Signal Support
 *
 *  Copyright (C) 2003-2006  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext V0.05
 *  V0.02  changed vcmds to vxi arg
 *
 */

#include <linux/sched.h>

#include <asm/errno.h>
#include <asm/uaccess.h>

#include <linux/vs_context.h>
#include <linux/vserver/signal_cmd.h>


int vx_info_kill(struct vx_info *vxi, int pid, int sig)
{
	int retval, count=0;
	struct task_struct *p;
	unsigned long priv = 0;

	retval = -ESRCH;
	vxdprintk(VXD_CBIT(misc, 4),
		"vx_info_kill(%p[#%d],%d,%d)*",
		vxi, vxi->vx_id, pid, sig);
	read_lock(&tasklist_lock);
	switch (pid) {
	case  0:
		priv = 1;
	case -1:
		for_each_process(p) {
			int err = 0;

			if (vx_task_xid(p) != vxi->vx_id || p->pid <= 1 ||
				(pid && vxi->vx_initpid == p->pid))
				continue;

			err = group_send_sig_info(sig, (void*)priv, p);
			++count;
			if (err != -EPERM)
				retval = err;
		}
		break;

	case 1:
		if (vxi->vx_initpid) {
			pid = vxi->vx_initpid;
			printk("··· tasks left: %d\n", atomic_read(&vxi->vx_tasks));
			/* for now, only SIGINT to private init ... */
			if (!vx_info_flags(vxi, VXF_STATE_ADMIN, 0) &&
				/* ... as long as there are tasks left */
				(atomic_read(&vxi->vx_tasks) > 1))
				sig = SIGINT;
			priv = 1;
		}
		/* fallthrough */
	default:
		p = find_task_by_real_pid(pid);
		if (p) {
			if (vx_task_xid(p) == vxi->vx_id)
				retval = group_send_sig_info(sig,
					(void*)priv, p);
		}
		break;
	}
	read_unlock(&tasklist_lock);
	vxdprintk(VXD_CBIT(misc, 4),
		"vx_info_kill(%p[#%d],%d,%d) = %d",
		vxi, vxi->vx_id, pid, sig, retval);
	return retval;
}

int vc_ctx_kill(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_kill_v0 vc_data;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	/* special check to allow guest shutdown */
	if (!vx_info_flags(vxi, VXF_STATE_ADMIN, 0) &&
		/* forbid killall pid=0 when init is present */
		(((vc_data.pid < 1) && vxi->vx_initpid) ||
		(vc_data.pid > 1)))
		return -EACCES;

	return vx_info_kill(vxi, vc_data.pid, vc_data.sig);
}


static int __wait_exit(struct vx_info *vxi)
{
	DECLARE_WAITQUEUE(wait, current);
	int ret = 0;

	add_wait_queue(&vxi->vx_wait, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

wait:
	if (vx_info_state(vxi,
		VXS_SHUTDOWN|VXS_HASHED|VXS_HELPER) == VXS_SHUTDOWN)
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



int vc_wait_exit(struct vx_info *vxi, void __user *data)
{
	struct vcmd_wait_exit_v0 vc_data;
	int ret;

	ret = __wait_exit(vxi);
	vc_data.reboot_cmd = vxi->reboot_cmd;
	vc_data.exit_code = vxi->exit_code;

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		ret = -EFAULT;
	return ret;
}

