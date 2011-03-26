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


#else
#warning duplicate inclusion
#endif
