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


#else
#warning duplicate inclusion
#endif
