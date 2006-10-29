#ifndef _VX_VS_CVIRT_H
#define _VX_VS_CVIRT_H

#include "vserver/cvirt.h"
#include "vserver/debug.h"


/* utsname virtualization */

static inline struct new_utsname *vx_new_utsname(void)
{
	if (current->vx_info)
		return &current->vx_info->cvirt.utsname;
	return &system_utsname;
}

#define vx_new_uts(x)		((vx_new_utsname())->x)


/* pid faking stuff */


#define vx_info_map_pid(v,p) \
	__vx_info_map_pid((v), (p), __FUNC__, __FILE__, __LINE__)
#define vx_info_map_tgid(v,p)  vx_info_map_pid(v,p)
#define vx_map_pid(p)	vx_info_map_pid(current->vx_info, p)
#define vx_map_tgid(p) vx_map_pid(p)

static inline int __vx_info_map_pid(struct vx_info *vxi, int pid,
	const char *func, const char *file, int line)
{
	if (vx_info_flags(vxi, VXF_INFO_INIT, 0)) {
		vxfprintk(VXD_CBIT(cvirt, 2),
			"vx_map_tgid: %p/%llx: %d -> %d",
			vxi, (long long)vxi->vx_flags, pid,
			(pid && pid == vxi->vx_initpid)?1:pid,
			func, file, line);
		if (pid == 0)
			return 0;
		if (pid == vxi->vx_initpid)
			return 1;
	}
	return pid;
}

#define vx_info_rmap_pid(v,p) \
	__vx_info_rmap_pid((v), (p), __FUNC__, __FILE__, __LINE__)
#define vx_rmap_pid(p)	vx_info_rmap_pid(current->vx_info, p)
#define vx_rmap_tgid(p) vx_rmap_pid(p)

static inline int __vx_info_rmap_pid(struct vx_info *vxi, int pid,
	const char *func, const char *file, int line)
{
	if (vx_info_flags(vxi, VXF_INFO_INIT, 0)) {
		vxfprintk(VXD_CBIT(cvirt, 2),
			"vx_rmap_tgid: %p/%llx: %d -> %d",
			vxi, (long long)vxi->vx_flags, pid,
			(pid == 1)?vxi->vx_initpid:pid,
			func, file, line);
		if ((pid == 1) && vxi->vx_initpid)
			return vxi->vx_initpid;
		if (pid == vxi->vx_initpid)
			return ~0U;
	}
	return pid;
}


static inline void vx_activate_task(struct task_struct *p)
{
	struct vx_info *vxi;

	if ((vxi = p->vx_info)) {
		vx_update_load(vxi);
		atomic_inc(&vxi->cvirt.nr_running);
	}
}

static inline void vx_deactivate_task(struct task_struct *p)
{
	struct vx_info *vxi;

	if ((vxi = p->vx_info)) {
		vx_update_load(vxi);
		atomic_dec(&vxi->cvirt.nr_running);
	}
}

static inline void vx_uninterruptible_inc(struct task_struct *p)
{
	struct vx_info *vxi;

	if ((vxi = p->vx_info))
		atomic_inc(&vxi->cvirt.nr_uninterruptible);
}

static inline void vx_uninterruptible_dec(struct task_struct *p)
{
	struct vx_info *vxi;

	if ((vxi = p->vx_info))
		atomic_dec(&vxi->cvirt.nr_uninterruptible);
}


struct inode;

#define VXF_FAKE_INIT	(VXF_INFO_INIT|VXF_STATE_INIT)

static inline
int proc_pid_visible(struct task_struct *task, int pid)
{
	if ((pid == 1) &&
		!vx_flags(VXF_FAKE_INIT, VXF_FAKE_INIT))
		/* show a blend through init */
		goto visible;
	if (vx_check(vx_task_xid(task), VX_WATCH|VX_IDENT))
		goto visible;
	return 0;
visible:
	return 1;
}

static inline
struct task_struct *find_proc_task_by_pid(int pid)
{
	struct task_struct *task = find_task_by_pid(pid);

	if (task && !proc_pid_visible(task, pid)) {
		vxdprintk(VXD_CBIT(misc, 6),
			"dropping task %p[#%u,%u] for %p[#%u,%u]",
			task, task->xid, task->pid,
			current, current->xid, current->pid);
		put_task_struct(task);
		task = NULL;
	}
	return task;
}

static inline
struct task_struct *vx_get_proc_task(struct inode *inode, struct pid *pid)
{
	struct task_struct *task = get_pid_task(pid, PIDTYPE_PID);

	if (task && !proc_pid_visible(task, pid->nr)) {
		vxdprintk(VXD_CBIT(misc, 6),
			"dropping task %p[#%u,%u] for %p[#%u,%u]",
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
