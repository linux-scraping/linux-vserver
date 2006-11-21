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
