#ifndef _VS_SCHED_H
#define _VS_SCHED_H

#include "vserver/base.h"
#include "vserver/context.h"
#include "vserver/sched.h"


#define MAX_PRIO_BIAS		 20
#define MIN_PRIO_BIAS		-20

static inline
int vx_adjust_prio(struct task_struct *p, int prio, int max_user)
{
	struct vx_info *vxi = p->vx_info;

	if (vxi)
		prio += vx_cpu(vxi, sched_pc).prio_bias;
	return prio;
}

static inline void vx_account_user(struct vx_info *vxi,
	cputime_t cputime, int nice)
{
	if (!vxi)
		return;
	vx_cpu(vxi, sched_pc).user_ticks += cputime;
}

static inline void vx_account_system(struct vx_info *vxi,
	cputime_t cputime, int idle)
{
	if (!vxi)
		return;
	vx_cpu(vxi, sched_pc).sys_ticks += cputime;
}

#else
#warning duplicate inclusion
#endif
