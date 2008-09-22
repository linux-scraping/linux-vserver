#ifndef _VS_PID_H
#define _VS_PID_H

#include "vserver/base.h"
#include "vserver/check.h"
#include "vserver/context.h"
#include "vserver/debug.h"
#include "vserver/pid.h"
#include <linux/pid_namespace.h>


#define VXF_FAKE_INIT	(VXF_INFO_INIT | VXF_STATE_INIT)

static inline
int vx_proc_task_visible(struct task_struct *task)
{
	if ((task->pid == 1) &&
		!vx_flags(VXF_FAKE_INIT, VXF_FAKE_INIT))
		/* show a blend through init */
		goto visible;
	if (vx_check(vx_task_xid(task), VS_WATCH | VS_IDENT))
		goto visible;
	return 0;
visible:
	return 1;
}

#define find_task_by_real_pid(pid) find_task_by_pid_ns(pid, &init_pid_ns)

#if 0

static inline
struct task_struct *vx_find_proc_task_by_pid(int pid)
{
	struct task_struct *task = find_task_by_real_pid(pid);

	if (task && !vx_proc_task_visible(task)) {
		vxdprintk(VXD_CBIT(misc, 6),
			"dropping task (find) %p[#%u,%u] for %p[#%u,%u]",
			task, task->xid, task->pid,
			current, current->xid, current->pid);
		task = NULL;
	}
	return task;
}

#endif

static inline
struct task_struct *vx_get_proc_task(struct inode *inode, struct pid *pid)
{
	struct task_struct *task = get_pid_task(pid, PIDTYPE_PID);

	if (task && !vx_proc_task_visible(task)) {
		vxdprintk(VXD_CBIT(misc, 6),
			"dropping task (get) %p[#%u,%u] for %p[#%u,%u]",
			task, task->xid, task->pid,
			current, current->xid, current->pid);
		put_task_struct(task);
		task = NULL;
	}
	return task;
}

#if 0

static inline
struct task_struct *vx_child_reaper(struct task_struct *p)
{
	struct vx_info *vxi = p->vx_info;
	struct task_struct *reaper = child_reaper(p);

	if (!vxi)
		goto out;

	BUG_ON(!p->vx_info->vx_reaper);

	/* child reaper for the guest reaper */
	if (vxi->vx_reaper == p)
		goto out;

	reaper = vxi->vx_reaper;
out:
	vxdprintk(VXD_CBIT(xid, 7),
		"vx_child_reaper(%p[#%u,%u]) = %p[#%u,%u]",
		p, p->xid, p->pid, reaper, reaper->xid, reaper->pid);
	return reaper;
}

#endif


#else
#warning duplicate inclusion
#endif
