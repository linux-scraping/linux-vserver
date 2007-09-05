#ifndef _VS_PID_H
#define _VS_PID_H

#include "vserver/base.h"
#include "vserver/context.h"
#include "vserver/debug.h"
#include <linux/pid_namespace.h>


/* pid faking stuff */


#define vx_info_map_pid(v, p) \
	__vx_info_map_pid((v), (p), __FUNC__, __FILE__, __LINE__)
#define vx_info_map_tgid(v,p)  vx_info_map_pid(v,p)
#define vx_map_pid(p) vx_info_map_pid(current->vx_info, p)
#define vx_map_tgid(p) vx_map_pid(p)

static inline int __vx_info_map_pid(struct vx_info *vxi, int pid,
	const char *func, const char *file, int line)
{
	if (vx_info_flags(vxi, VXF_INFO_INIT, 0)) {
		vxfprintk(VXD_CBIT(cvirt, 2),
			"vx_map_tgid: %p/%llx: %d -> %d",
			vxi, (long long)vxi->vx_flags, pid,
			(pid && pid == vxi->vx_initpid) ? 1 : pid,
			func, file, line);
		if (pid == 0)
			return 0;
		if (pid == vxi->vx_initpid)
			return 1;
	}
	return pid;
}

#define vx_info_rmap_pid(v, p) \
	__vx_info_rmap_pid((v), (p), __FUNC__, __FILE__, __LINE__)
#define vx_rmap_pid(p) vx_info_rmap_pid(current->vx_info, p)
#define vx_rmap_tgid(p) vx_rmap_pid(p)

static inline int __vx_info_rmap_pid(struct vx_info *vxi, int pid,
	const char *func, const char *file, int line)
{
	if (vx_info_flags(vxi, VXF_INFO_INIT, 0)) {
		vxfprintk(VXD_CBIT(cvirt, 2),
			"vx_rmap_tgid: %p/%llx: %d -> %d",
			vxi, (long long)vxi->vx_flags, pid,
			(pid == 1) ? vxi->vx_initpid : pid,
			func, file, line);
		if ((pid == 1) && vxi->vx_initpid)
			return vxi->vx_initpid;
		if (pid == vxi->vx_initpid)
			return ~0U;
	}
	return pid;
}


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

static inline
struct task_struct *vx_find_proc_task_by_pid(int pid)
{
	struct task_struct *task = find_task_by_pid(pid);

	if (task && !vx_proc_task_visible(task)) {
		vxdprintk(VXD_CBIT(misc, 6),
			"dropping task (find) %p[#%u,%u] for %p[#%u,%u]",
			task, task->xid, task->pid,
			current, current->xid, current->pid);
		task = NULL;
	}
	return task;
}

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


#else
#warning duplicate inclusion
#endif
